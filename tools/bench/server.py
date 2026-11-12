#!/usr/bin/env python3
# Run: uv run --with aiohttp --with pyserial python server.py
import asyncio
import re
import time
from pathlib import Path

import serial
from serial.tools import list_ports
from aiohttp import web


class Bench:
    def __init__(self):
        self.link = None
        self.lock = asyncio.Lock()
        self.state = dict(connected=False, verified=False, port='', error='', last_rx=None,
                          overflow=0, log=[], encoders={})
        self.reset()

    def reset(self):
        self.state['overflow'] = 0
        self.state['encoders'] = {str(e): dict(port=p, count=0, invalid=0, presses=0,
            pressed=None, delta=0, updated=None, observed=False) for e, p in [(1, 3), (2, 2), (3, 1), (4, 4)]}

    def parse(self, line):
        self.state['last_rx'] = time.time()
        self.state['log'] = (self.state['log'] + [line])[-35:]
        banner = re.match(r'Genesis (dual|triple|quad) encoder bench', line)
        if banner or line.startswith('ENC1 port=3'):
            self.state['verified'] = True
        if banner:
            self.reset()
        match = re.fullmatch(r'COUNT encoder=([1234]) delta=(-?\d+) total=(-?\d+) invalid=(\d+)', line)
        if match:
            e, delta, total, invalid = match.groups()
            self.state['encoders'][e].update(count=int(total), delta=int(delta), invalid=int(invalid),
                updated=time.time(), observed=True)
        match = re.fullmatch(r'BUTTON ms=\d+ encoder=([1234]) event=(PRESS|RELEASE) raw=[01]', line)
        if match:
            e, event = match.groups()
            item = self.state['encoders'][e]
            item['pressed'] = event == 'PRESS'
            if event == 'PRESS':
                item['presses'] += 1
        match = re.match(r'OVERFLOW dropped=(\d+)', line)
        if match:
            self.state['overflow'] = int(match[1])

    async def disconnect(self):
        if self.link:
            self.link.close()
        self.link = None
        self.state.update(connected=False, verified=False)

    async def connect(self, port):
        async with self.lock:
            await self.disconnect()
            self.reset()
            self.state.update(error='', log=[], last_rx=None, port=port)
            try:
                link = serial.Serial()
                link.port, link.baudrate, link.timeout = port, 115200, 0
                link.dtr = link.rts = False
                link.open()
                self.link = link
                self.state['connected'] = True
                # Existing firmware: test mode disables verbose raw-edge output.
                link.write(b't\ns\n')
            except (OSError, serial.SerialException) as exc:
                self.state['error'] = str(exc)
                await self.disconnect()

    async def reader(self):
        buffer = b''
        previous = None
        last_status = 0
        while True:
            try:
                if self.link:
                    if previous is not self.link:
                        buffer = b''
                        previous = self.link
                        last_status = 0
                    if time.monotonic() - last_status > 3:
                        self.link.write(b's\n')
                        last_status = time.monotonic()
                    data = self.link.read(8192)
                    buffer += data
                    while b'\n' in buffer:
                        line, buffer = buffer.split(b'\n', 1)
                        self.parse(line.decode(errors='replace').strip())
                    if len(buffer) > 16384:
                        buffer = b''
                await asyncio.sleep(.025)
            except (OSError, serial.SerialException) as exc:
                self.state['error'] = 'USB connection lost: ' + str(exc)
                await self.disconnect()


bench = Bench()
ROOT = Path(__file__).parent


async def index(request):
    return web.FileResponse(ROOT / 'index.html')


async def state(request):
    return web.json_response(bench.state)


async def ports(request):
    return web.json_response([dict(path=p.device, name=p.description, serial=p.serial_number)
        for p in list_ports.comports() if p.vid is not None])


async def action(request):
    # Only same-origin browser actions may write to this local serial connection.
    origin = request.headers.get('Origin')
    if origin and origin != f'http://{request.host}':
        raise web.HTTPForbidden()
    body = await request.json()
    command = body.get('action')
    if command == 'connect':
        allowed = {p.device for p in list_ports.comports() if p.vid == 0x303A}
        if body.get('port') not in allowed:
            raise web.HTTPBadRequest(text='Select a connected Espressif USB device.')
        await bench.connect(body['port'])
    elif command == 'disconnect':
        await bench.disconnect()
    elif command in ('reset', 'status'):
        if not bench.link:
            raise web.HTTPConflict(text='Connect the board first.')
        try:
            bench.link.write(b'z\ns\n' if command == 'reset' else b's\n')
        except (OSError, serial.SerialException) as exc:
            bench.state['error'] = str(exc)
            await bench.disconnect()
            raise web.HTTPConflict(text='USB connection lost.')
        if command == 'reset':
            bench.reset()
    else:
        raise web.HTTPBadRequest(text='Unknown action.')
    return web.json_response(bench.state)


async def lifecycle(app):
    task = asyncio.create_task(bench.reader())
    yield
    task.cancel()
    await bench.disconnect()


app = web.Application()
app.add_routes([web.get('/', index), web.get('/api/state', state),
    web.get('/api/ports', ports), web.post('/api/action', action)])
app.cleanup_ctx.append(lifecycle)
if __name__ == '__main__':
    web.run_app(app, host='127.0.0.1', port=8773)
