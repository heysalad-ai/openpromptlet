# Build and setup

This guide targets the **Waveshare ESP32-S3-Touch-LCD-1.46** and **Axiometa Genesis Mini + four AX22 modules**. The included helper tools target macOS/Linux; their raw USB implementation uses POSIX `termios`. Other systems can build the firmware, but need a suitable serial terminal for manual pairing.

## 1. Assemble and power the bench prototype

Read [parts](../hardware/parts-list.md) and [mechanical limits](../hardware/README.md) first. Disconnect power when changing module cables. Connect compatible AX22 modules to the labelled Genesis ports:

| Display control | Genesis port | Button / A / B GPIO |
| --- | --- | --- |
| 1 | 3 | 9 / 16 / 15 |
| 2 | 2 | 7 / 6 / 5 |
| 3 | 1 | 4 / 3 / 2 |
| 4 | 4 | 1 / 17 / 18 |

The code uses the official board constants: IO0 button (active high), IO1 A, IO2 B. Use module cables with the correct vendor pinout; this is not a generic three-wire encoder connection. Connect one USB data/power cable to each board. No wired data link or shared supply is required between the boards: the link is ESP-NOW.

## 2. Get the source

```sh
git clone https://github.com/heysalad-ai/openpromptlet.git
cd openpromptlet
```

Install Python 3.9+ and [ESP-IDF v5.3.2](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/get-started/index.html) with ESP32-S3 tools. In the ESP-IDF shell:

```sh
cd firmware/screen
python tools/bootstrap.py
idf.py -D SDKCONFIG_DEFAULTS=sdkconfig.defaults set-target esp32s3
idf.py build
```

The bootstrap verifies exact vendor file hashes and the pinned LVGL archive. The defaults select 16 MB flash, octal PSRAM, the two-slot OTA partition table and the four-control application. It needs no gallery assets. The display rotates 180° in `main/encoder_demo.cpp`; change `LV_DISP_ROT_180` if your mounting differs, then rebuild.

## 3. USB-install the screen

Identify the correct serial port; replace `SCREEN_PORT` below with its actual path. Before replacing existing firmware, keep a private full backup if you need recovery:

```sh
python -m esptool --chip esp32s3 --port SCREEN_PORT read_flash 0 ALL ~/openpromptlet-screen-backup.bin
idf.py -p SCREEN_PORT flash monitor
```

The first `flash` installs the bootloader, application and partition table and may make prior data inaccessible. Full backups can contain network credentials, so do not commit them. Exit the IDF monitor with Ctrl+]. If the screen is in download mode, follow the board's BOOT/RESET procedure; verify the board identity before writing. Do not run `erase-flash` as a routine setup step.

## 4. Build and USB-install the sender

Install [Arduino CLI](https://arduino.github.io/arduino-cli/) or the Arduino IDE with Espressif's ESP32 core **3.3.11**. CLI setup from the repository root:

```sh
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.11
arduino-cli compile -b esp32:esp32:axiometa_genesis_mini --build-property compiler.cpp.extra_flags=-DOD_WIFI_UPDATE firmware/axiometa_encoder_screen_link
arduino-cli board list
arduino-cli upload -p ENCODER_PORT -b esp32:esp32:axiometa_genesis_mini firmware/axiometa_encoder_screen_link
```

Replace `ENCODER_PORT` with the Genesis USB port. Keep a private backup before replacing existing firmware if recovery matters. On Apple Silicon, an incompatible bundled Intel `ctags` can prevent compilation; install a compatible tool or the required translation support, then point `runtime.tools.ctags.path` at its tool directory. This is a host build-tool issue, not an encoder wiring change.

Both builds include optional authenticated LAN update support, but networking remains unprovisioned until you explicitly configure it over USB. Initial setup and ESP-NOW pairing do not require Wi-Fi credentials or an internet service.

## 5. Start the USB bench interface and pair

From the repository root, create an isolated Python environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r tools/bench/requirements.txt
python tools/bench/server.py
```

Open **http://127.0.0.1:8773**, choose the Genesis serial port and connect. In another terminal, activate the same environment and list the boards' USB serial identifiers:

```sh
python -m serial.tools.list_ports -v
python tools/bench/screen-link/pair_and_monitor.py --encoder-serial ENCODER_USB_SERIAL --screen-serial SCREEN_USB_SERIAL --log /tmp/openpromptlet-pairing.jsonl
```

Use each device's USB serial number from `SER=...`, not its `/dev/...` port path. The helper temporarily releases the interface's encoder connection, reads actual runtime STA MACs, configures both peers, reconnects the interface and monitors the screen. Ctrl+C stops the screen monitor. Peers are saved across reboot. The interface must already be running for this helper.

For manual pairing, use separate 115200-baud newline serial terminals with local echo off. Send `STATUS` to the screen and `link` to the sender. Read the **runtime STA MAC** reported by each. Send `peer OTHER_BOARD_MAC` to each board. On the screen look for `peer_configured=1`; on the sender look for `paired=1` and `welcomed=1`. The default radio channel is 6. Close other serial owners before using a terminal. Do not guess a Wi-Fi MAC from the USB serial number.

## 6. Check the controls

Turn every knob in both directions and press/release each once. Each should affect its matching control on the screen and in the local interface; a press shows **CONFIRMED**. Screen `STATUS` should report `rotation=180`. Reboot both boards and verify peers restore and input still works.

Counts are x4 quadrature transitions, not detents. Clockwise sign depends on the module/mount. `r 1` through `r 4` reverse a sender channel for the current session; those calibration changes are not persisted. `z` resets the trial. Rising `invalid` or `OVERFLOW` counters mean a capture needs investigation. If pairing fails, verify both board identities, port ownership, radio channel and USB power.

## Optional: LAN updates

After successful USB setup, stop the bench server/serial monitors, connect both boards by USB and run:

```sh
python tools/bench/screen-link/setup_wifi.py --encoder-serial ENCODER_USB_SERIAL --screen-serial SCREEN_USB_SERIAL
```

Open **http://127.0.0.1:8774** and enter the local network details. This creates private per-device keys under `~/.local/share/opendotlet/update-keys/`. Keep that directory outside source control. Both boards must use the same Wi-Fi channel; verify the ESP-NOW link again after provisioning.

For later updates use only an application image, never a bootloader, partition table or merged flash image:

```sh
python tools/bench/screen-link/ota_upload.py --config PRIVATE_DEVICE_JSON --image APPLICATION_BIN
```

The tool uses a fresh nonce and HMAC-SHA256 bound to the image digest. It verifies the image and writes the inactive application slot. This prototype LAN protocol is not independently audited and does not encrypt firmware bytes. ESP-NOW control packets are also unencrypted. Keep this bench link limited to input/display experiments. Automatic rollback, battery operation and production reliability are not claimed.
