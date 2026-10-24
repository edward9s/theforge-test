"""Cook the reference's hash-pinned CC0 textures and HDR environment."""
import hashlib,json,math,struct,subprocess,tempfile,shutil
from pathlib import Path
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def download(e):
 p=ROOT/'.deps/scene'/(e['name']+Path(e['url']).suffix);p.parent.mkdir(parents=True,exist_ok=True)
 if p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()==e['sha256']:return p
 tmp=p.with_suffix('.partial')
 try:
  subprocess.run(['curl','-fLsS','--retry','3','--connect-timeout','20','--max-time','300',e['url'],'-o',str(tmp)],check=True)
  if hashlib.sha256(tmp.read_bytes()).hexdigest()!=e['sha256']:raise ValueError('Checksum mismatch: '+e['name'])
  tmp.replace(p)
 finally:tmp.unlink(missing_ok=True)
 return p

def hdr(path):
 with path.open('rb') as f:
  if not f.readline().startswith(b'#?RADIANCE'):raise ValueError('Expected Radiance HDR')
  while f.readline().strip():pass
  ay,h,ax,w=f.readline().split()
  if (ay,ax)!=(b'-Y',b'+X'):raise ValueError('Unexpected HDR orientation')
  h,w=int(h),int(w);pixels=np.empty((h,w,4),dtype=np.uint8)
  for y in range(h):
   if f.read(4)!=bytes([2,2,w>>8,w&255]):raise ValueError('Expected HDR RLE')
   for c in range(4):
    x=0
    while x<w:
     count=f.read(1)[0]
     if count==0:raise ValueError('Invalid HDR run')
     if count>128:n=count-128;pixels[y,x:x+n,c]=f.read(1)[0]
     else:n=count;pixels[y,x:x+n,c]=np.frombuffer(f.read(n),dtype=np.uint8)
     x+=n
     if x>w:raise ValueError('HDR overflow')
  return pixels[:,:,:3].astype(np.float32)*np.exp2(pixels[:,:,3:4].astype(np.float32)-136)

def directions(w,h):
 theta=(np.arange(h,dtype=np.float32)+.5)*np.pi/h;phi=((np.arange(w,dtype=np.float32)+.5)/w-.5)*2*np.pi
 y,x=np.meshgrid(theta,phi,indexing='ij');return np.stack([np.sin(y)*np.cos(x),np.cos(y),np.sin(y)*np.sin(x)],-1)
def sample(im,d):
 h,w=im.shape[:2];x=((np.arctan2(d[...,2],d[...,0])/(2*np.pi)+.5)*w-.5)%w;y=np.clip(np.arccos(np.clip(d[...,1],-1,1))/np.pi*h-.5,0,h-1)
 ix,iy=np.floor(x).astype(int),np.floor(y).astype(int);fx,fy=(x-ix)[...,None],(y-iy)[...,None]
 top=im[iy,ix]*(1-fx)+im[iy,(ix+1)%w]*fx;bottom=im[np.minimum(iy+1,h-1),ix]*(1-fx)+im[np.minimum(iy+1,h-1),(ix+1)%w]*fx
 return top*(1-fy)+bottom*fy

def environment(im,out):
 h,w=im.shape[:2];d=directions(w,h);x,y,z=d[...,0],d[...,1],d[...,2]
 basis=[.282095*np.ones_like(x),.488603*y,.488603*z,.488603*x,1.092548*x*y,1.092548*y*z,.315392*(3*z*z-1),1.092548*x*z,.546274*(x*x-y*y)]
 solid=np.sqrt(np.maximum(0,1-y*y))*(2*np.pi/w)*(np.pi/h);sh=np.zeros((9,4),dtype='<f4')
 for i,b in enumerate(basis):sh[i,:3]=np.sum(im*(b*solid)[...,None],axis=(0,1))*(np.pi if i==0 else 2*np.pi/3 if i<4 else np.pi/4)
 (out/'irradiance.bin').write_bytes(sh.tobytes());levels=[]
 for mip in range(9):
  w,h=max(1,256>>mip),max(1,128>>mip);n=directions(w,h)
  if mip==0:color=sample(im,n)
  else:
   rough=mip/8;a=rough*rough;up=np.zeros_like(n);up[...,1]=1;tangent=np.cross(up,n);tangent/=np.linalg.norm(tangent,axis=-1,keepdims=True);bitangent=np.cross(n,tangent);color=np.zeros_like(n,dtype=np.float64);weight=np.zeros((h,w,1))
   for i in range(128):
    bits=int(f'{i:032b}'[::-1],2)/2**32;phi=2*np.pi*i/128;ct=math.sqrt((1-bits)/(1+(a*a-1)*bits));st=math.sqrt(max(0,1-ct*ct));halfway=tangent*(math.cos(phi)*st)+bitangent*(math.sin(phi)*st)+n*ct;light=2*np.sum(n*halfway,axis=-1,keepdims=True)*halfway-n;nl=np.maximum(0,np.sum(n*light,axis=-1,keepdims=True));color+=sample(im,light)*nl;weight+=nl
   color/=np.maximum(weight,1e-6)
  levels.append(np.concatenate([np.clip(color,0,65000),np.ones((h,w,1))],axis=-1).astype('<f2').tobytes())
 (out/'environment.tex').write_bytes(struct.pack('<4I',0x31544646,256,128,9)+b''.join(levels))

def texture(image,out,name):
 pixels=np.asarray(image.convert('RGBA'),dtype=np.float32)/255;srgb=name.endswith('_albedo') or name=='canopy'
 if srgb:
  rgb=pixels[...,:3];pixels[...,:3]=np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4)
 levels=[]
 while True:
  packed=pixels.copy()
  if srgb:
   rgb=packed[...,:3];packed[...,:3]=np.where(rgb<=.0031308,rgb*12.92,1.055*np.maximum(rgb,0)**(1/2.4)-.055)
  levels.append(np.round(np.clip(packed,0,1)*255).astype(np.uint8).tobytes());h,w=pixels.shape[:2]
  if w==1 and h==1:break
  pixels=pixels.reshape(h//2,2,w//2,2,4).mean(axis=(1,3))
 (out/f'{name}.tex').write_bytes(struct.pack('<4I',0x31544646,1024,1024,len(levels))+b''.join(levels))

def main():
 dest=ROOT/'.generated/assets/forest';dest.parent.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(dir=dest.parent) as tmp:
  out=Path(tmp)
  for e in json.loads((ROOT/'assets/sources.json').read_text()):
   src=download(e)
   if e['name']=='environment':environment(hdr(src),out)
   else:
    with Image.open(src) as im:
     if im.size!=(1024,1024):raise ValueError('Unexpected texture dimensions')
     if 'crop' in e:im=im.crop(e['crop']).resize((1024,1024),Image.Resampling.LANCZOS)
     texture(im,out,e['name'])
   print('Prepared',e['name'],flush=True)
  if dest.exists():shutil.rmtree(dest)
  shutil.copytree(out,dest)
 assets=dest.parent;shutil.copy(ROOT/'assets/PathStatement.txt',assets/'PathStatement.txt');shutil.copy(ROOT/'assets/CREDITS.md',dest/'CREDITS.md')
 (assets/'GPUCfg').mkdir(exist_ok=True)
 shutil.copy(ROOT/'.deps/forge/Common_3/OS/Android/android_gpu.data',assets/'GPUCfg/gpu.data')
 shutil.copy(ROOT/'.deps/forge/Examples_3/Unit_Tests/src/01_Transformations/GPUCfg/gpu.cfg',assets/'GPUCfg/gpu.cfg')
if __name__=='__main__':main()
