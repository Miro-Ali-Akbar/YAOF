# YAOF: Yet Another Opinionated Flipper

Personal, opinionated firmware for the [Flipper Zero](https://flipperzero.one/), built on top of [Momentum](https://github.com/Next-Flip/Momentum-Firmware).

YAOF tracks Momentum's `dev` branch and layers its own defaults, navigation and theming on top. The firmware still reports itself as Momentum-based, so Momentum apps, JS scripts and the Flipper mobile app keep working.

The upstream Momentum readme is in [`ReadMe.md`](../ReadMe.md).

## Install

Download `flipper-z-f7-update-yaof-*.tgz` from the [latest release](https://github.com/Miro-Ali-Akbar/YAOF/releases/latest) (or from a build's artifacts) and install it with qFlipper (**Install from file**).

Do not update firmware from the Flipper mobile app: that installs official firmware over YAOF.

## Build

```sh
./fbt updater_package     # dist/f7-C/flipper-z-f7-update-yaof-*.tgz
./fbt flash_usb_full      # build and flash a USB-connected Flipper
```

Releases are built by GitHub Actions when a `yaof-NNN` tag is pushed.

## Syncing with Momentum

```sh
git fetch upstream
git merge upstream/dev
```

## License

GPL-3.0, same as Momentum and the official firmware.
