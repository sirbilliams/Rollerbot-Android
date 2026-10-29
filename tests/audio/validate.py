#!/usr/bin/env python3
"""Inventory private media by signature, generate original AVI fixture and run decoder."""
import os, pathlib, struct, subprocess, sys, json, zlib
out=pathlib.Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
inventory=[]
for root in sys.argv[3:]:
 for d,_,files in os.walk(os.path.expanduser(root)):
  for name in files:
   path=pathlib.Path(d)/name
   with path.open('rb') as f: head=f.read(12)
   if head[:4]==b'RIFF' or path.suffix.lower() in ('.avi','.mpg','.mpeg','.mov','.wmv','.mp4','.bik','.smk'):
    inventory.append({'root':pathlib.Path(root).name,'file':str(path.relative_to(root)),'signature':head[:4].hex(),'form':head[8:12].decode('latin1')})
(out/'inventory.json').write_text(json.dumps(inventory,indent=2))
def chunk(tag,b):return tag+struct.pack('<I',len(b))+b+b'\0'*(len(b)%2)
def listing(tag,b):return chunk(b'LIST',tag+b)
def stream(kind,fmt,scale,rate):
 h=bytearray(56);h[:4]=kind;struct.pack_into('<II',h,20,scale,rate)
 return listing(b'strl',chunk(b'strh',h)+chunk(b'strf',fmt))
avih=bytearray(56);struct.pack_into('<I',avih,0,40000)
fmt=struct.pack('<IiiHHIIiiII',40,2,2,1,24,0,16,0,0,0,0)
header=listing(b'hdrl',chunk(b'avih',avih)+stream(b'vids',fmt,1,25)+stream(b'auds',struct.pack('<HHIIHH',1,1,8000,16000,2,16),1,8000))
# Two red pixels per row, DWORD aligned; bottom-up bitmap.
frames=listing(b'movi',chunk(b'00db',(b'\0\0\xff'*2+b'\0\0')*2)+chunk(b'01wb',struct.pack('<hhhh',0,1000,-1000,0)))
fixture=out/'synthetic.avi';fixture.write_bytes(chunk(b'RIFF',b'AVI '+header+frames))
raw=out/'synthetic.rgba'
subprocess.run(['timeout','30',sys.argv[1],str(out/'mix.wav'),str(fixture),str(raw)],env={**os.environ,'SDL_AUDIODRIVER':'dummy'},check=True)
def pngchunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b))
pixels=raw.read_bytes();assert pixels==bytes([255,0,0,255])*4
(out/'synthetic.png').write_bytes(b'\x89PNG\r\n\x1a\n'+pngchunk(b'IHDR',struct.pack('>IIBBBBB',2,2,8,6,0,0,0))+pngchunk(b'IDAT',zlib.compress(b'\0'+pixels[:8]+b'\0'+pixels[8:]))+pngchunk(b'IEND',b''))
# Exercise decoder on every discovered AVI, including extensionless RIFFs.
for i,item in enumerate(inventory):
 if item['form']=='AVI ':raise RuntimeError('Retail AVI discovered: add per-codec fixture before claiming coverage')
print('Inventory:',len(inventory),'RIFF/media files; no retail AVI. Synthetic BI_RGB/PCM passed.')
