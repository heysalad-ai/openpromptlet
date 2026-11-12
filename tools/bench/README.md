# Local bench tools

Run `python -m pip install -r tools/bench/requirements.txt` in a virtual environment, then `python tools/bench/server.py`. The interface listens on loopback port 8773; it reads four encoders and does not flash boards.

Pairing and optional LAN setup are described in [the setup guide](../../docs/setup.md). One serial owner per board at a time. `screen-link/usb_link.py` uses POSIX USB handling and does not toggle DTR/RTS.

Run regression checks from the repository root:

```sh
python -m unittest discover -s tools/bench -p test_protocol.py -v
c++ -std=c++11 tools/bench/test_encoder_packet.cpp -o /tmp/openpromptlet-packet-test
/tmp/openpromptlet-packet-test
```
