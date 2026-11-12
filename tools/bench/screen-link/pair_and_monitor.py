"""Pair using runtime STA MACs; keep the original test UI as encoder serial owner."""
import argparse,json,re,time,urllib.request,os,termios,tty,select
from pathlib import Path
import serial
from serial.tools import list_ports
p=argparse.ArgumentParser();p.add_argument('--encoder-serial',required=True);p.add_argument('--screen-serial',required=True);p.add_argument('--log',required=True);args=p.parse_args()
ports={p.serial_number.upper():p.device for p in list_ports.comports() if p.serial_number}
enc_port=ports[args.encoder_serial.upper()];screen_port=ports[args.screen_serial.upper()]
def open_port(port):
 s=serial.Serial();s.port=port;s.baudrate=115200;s.timeout=.2;s.dtr=s.rts=False;s.open();return s
from usb_link import RawUSB
log=Path(args.log);log.parent.mkdir(exist_ok=True,parents=True)
def record(label,line):
 with log.open('a') as f:f.write(json.dumps({'time':time.time(),'source':label,'line':line})+'\n')
def wait(s,label,command,pattern,timeout=8):
 s.write(command.encode()+b'\n');end=time.monotonic()+timeout
 while time.monotonic()<end:
  line=s.readline().decode(errors='replace').strip()
  if line:
   record(label,line);m=re.search(pattern,line)
   if m:return m
 raise RuntimeError(label+' did not answer '+command.split()[0])
# Release the UI's serial ownership explicitly, and restore it even if pairing fails.
req=urllib.request.Request('http://127.0.0.1:8773/api/action',data=b'{"action":"disconnect"}',headers={'Content-Type':'application/json'})
json.load(urllib.request.urlopen(req))
screen=RawUSB(screen_port);encoder=open_port(enc_port)
try:
 time.sleep(1.5)
 sm=wait(screen,'screen','STATUS',r'SCREEN_LINK v1 MAC=([0-9a-fA-F:]{17})').group(1)
 em=wait(encoder,'encoder','link',r'LINK .*MAC=([0-9a-fA-F:]{17})').group(1)
 assert em!='00:00:00:00:00:00' and sm!='00:00:00:00:00:00', 'Invalid runtime MAC'
 wait(screen,'screen','peer '+em,r'peer_configured=1')
 wait(encoder,'encoder','peer '+sm,r'paired=1')
 encoder.close()
 req=urllib.request.Request('http://127.0.0.1:8773/api/action',data=json.dumps({'action':'connect','port':enc_port}).encode(),headers={'Content-Type':'application/json'})
 state=json.load(urllib.request.urlopen(req));assert state['connected']
 print(json.dumps({'ready':True,'screen_mac':sm,'encoder_mac':em,'screen_port':screen_port,'encoder_port':enc_port,'channel':6,'log':str(log)}),flush=True)
 while True:
  line=screen.readline().decode(errors='replace').strip()
  if line:
   record('screen',line)
   if 'SCREEN_APPLIED' in line:print(line,flush=True)
except KeyboardInterrupt:pass
finally:
 encoder.close();screen.close()
 try:
  req=urllib.request.Request('http://127.0.0.1:8773/api/action',data=json.dumps({'action':'connect','port':enc_port}).encode(),headers={'Content-Type':'application/json'})
  urllib.request.urlopen(req).close()
 except Exception:pass
