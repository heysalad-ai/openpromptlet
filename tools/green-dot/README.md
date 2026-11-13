# Green-dot display bridge

This bridge mirrors host task state onto the ESP32-S3. It does not execute browser actions, launch jobs, recognize speech or approve requests. The host agent owns the real task; send a checkpoint only after its browser/tool result is observed.

Install the green-dot display profile, close other programs using the screen serial port, then use Python 3.10+ with `pyserial==3.5`:

```sh
python screen_bridge.py --screen-serial YOUR_SCREEN_USB_SERIAL --feed http://127.0.0.1:8794/state --brightness 30
```

Keep the feed private to the trusted companion. It returns an object with `heartbeat` (current Unix seconds, renewed at least once a second), `phase` (0 ready, 1 starting, 2 working, 3 complete, 4 failed, 5 busy, 6 uncertain, 7 offline), `done` (verified checkpoint count, 0–5), `elapsed` (actual seconds), `title` (up to 48 ASCII characters) and `detail` (up to 144 ASCII characters, newlines allowed). Label planned duration and tokens as estimates; do not substitute them for reported usage.

The adapter selects the exact USB serial, keeps DTR/RTS low, negotiates a fresh nonce and increases state revisions. Missing/stale host state stops outgoing heartbeats; the firmware shows offline after about three seconds. No task is replayed on reconnect. stdout contains checkpoint acknowledgements for local verification.

The recorded setup used a peer-restricted LAN feed on the Mac and this same protocol over Windows USB. Private connection records, installation scripts and full-flash backups are not release inputs. See [the actual hardware demonstration](../../docs/GREEN-DOT-DEMO.md).
