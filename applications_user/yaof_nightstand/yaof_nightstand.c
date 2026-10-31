// YAOF Nightstand: shows a clock and emulates the home NFC tag while open.
// The tag file is a personal secret deployed from YAOF-config, referenced by path.
#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <input/input.h>
#include <datetime/datetime.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>

#define HOME_TAG_PATH "/ext/nfc/home.nfc"

typedef struct {
    FuriMessageQueue* input_queue;
    bool emulating;
} Nightstand;

static void yaof_nightstand_draw_callback(Canvas* canvas, void* context) {
    Nightstand* app = context;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char time_str[16];
    char date_str[16];
    snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
    snprintf(date_str, sizeof(date_str), "%04u-%02u-%02u", dt.year, dt.month, dt.day);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, time_str);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, date_str);
    canvas_draw_str_aligned(
        canvas,
        64,
        58,
        AlignCenter,
        AlignCenter,
        app->emulating ? "Emulating home tag" : "home.nfc not found");
}

static void yaof_nightstand_input_callback(InputEvent* event, void* context) {
    Nightstand* app = context;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

int32_t yaof_nightstand_app(void* p) {
    UNUSED(p);

    Nightstand app = {0};
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Start NFC emulation of the home tag (runs on its own thread).
    Nfc* nfc = nfc_alloc();
    NfcDevice* device = nfc_device_alloc();
    NfcListener* listener = NULL;
    if(nfc_device_load(device, HOME_TAG_PATH)) {
        NfcProtocol protocol = nfc_device_get_protocol(device);
        listener = nfc_listener_alloc(nfc, protocol, nfc_device_get_data(device, protocol));
        nfc_listener_start(listener, NULL, NULL);
        app.emulating = true;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, yaof_nightstand_draw_callback, &app);
    view_port_input_callback_set(view_port, yaof_nightstand_input_callback, &app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Redraw every 500 ms so the clock ticks; exit on Back.
    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app.input_queue, &event, 500) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                running = false;
            }
        }
        view_port_update(view_port);
    }

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);

    if(listener) {
        nfc_listener_stop(listener);
        nfc_listener_free(listener);
    }
    nfc_device_free(device);
    nfc_free(nfc);
    furi_message_queue_free(app.input_queue);

    return 0;
}
