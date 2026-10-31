// YAOF Nightstand: clock that emulates a chosen NFC or RFID tag while open.
// Down opens a file browser (/ext) to pick the default tag; the choice is saved.
// The tag file is a personal secret, deployed from YAOF-config by path.
#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <input/input.h>
#include <datetime/datetime.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <toolbox/path.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/lfrfid_dict_file.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

#define CONFIG_DIR  "/ext/apps_data/yaof_nightstand"
#define CONFIG_FILE CONFIG_DIR "/target.txt"
#define DEFAULT_TAG "/ext/nfc/home.nfc"

typedef enum {
    EmuNone,
    EmuNfc,
    EmuRfid,
} EmuType;

typedef struct {
    FuriMessageQueue* input_queue;
    Storage* storage;
    Gui* gui;
    ViewPort* view_port;
    FuriString* target;
    EmuType emu;
    // NFC backend
    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcListener* nfc_listener;
    // RFID backend
    ProtocolDict* dict;
    LFRFIDWorker* lfworker;
} Nightstand;

static EmuType type_of(const char* path) {
    const char* dot = strrchr(path, '.');
    if(!dot) return EmuNone;
    if(strcmp(dot, ".nfc") == 0) return EmuNfc;
    if(strcmp(dot, ".rfid") == 0) return EmuRfid;
    return EmuNone;
}

static void emu_stop(Nightstand* app) {
    switch(app->emu) {
    case EmuNfc:
        if(app->nfc_listener) {
            nfc_listener_stop(app->nfc_listener);
            nfc_listener_free(app->nfc_listener);
            app->nfc_listener = NULL;
        }
        if(app->nfc_device) {
            nfc_device_free(app->nfc_device);
            app->nfc_device = NULL;
        }
        if(app->nfc) {
            nfc_free(app->nfc);
            app->nfc = NULL;
        }
        break;
    case EmuRfid:
        if(app->lfworker) {
            lfrfid_worker_stop(app->lfworker);
            lfrfid_worker_stop_thread(app->lfworker);
            lfrfid_worker_free(app->lfworker);
            app->lfworker = NULL;
        }
        if(app->dict) {
            protocol_dict_free(app->dict);
            app->dict = NULL;
        }
        break;
    default:
        break;
    }
    app->emu = EmuNone;
}

static void emu_start(Nightstand* app) {
    const char* path = furi_string_get_cstr(app->target);
    EmuType type = type_of(path);
    if(type == EmuNfc) {
        app->nfc = nfc_alloc();
        app->nfc_device = nfc_device_alloc();
        if(nfc_device_load(app->nfc_device, path)) {
            NfcProtocol protocol = nfc_device_get_protocol(app->nfc_device);
            app->nfc_listener =
                nfc_listener_alloc(app->nfc, protocol, nfc_device_get_data(app->nfc_device, protocol));
            nfc_listener_start(app->nfc_listener, NULL, NULL);
            app->emu = EmuNfc;
        } else {
            nfc_device_free(app->nfc_device);
            app->nfc_device = NULL;
            nfc_free(app->nfc);
            app->nfc = NULL;
        }
    } else if(type == EmuRfid) {
        app->dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
        ProtocolId id = lfrfid_dict_file_load(app->dict, path);
        if(id != PROTOCOL_NO) {
            app->lfworker = lfrfid_worker_alloc(app->dict);
            lfrfid_worker_start_thread(app->lfworker);
            lfrfid_worker_emulate_start(app->lfworker, (LFRFIDProtocol)id);
            app->emu = EmuRfid;
        } else {
            protocol_dict_free(app->dict);
            app->dict = NULL;
        }
    }
}

static void load_target(Nightstand* app) {
    furi_string_set(app->target, DEFAULT_TAG);
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, CONFIG_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[256];
        size_t n = storage_file_read(f, buf, sizeof(buf) - 1);
        if(n > 0) {
            buf[n] = '\0';
            // trim trailing newline/space
            while(n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) {
                buf[--n] = '\0';
            }
            if(n > 0) furi_string_set(app->target, buf);
        }
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void save_target(Nightstand* app) {
    storage_simply_mkdir(app->storage, CONFIG_DIR);
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, CONFIG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* p = furi_string_get_cstr(app->target);
        storage_file_write(f, p, strlen(p));
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void yaof_nightstand_draw_callback(Canvas* canvas, void* context) {
    Nightstand* app = context;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char time_str[16];
    char date_str[16];
    snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
    snprintf(date_str, sizeof(date_str), "%04u-%02u-%02u", dt.year, dt.month, dt.day);

    FuriString* base = furi_string_alloc();
    path_extract_filename(app->target, base, false);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, time_str);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, date_str);

    char status[64];
    const char* kind = app->emu == EmuNfc ? "NFC" : app->emu == EmuRfid ? "RFID" : "none";
    if(app->emu == EmuNone) {
        snprintf(status, sizeof(status), "not found: %s", furi_string_get_cstr(base));
    } else {
        snprintf(status, sizeof(status), "%s: %s", kind, furi_string_get_cstr(base));
    }
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, status);
    canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignCenter, "Down: pick tag");
    furi_string_free(base);
}

static void yaof_nightstand_input_callback(InputEvent* event, void* context) {
    Nightstand* app = context;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

static void pick_target(Nightstand* app) {
    emu_stop(app);
    gui_remove_view_port(app->gui, app->view_port);

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, "*", NULL);
    opts.base_path = "/ext";
    opts.hide_dot_files = true;

    FuriString* result = furi_string_alloc_set(app->target);
    if(dialog_file_browser_show(dialogs, result, app->target, &opts)) {
        if(type_of(furi_string_get_cstr(result)) != EmuNone) {
            furi_string_set(app->target, result);
            save_target(app);
        }
    }
    furi_string_free(result);
    furi_record_close(RECORD_DIALOGS);

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    emu_start(app);
}

int32_t yaof_nightstand_app(void* p) {
    UNUSED(p);

    Nightstand app = {0};
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.storage = furi_record_open(RECORD_STORAGE);
    app.target = furi_string_alloc();
    app.emu = EmuNone;

    load_target(&app);
    emu_start(&app);

    app.view_port = view_port_alloc();
    view_port_draw_callback_set(app.view_port, yaof_nightstand_draw_callback, &app);
    view_port_input_callback_set(app.view_port, yaof_nightstand_input_callback, &app);
    app.gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app.input_queue, &event, 500) == FuriStatusOk) {
            if(event.type == InputTypeShort) {
                if(event.key == InputKeyBack) {
                    running = false;
                } else if(event.key == InputKeyDown) {
                    pick_target(&app);
                }
            }
        }
        view_port_update(app.view_port);
    }

    emu_stop(&app);
    gui_remove_view_port(app.gui, app.view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app.view_port);
    furi_string_free(app.target);
    furi_record_close(RECORD_STORAGE);
    furi_message_queue_free(app.input_queue);

    return 0;
}
