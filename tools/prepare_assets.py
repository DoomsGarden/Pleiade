from pathlib import Path
import sys,re,json,hashlib
import numpy as np
from PIL import Image
root=Path(sys.argv[1]);out=Path(__file__).resolve().parents[1]/'dist/assets'
shader=(Path(__file__).parent/'desktop-source/shaders.hpp').read_text()
shaders={}
for name in ['geometry','shadow','material','fullscreen','composite']:
 s=re.search(r'inline const char \*'+name+r' = R"GLSL\((.*?)\)GLSL";',shader,re.S).group(1)
 s=s.replace('#version 430 core','#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;')
 # Desktop GLSL permits implicit int/float promotion; GLSL ES does not.
 if name!='fullscreen':
  held=[]
  def protect(m):
   held.append(m.group(0));return '@@H'+str(len(held)-1)+'@@'
  s=re.sub(r'\b(?:\d+\.?\d*|\.\d+)[eE][+-]?\d+|#version[^\n]*|layout\([^)]*\)|for\s*\([^)]*\)|\[\d+\]|textureSize\([^)]*\)|u(?:Shadows|Natural|Materials|Wet|VisibilityEnabled|Water|WaterSSR|Contact|AA)\s*[!=<>]+\s*\d+',protect,s)
  s=re.sub(r'(?<![\w.])\d+(?![\w.])',lambda m:m.group(0)+'.0',s)
  for i,value in enumerate(held):s=s.replace('@@H'+str(i)+'@@',value)
 else:
  s=s.replace('p*2-1','p*2.0-1.0')
 shaders[name]=s
material=shaders['material'];a=material.index('float visibility(vec3 n){');b=material.index('\nvec3 sky',a)
shaders['material']=material[:a]+(Path(__file__).parent/'shaders/shadow_filter.glsl').read_text()+material[b:]
(out/'shaders.json').write_text(json.dumps(shaders))
for name in ['ground','bark','rock']:
 data=(root/f'assets/runtime/{name}.pltex').read_bytes(); w=int.from_bytes(data[8:12],'little'); h=int.from_bytes(data[12:16],'little');packed=[]
 for i in range(2):
  a=np.frombuffer(data[16+i*w*h*4:16+(i+1)*w*h*4],dtype=np.uint8).reshape(h,w,4)
  # Resize channels independently: height/roughness must not premultiply RGB.
  resized=np.stack([np.asarray(Image.fromarray(a[:,:,c]).resize((512,512),Image.Resampling.BOX)) for c in range(4)],axis=2).copy()
  if i==1:
   n=resized[:,:,:3].astype(float)/127.5-1;n/=np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-6);resized[:,:,:3]=np.clip((n+1)*127.5,0,255).astype(np.uint8)
  packed.append(resized.tobytes())
 (out/f'{name}.rgba').write_bytes(b''.join(packed))
manifest={'desktop_base':'0.003.0.17','viewer_version':'0.000.00.009','texture_size':512,'material_sources':json.loads((root/'assets/materials.json').read_text()),'desktop_shader_sha256':hashlib.sha256(shader.encode()).hexdigest(),'limits':['Escenas estáticas exportadas: no es el motor de juego completo.','Texturas 512²; mapas PC 1024².','Sin streaming, FoW ni selección de capas por distancia.','Reflejos SSR experimentales, desactivados inicialmente.','Sombras de contacto heredadas de PC: artefactos pendientes.','Solo cámara exterior; agua sin simulación de fluidos.']}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False))

import runpy
runpy.run_path(str(Path(__file__).parent/"apply_celestial.py"))
