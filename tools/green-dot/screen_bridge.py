import argparse, json, re, secrets, time, urllib.request
from pathlib import Path
import serial
from serial.tools import list_ports
parser=argparse.ArgumentParser(description='Mirror live host checkpoints onto the OpenPromptlet S3 screen; never launches or approves tasks.')
parser.add_argument('--screen-serial',required=True)
parser.add_argument('--feed',required=True,help='HTTP URL of the trusted host state JSON')
parser.add_argument('--brightness',type=int,default=30,choices=range(10,101))
args=parser.parse_args()
def report(**data):
 print(json.dumps(data),flush=True)
ports=[p.device for p in list_ports.comports() if re.sub('[^0-9a-f]','',(p.serial_number or '').lower())==re.sub('[^0-9a-f]','',args.screen_serial.lower())]
if len(ports)!=1:raise RuntimeError('Matching screen not found')
p=serial.Serial(port=None,baudrate=115200,timeout=.05,write_timeout=2);p.dtr=False;p.rts=False;p.port=ports[0];p.open()
nonce=secrets.token_hex(16);revision=0;hello=False;next_hello=0;last_ack=0;last_report='';last_fetch=0;state=None
def send(line):p.write((line+'\n').encode());p.flush()
def text(value,limit):return ''.join(c if 32<=ord(c)<127 or c=='\n' else '?' for c in value)[:limit].encode().hex() or '-'
report(phase='green_bridge_started',port=ports[0])
while True:
 now=time.monotonic()
 if now>=next_hello and not hello:
  send('HOST_HELLO '+nonce);next_hello=now+1
 if now-last_fetch>.5:
  last_fetch=now
  try:
   data=json.load(urllib.request.urlopen(args.feed,timeout=2))
   # Freshness is tied to producer heartbeat, not event timestamps. Never
   # hold an obsolete Working or Complete state after the host goes away.
   if abs(time.time()-data['heartbeat'])>4:raise ValueError('Host heartbeat expired')
   state=data
  except Exception:state=None
  if hello and state:
   revision+=1;done=state['done'];phase=state['phase'];elapsed=state['elapsed']
   send('HOST_STATE %s %d %d %d %d %s %s'%(nonce,revision,phase,elapsed,args.brightness,text(state['title'],48),text(state['detail'],144)))
   send('VISUAL_STATE %s %d %d'%(nonce,revision,done))
 line=p.readline().decode('ascii',errors='replace').strip()
 if line.startswith('ACTION_HELLO') and 'nonce='+nonce in line:hello=True;last_ack=now
 if line.startswith(('HOST_ACK','VISUAL_ACK','SCREEN_LINK','ACTION_STATUS')):
  if line.startswith('HOST_ACK'):last_ack=now
  if line.startswith('VISUAL_ACK'):
   value=line.split('checkpoints=')[-1]
   if value!=last_report:
    last_report=value;report(phase='screen_visual_ack',line=line)
 if hello and now-last_ack>4:
  hello=False;nonce=secrets.token_hex(16);revision=0
 time.sleep(.01)
