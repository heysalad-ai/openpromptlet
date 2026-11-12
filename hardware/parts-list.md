# Parts list

[Download CSV](parts-list.csv)

| Part | Quantity | Notes |
| --- | --- | --- |
| Waveshare **ESP32-S3-Touch-LCD-1.46** | 1 | 412 × 412 SPD2010 LCD; ESP32-S3, 16 MB flash, 8 MB PSRAM. Do not substitute the AMOLED-1.43. |
| Axiometa **Genesis Mini** | 1 | Use the official Arduino board definition. |
| Axiometa **AX22** rotary encoder modules | 4 | Push-button encoders; the signal mapping follows the AX22-0003 Rev 2 schematic. |
| Matching Axiometa module cables | 4 | Connect to labelled ports 3, 2, 1 and 4. Verify connector orientation against the vendor documentation. |
| USB-C power/data cables | 2 | Each board receives its own USB power; data required for initial flashing and pairing. |
| Printed top and bottom | 1 each | See [STLs and fit notes](README.md). |
| 6 × 2 mm disc magnets | 6 optional | Three lid-retention pairs in the CAD; retention is unverified. Removable nonconductive tape is an alternative during fit checks. |
| Encoder caps | 4 optional | The photographed prototype uses bare controls. No verified cap design is released here. |
| Board fixings/spacers | Measure for your stack | The final fastener lengths and LCD-1.46 mount are still open. Do not force screws into the display or PCB. |

The bottom has optional magnetic join pockets for a future keypad. No join magnets, keypad, battery or charger are needed for this USB-powered controller build.

Vendor references: [Waveshare board documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46) · [Axiometa Arduino board definition](https://github.com/espressif/arduino-esp32/tree/3.3.11/variants/axiometa_genesis_mini)
