"""Refined edge-lit alternative; keeps the selected solid icon untouched."""
from pathlib import Path
import numpy as np
import struct,zlib
root=Path(__file__).resolve().parents[1]/'assets/branding'
root.mkdir(exist_ok=True)
start=[142,256];tip=[404,256]
upper=np.array([start,[142,114],tip],float)
lower=np.array([start,[142,398],tip],float)
x=np.linspace(142,404,133);u=(x-142)/262
y=256-41*np.sin(np.pi*u)**2*np.sin(4*np.pi*u)
middle=np.vstack([[0,256],np.column_stack([x,y]),[512,256]])
paths=[upper,lower,middle]
points=lambda p:' '.join(f'{x:.2f},{y:.2f}' for x,y in p)
shapes='\n'.join(f'    <polyline points="{points(p)}"/>' for p in paths)
svg=f'''<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
  <title>HYPNOTIC refined edge-glow icon</title>
  <desc>Crisp cyan edge light around a dark teal play and waveform mark, with restrained bloom.</desc>
  <defs>
    <filter id="edge-light" x="-30%" y="-30%" width="160%" height="160%" color-interpolation-filters="sRGB">
      <feMorphology in="SourceAlpha" operator="erode" radius="1.3" result="inner"/>
      <feComposite in="SourceAlpha" in2="inner" operator="out" result="rim"/>
      <feGaussianBlur in="rim" stdDeviation="2.2" result="bloom"/>
      <feFlood flood-color="#14dbfa" flood-opacity=".6" result="light"/>
      <feComposite in="light" in2="bloom" operator="in" result="halo"/>
      <feFlood flood-color="#07323d" result="body-color"/>
      <feComposite in="body-color" in2="SourceAlpha" operator="in" result="body"/>
      <feFlood flood-color="#36e7ff" result="rim-color"/>
      <feComposite in="rim-color" in2="rim" operator="in" result="edge"/>
      <feMerge><feMergeNode in="body"/><feMergeNode in="halo"/><feMergeNode in="edge"/></feMerge>
    </filter>
    <g id="mark" fill="none" stroke="white" stroke-width="12" stroke-linecap="round" stroke-linejoin="round">
{shapes}
    </g>
  </defs>
  <rect width="512" height="512" fill="black"/>
  <use href="#mark" filter="url(#edge-light)"/>
</svg>
'''
(root/'hypnotic-edge-glow.svg').write_text(svg,encoding='utf-8')
ss=2;size=512*ss
xx,yy=np.meshgrid((np.arange(size,dtype=np.float32)+.5)/ss,(np.arange(size,dtype=np.float32)+.5)/ss)
best=np.full(xx.shape,1e6,np.float32)
for path in paths:
    for a,b in zip(path[:-1],path[1:]):
        dx,dy=b-a;t=np.clip(((xx-a[0])*dx+(yy-a[1])*dy)/(dx*dx+dy*dy),0,1)
        np.minimum(best,(xx-a[0]-t*dx)**2+(yy-a[1]-t*dy)**2,out=best)
edge=np.sqrt(best)-6
coverage=np.clip(.5-edge*ss,0,1)
rim=np.clip(.5+(.85-np.abs(edge+.25))*ss,0,1)
halo=.36*np.exp(-edge*edge/(2*2.6**2))
pixels=coverage[:,:,None]*np.array([7,43,52])+halo[:,:,None]*np.array([12,150,180])
pixels=pixels*(1-rim[:,:,None])+rim[:,:,None]*np.array([54,231,255])
pixels=np.rint(np.clip(pixels,0,255).reshape(512,ss,512,ss,3).mean(axis=(1,3))).astype(np.uint8)
def chunk(kind,data):return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',512,512,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+row.tobytes() for row in pixels),9))+chunk(b'IEND',b'')
out=root/'hypnotic-edge-glow.png';out.write_bytes(png);print(out)
