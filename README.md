# ATK Control

A native Qt 6 application for configuring ATK, VXE and Compx gaming mice on Linux.
It talks to the mouse directly over raw HID, uses a dark interface, and stays light on memory.

The project is an independent reimplementation of the protocol work published by the
MIT-licensed [atk-control](https://github.com/NasirA1991/atk-control) project. It is not
affiliated with ATK, VXE or Compx.

## Features

- Native C++/Qt 6 Widgets UI with a dark Fusion-based theme.
- Protocol v1 (17-byte reports) and protocol v2 (64-byte reports).
- Device overview with model, sensor, firmware, dongle, connection and support status.
- Battery percentage, charging state and voltage, refreshed in the background.
- DPI stages with X/Y values, active stage selection and per-stage RGB colors.
- Polling rate, lift-off distance, debounce, sleep timeout and sensor mode.
- Motion Sync, ripple control, straight-line correction and angle snapping where supported.
- Button remapping with read-back verification and a reset-to-defaults action.
- Logo / DPI lighting mode, brightness and speed.
- On-board profile switching and explicit profile commit for protocol v1.
- Raw EEPROM read/write for protocol v1 and a protocol self-test.
- CLI mode for listing devices, printing battery state as text or JSON.
- D-Bus battery service and a cache file for the bundled KDE Plasma widget.
- Built-in udev rule installer using `pkexec`.

## Supported devices

The model tables cover the ATK, VXE and Compx families, including MAD R, F1, X1, Z1, U2,
A9, Dragonfly, R1, V3 and X3 series. Both 1K, 4K and 8K receivers are recognized.

Unknown devices with a supported ATK/VXE vendor ID are opened in experimental mode. The app
shows which features the device reports and disables controls that are not available. Writes
are still allowed for experimental devices, so keep the original vendor software available if
you want to restore defaults.

Supported USB vendor IDs:

- `0x373b` - ATK / Compx
- `0x3554` - Compx
- `0x25a7` - VXE

The detection path was verified with a `0x373b:0x1085` CX Wireless mouse 1K dongle. Full
hardware write testing requires the udev rule below and the physical mouse.

## Requirements

Runtime:

- Linux with hidraw support
- Qt 6 Core, Gui, Widgets and DBus
- A KDE Plasma 6 session for the optional widget
- A udev rule for unprivileged access to the mouse

Build:

- CMake 3.21 or newer
- A C++17 compiler
- Qt 6 development packages
- `rsvg-convert` or ImageMagick only if you regenerate the PNG icon

On Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake qt6-base
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/atk-control
```

Or use the helper script:

```bash
./scripts/build.sh
```

## Run

```bash
./build/atk-control
```

If the device list shows `no permission`, install the udev rule:

```bash
./scripts/install-udev.sh
```

The same action is available from the application: **Advanced -> Permissions -> Install udev rule**.
After installing, reconnect the mouse and press **Refresh**.

## Command line

```bash
atk-control --list
atk-control --list --json
atk-control --battery
atk-control --battery --json
atk-control --selftest
atk-control --install-udev
atk-control --debug
atk-control --device /dev/hidraw9 --battery
```

`--list` and `--battery` return a non-zero exit code when no supported device is found.

Example JSON:

```json
{"charging":false,"connected":true,"model":"ATK F1 Pro","percent":82,"protocol":"v2","voltage_mv":4038}
```

## AppImage

Download the AppImage tools and place them in `/tmp/appimage-tools` (or set `APPIMAGE_TOOLS`):

```bash
mkdir -p /tmp/appimage-tools
cd /tmp/appimage-tools
curl -LO https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
curl -LO https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
curl -LO https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x linuxdeploy linuxdeploy-plugin-qt appimagetool
```

Build the AppImage:

```bash
./scripts/build-appimage.sh
```

The result is `atk-control-1.0.0-x86_64.AppImage`. It bundles the X11, Wayland and offscreen
Qt platform plugins. The AppImage still needs the udev rule for raw HID access.

Verify the AppImage without a desktop session:

```bash
QT_QPA_PLATFORM=offscreen ./atk-control-1.0.0-x86_64.AppImage --selftest
```

## KDE Plasma widget

The widget reads `~/.cache/atk-control/battery.json`, so it works even when the D-Bus service
is not available. Start ATK Control at least once so the cache file is created.

```bash
./scripts/install-plasmoid.sh
```

Then add **ATK Mouse Battery** from the Plasma widget picker. The widget refreshes every 30
seconds and offers a manual refresh button.

## D-Bus interface

When the application is running it registers:

- Service: `org.atk.Control`
- Object: `/Battery`
- Interface: `org.atk.Control.Battery`
- Method: `GetBattery()` returning a `QVariantMap`
- Signal: `BatteryChanged(QVariantMap)`

The map contains `connected`, `percent`, `charging`, `voltage_mv`, `model` and `updated`.

## Battery cache

The cache file is written atomically on every battery update:

```
~/.cache/atk-control/battery.json
```

Fields: `connected`, `percent`, `charging`, `voltage_mv`, `model`, `updated` (Unix seconds).

## Project layout

```
src/                 C++ sources
  protocol.*         v1 and v2 framing, checksums, DPI and RGB encoding
  models.*           model, sensor and receiver tables
  hidraw.*           hidraw discovery and transport
  device.*           high-level device state and verified setters
  worker.*           background device thread
  batteryservice.*   D-Bus battery service
  mainwindow.*       Qt Widgets interface
  main.cpp           application entry point and CLI
packaging/           desktop file, icons, udev rule, Plasma widget
scripts/             build, AppImage, udev and widget install helpers
```

## Protocol notes

Protocol v1 uses a 17-byte HID report with report ID `0x08`; the 16-byte payload carries a
command, a small body and a checksum that makes the total equal `0x55`. Configuration values
are stored as single bytes with a complement byte.

Protocol v2 uses a 64-byte frame with report ID `0x08`, a checksum, marker `0x72`, target,
command and length fields. Configuration is read and written through command `0x17` memory
blocks. Battery, charging, DPI, buttons and lighting use dedicated commands.

The protocol research and device tables are based on the `atk-control` project. See
[PROTOCOL.md](https://github.com/NasirA1991/atk-control/blob/main/docs/PROTOCOL.md) in that
repository for the original notes.

## Safety and limitations

- This is unofficial software. Use it at your own risk.
- Raw EEPROM writes can change undocumented settings. Read before you write.
- Some devices answer only while the mouse is awake. Move the mouse if battery or settings
  look stale.
- Wireless dongles may expose a different feature set than a wired connection.
- The app verifies writes by reading the value back, but a failed verification can still
  leave a partial change on the device.
- The AppImage contains Qt libraries; it is larger than the dynamically linked binary.

## Credits

- [atk-control](https://github.com/NasirA1991/atk-control) for the protocol research and
  model tables (MIT).
- [atk-info](https://github.com/surfaceflinger/atk-info) and
  [Linux-ATK](https://github.com/VoideUI/Linux-ATK) for additional device and battery details.
- The Qt project for the application framework.

## License

GPL-3.0-only. See [LICENSE](LICENSE).

The protocol research this project is based on was published under MIT by the upstream
`atk-control` project; that attribution is kept in the credits above.
