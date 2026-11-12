"""Loopback-only, password-masked provisioning for the two USB-identified boards."""
import argparse,asyncio,json,os,re,secrets,signal,subprocess,time,urllib.request
from pathlib import Path
from aiohttp import web
from serial.tools import list_ports
from usb_link import RawUSB
P=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--encoder-serial',required=True);parser.add_argument('--screen-serial',required=True);args=parser.parse_args()
csrf=secrets.token_hex(24);state={'status':'Enter Wi-Fi details locally; do not paste passwords in chat.'};busy=False
HTML='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>OpenDotlet Wi-Fi setup</title><style>body{font:18px system-ui;background:#131918;color:#f4f1e9;max-width:620px;margin:6vh auto;padding:24px}h1{font-size:32px}label,input,button{display:block;width:100%;box-sizing:border-box}input,button{font:inherit;padding:14px;margin:8px 0 24px;border-radius:10px;border:1px solid #7b8783}button{background:#ff9c52;color:#171916;cursor:pointer}small{color:#bec9c3}pre{white-space:pre-wrap}</style><h1>OpenDotlet Wi-Fi setup</h1><p>Configure the encoder board and round screen for authenticated updates on the same Wi-Fi network.</p><small>USB power only. This page runs on this Mac. Your password is sent over USB to the boards and is not saved on this Mac.</small><form id="f"><input type="hidden" name="csrf" value="TOKEN"><label>Wi-Fi network name<input name="ssid" required maxlength="32" autocomplete="off"></label><label>Wi-Fi password<input name="password" type="password" required minlength="8" maxlength="63" autocomplete="off"></label><button>Configure both boards</button></form><pre id="status"></pre><script>const f=document.getElementById('f'),s=document.getElementById('status');f.onsubmit=async(e)=>{e.preventDefault();const b=f.querySelector('button');b.disabled=true;s.textContent='Configuring boards. This can take a minute.';try{const r=await fetch('/configure',{method:'POST',body:new FormData(f)});s.textContent=JSON.stringify(await r.json(),null,2);}catch(e){s.textContent='Setup did not complete. Check the board connections.';}f.password.value='';b.disabled=false;};</script></html>'''.replace('TOKEN',csrf)
def ui(action):
 req=urllib.request.Request('http://127.0.0.1:8773/api/action',data=json.dumps(action).encode(),headers={'Content-Type':'application/json'})
 return json.load(urllib.request.urlopen(req,timeout=4))
def stop_monitor(port):
 r=subprocess.run(['lsof','-t',port],capture_output=True,text=True)
 for pid in r.stdout.split():
  cmd=subprocess.check_output(['ps','-p',pid,'-o','command='],text=True)
  if 'pair_and_monitor.py' not in cmd:raise RuntimeError('Screen serial is owned by another application; close it first')
  os.kill(int(pid),signal.SIGINT)
 time.sleep(.6)
def provision(ssid,password):
 ports={p.serial_number.upper():p.device for p in list_ports.comports() if p.serial_number}
 encoder=ports[args.encoder_serial.upper()];screen=ports[args.screen_serial.upper()]
 stop_monitor(screen);ui({'action':'disconnect'});results=[]
 keys=Path.home()/'.local/share/opendotlet/update-keys';keys.mkdir(parents=True,exist_ok=True,mode=0o700);keys.chmod(0o700)
 try:
  for name,serial_id,port in [('screen',args.screen_serial,screen),('encoder',args.encoder_serial,encoder)]:
   s=RawUSB(port)
   def wait(command,pattern,timeout=12):
    s.write(command.encode()+b'\n');end=time.monotonic()+timeout;retry=time.monotonic()+1
    while time.monotonic()<end:
     line=s.readline().decode(errors='replace').strip();m=re.search(pattern,line)
     if m:return m
     # The web UI can reset USB while releasing serial ownership. Retry only
     # read-only queries while the encoder finishes booting.
     if command in ('UPDATE STATUS','STATUS','link') and time.monotonic()>=retry:
      s.write(command.encode()+b'\n');retry=time.monotonic()+1
    raise RuntimeError(name+' did not answer '+command.split()[0])
   try:
    wait('UPDATE STATUS',r'WIFI_UPDATE enabled=1')
    # Identify using firmware's actual MAC, not a guessed source-code address.
    mac=wait('STATUS' if name=='screen' else 'link',r'MAC=([0-9a-fA-F:]{17})').group(1).lower()
    if mac.replace(':','')!=serial_id.lower().replace(':',''):raise RuntimeError('Runtime device identity mismatch')
    key=secrets.token_bytes(32);command='WIFI '+ssid.encode().hex()+' '+password.encode().hex()+' '+key.hex()
    wait(command,r'WIFI_UPDATE configuration accepted')
    cfg={'name':name,'mac_hex':mac.replace(':',''),'key_hex':key.hex(),'ip':None};file=keys/(cfg['mac_hex']+'.json')
    fd=os.open(file,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
    with os.fdopen(fd,'w') as f:json.dump(cfg,f)
    end=time.monotonic()+35
    while time.monotonic()<end:
     line=s.readline().decode(errors='replace').strip();m=re.search(r'WIFI_UPDATE IP=([0-9.]+)',line)
     if m:cfg['ip']=m.group(1);break
    if not cfg['ip']:raise RuntimeError(name+' did not obtain a Wi-Fi address. Check the network/password and retry.')
    file.write_text(json.dumps(cfg));file.chmod(0o600);results.append({'device':name,'ip':cfg['ip'],'private_key_file':str(file)})
   finally:s.close()
 finally:
  ui({'action':'connect','port':encoder})
  log=P.parent/'evidence/screen-link-20261110/live-screen.jsonl'
  output=(P.parent/'evidence/screen-link-20261110/monitor-restarted.log').open('a')
  subprocess.Popen(['uv','run','--no-project','--with','pyserial','python',str(P/'pair_and_monitor.py'),'--encoder-serial',args.encoder_serial,'--screen-serial',args.screen_serial,'--log',str(log)],stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
 return results
async def index(request):return web.Response(text=HTML,content_type='text/html',headers={'Cache-Control':'no-store'})
async def configure(request):
 global busy,state
 if request.headers.get('Origin')!='http://'+request.host:raise web.HTTPForbidden()
 data=await request.post()
 if not secrets.compare_digest(data.get('csrf',''),csrf):raise web.HTTPForbidden()
 if busy:return web.json_response({'status':'Setup already running'},status=409)
 ssid=data.get('ssid','');password=data.get('password','')
 if not 1<=len(ssid.encode())<=32 or not 8<=len(password.encode())<=63:return web.json_response({'status':'Use a1–32byte network name and8–63byte WPA2 password'},status=400)
 busy=True
 try:state={'status':'Configured','devices':await asyncio.to_thread(provision,ssid,password)}
 except Exception as e:state={'status':str(e).replace(password,'[redacted]')}
 finally:busy=False;password=''
 return web.json_response(state)
app=web.Application(client_max_size=4096);app.add_routes([web.get('/',index),web.post('/configure',configure)])
if __name__=='__main__':web.run_app(app,host='127.0.0.1',port=8774,access_log=None)
