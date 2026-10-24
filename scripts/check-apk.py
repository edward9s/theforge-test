import struct,sys,zipfile
with zipfile.ZipFile(sys.argv[1]) as apk:
 names=set(apk.namelist());required={'AndroidManifest.xml','lib/arm64-v8a/libmain.so','assets/PathStatement.txt','assets/forest/environment.tex','assets/forest/irradiance.bin','assets/GPUCfg/gpu.data','assets/GPUCfg/gpu.cfg'}
 required|={f'assets/shaders/{s}.spv' for s in ['scene.vert','scene.frag','shadow.vert','shadow.frag','effects.frag','fullscreen.vert','post.frag']}
 if required-names:raise RuntimeError(f'Missing APK entries: {required-names}')
 data=apk.read('lib/arm64-v8a/libmain.so')
 if data[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<H',data,18)[0]!=183:raise RuntimeError('Expected ELF64 AArch64')
 phoff=struct.unpack_from('<Q',data,32)[0];phsize,phnum=struct.unpack_from('<HH',data,54)
 for i in range(phnum):
  kind,flags,offset,vaddr,paddr,filesz,memsz,align=struct.unpack_from('<II6Q',data,phoff+i*phsize)
  if kind==1 and (align<16384 or (vaddr-offset)%16384):raise RuntimeError('ELF load segment is not 16 KiB aligned')
print('APK ABI, ELF alignment and required assets validated')
