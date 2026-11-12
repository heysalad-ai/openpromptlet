# Initial export verification — 12 November 2026

- **Screen clean build: passed.** The standalone tree fetched the exact checksummed Waveshare files and LVGL 8.3.11, then configured/built with ESP-IDF 5.3.2 for ESP32-S3. Application size: 1,109,888 bytes; fits the configured 7 MiB application slot. The existing upstream LVGL deprecated enum warning remains.
- **Sender build: passed.** ESP32 Arduino 3.3.11, `esp32:esp32:axiometa_genesis_mini`, with `OD_WIFI_UPDATE`. Sketch: 915,314 bytes; dynamic globals: 54,836 bytes. This Mac used a compatible local ctags tool override.
- **Protocol: passed.** All eight Python tests in `tools/bench/test_protocol.py`; C++ wire encoding/decoding, signed bounds, malformed frame rejection and unicast peer checks in `test_encoder_packet.cpp`.
- **Local HTTP runtime: passed.** With an aiohttp test server on an ephemeral port: page, four-control state and serial-port inventory returned HTTP 200; unknown action returned HTTP 400. No serial device was opened by this check.
- **STLs: passed digital checks.** Both original SHA256 hashes match the exported bytes; trimesh reports watertight meshes. Top bounds 111.05 × 68 × 19.39 mm; bottom 111.05 × 68 × 20 mm. Mesh validity is not a physical assembly test.
- **Publication contents: checked.** An explicit source-file export excludes vendor downloads, build outputs, firmware binaries, private logs, backup images and device provisioning records. Gitleaks passes with two narrowly fingerprinted false-positive exceptions for public upstream SHA256 checksums whose filenames contain `PWR_Key`. The test MAC is a synthetic locally administered address.

No new device flash, print, enclosure fit, long-run or battery test was performed for this export. Earlier user-confirmed prototype behavior and remaining limits are recorded in [provenance.md](provenance.md).
