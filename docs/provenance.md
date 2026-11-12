# Prototype provenance and limits

Initial source export: 12 November 2026.

- Round-screen source: OpenDotlet checkpoint `68570dced09763f3e94cecfbb1df372af61646ef`, tag `opendotlet-four-encoder-rotated-20261112`.
- Sender and bench source: Firmware_Mgmt checkpoint `a70343ca4de4ef125f0589cb97d11a5e5aad27d3`.
- The application and helpers are retained with their internal OpenDotlet identifiers. Standalone screen CMake contains only the encoder application branch; unused gallery assets are omitted.
- The two STLs retain their original bytes; [the manifest](../hardware/stl/manifest.json) records their source filenames and hashes. The v5 bottom was generated specifically for the unchanged v4.2 four-control sticker top.

## Observed hardware evidence

Four-encoder physical acceptance on 10 November captured 209 consecutive screen-applied events with no gaps or duplicates, with turns in both directions and press/release on every control. On 12 November the screen application gained 180° software rotation; the user then confirmed the display is upright and all four knobs work. The original accepted checkpoints remain in their owning projects.

This export's verification is documented in [verification.md](verification.md). Build checks do not establish a new physical fit or long-run test. Full flash backups, raw private logs, Wi-Fi credentials, per-device keys and hardware MAC records are excluded.

## Still open

- Repeatable enclosure assembly and an LCD-1.46-specific bracket, with final fastener dimensions.
- Verified encoder caps, battery power/charging and a separate keypad integration.
- A hardware demo video and software integrations beyond counter/confirmation feedback.
- Long-run, production and independent security testing.
