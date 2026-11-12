# Firmware

- [`screen/`](screen): Waveshare ESP32-S3-Touch-LCD-1.46 receiver, ESP-IDF **5.3.2**, LVGL **8.3.11**. Includes the accepted 180° software rotation.
- [`axiometa_encoder_screen_link/`](axiometa_encoder_screen_link): four-encoder sender, ESP32 Arduino core **3.3.11**, board `esp32:esp32:axiometa_genesis_mini`.

Build, USB flash and pairing commands are in the [setup guide](../docs/setup.md). Do not flash the sender onto the screen or vice versa.

The screen CMake project remains named `opendotlet` and the sketch retains its original name for compatibility with existing OTA image validation. These internal names do not change the OpenPromptlet product name.

Vendor board support is not redistributed. Run the pinned bootstrap and review [third-party terms](../THIRD_PARTY_NOTICES.md). No compiled firmware or private flash dumps are published.
