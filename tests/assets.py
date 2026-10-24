from pathlib import Path
import struct
import numpy as np
root=Path(__file__).resolve().parents[1]/'.generated/assets'
textures=list((root/'forest').glob('*.tex'))
assert len(textures)==11,'Missing texture'
for path in textures:
 data=path.read_bytes();magic,w,h,mips=struct.unpack_from('<4I',data)
 assert magic==0x31544646 and w>0 and h>0
 half=path.stem=='environment';offset=16
 for mip in range(mips):
  size=w*h*(8 if half else 4);assert offset+size<=len(data),path
  if half:
   values=np.frombuffer(data[offset:offset+size],dtype='<f2');assert np.isfinite(values).all() and values.min()>=0,path
  offset+=size;w=max(1,w//2);h=max(1,h//2)
 assert offset==len(data),path
sh=np.frombuffer((root/'forest/irradiance.bin').read_bytes(),dtype='<f4')
assert sh.size==36 and np.isfinite(sh).all() and np.max(sh)>0
assert len(list((root/'shaders').glob('*.spv')))==7
for path in (root/'shaders').glob('*.spv'):assert path.read_bytes()[:4]==b'\x03\x02\x23\x07',path
print('11 complete texture mip chains, irradiance SH and 7 SPIR-V shaders validated')
