"""Authenticated app-only LAN update. The secret remains in a private local file."""
import argparse,hashlib,hmac,ipaddress,json,re,time,urllib.request
from pathlib import Path

def authorize(key,nonce,digest):
    if len(key)!=32 or not re.fullmatch('[0-9a-f]{32}',nonce) or not re.fullmatch('[0-9a-f]{64}',digest):
        raise ValueError('Malformed challenge or digest')
    return hmac.new(key,(nonce+' '+digest).encode(),hashlib.sha256).hexdigest()

def main():
    p=argparse.ArgumentParser();p.add_argument('--config',type=Path,required=True);p.add_argument('--image',type=Path,required=True);args=p.parse_args()
    cfg=json.loads(args.config.read_text());key=bytes.fromhex(cfg['key_hex']);ip=ipaddress.ip_address(cfg['ip'])
    if not ip.is_private or ip.is_multicast:raise ValueError('Only a local private device IP is supported')
    base=f'http://{ip}:8080'
    def info():return json.load(urllib.request.urlopen(base+'/info',timeout=4))
    before=info()
    if before['mac_hex'].lower()!=cfg['mac_hex'].lower():raise ValueError('Device MAC does not match USB provisioning')
    image=args.image.read_bytes()
    if len(image)<32 or image[0]!=0xe9:raise ValueError('Use an ESP32 application .bin, not a merged/ZIP file')
    digest=hashlib.sha256(image).hexdigest();nonce=urllib.request.urlopen(base+'/challenge',timeout=4).read().decode().strip()
    request=urllib.request.Request(base+'/update',data=image,method='POST',headers={'Content-Type':'application/octet-stream','X-OD-Nonce':nonce,'X-OD-SHA256':digest,'X-OD-MAC':authorize(key,nonce,digest)})
    result=urllib.request.urlopen(request,timeout=90).read().decode();print(result)
    deadline=time.monotonic()+35
    while time.monotonic()<deadline:
        try:
            after=info()
            if after['mac_hex']==before['mac_hex'] and after['partition']!=before['partition']:
                print(json.dumps({'verified_reboot':True,'before':before,'after':after,'uploaded_sha256':digest,'bytes':len(image)}));return
        except Exception:pass
        time.sleep(.5)
    raise RuntimeError('Upload accepted, but reboot into the other OTA slot was not observed')
if __name__=='__main__':main()
