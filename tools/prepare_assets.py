from pathlib import Path
import sys,json,hashlib
import numpy as np
from PIL import Image
root=Path(sys.argv[1]);out=Path(__file__).resolve().parents[1]/'dist/assets'
shader=(Path(__file__).parent/'desktop-source/shaders.hpp').read_text()
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
manifest={'desktop_base':'0.003.0.17','viewer_version':'0.000.00.010','texture_size':512,'material_sources':json.loads((root/'assets/materials.json').read_text()),'desktop_shader_sha256':hashlib.sha256(shader.encode()).hexdigest(),'limits':['Escenas de referencia y cinco biomas compactos por semilla.','Texturas 512²; mapas PC 1024².','Niveles de detalle por tamaño proyectado. Sin streaming de regiones ni FoW en el visor.','Reflejos SSR experimentales, desactivados inicialmente.','Sombras de contacto heredadas de PC: artefactos pendientes.','Solo cámara exterior; agua sin simulación de fluidos.']}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False))

import runpy
runpy.run_path(str(Path(__file__).parent/"prepare_shaders.py"))
