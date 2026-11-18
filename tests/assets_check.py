from pathlib import Path
import json,struct,numpy as np
r=Path(__file__).resolve().parents[1]/'dist/assets';data=json.loads((r/'scene.json').read_text());meshes={x['id']:x for x in data['meshes']}
for name in ['shore','building']:
 for obj in data[name]:
  assert len(obj['instance'])==24 and np.isfinite(obj['instance']).all()
  mesh=meshes[obj['mesh']];v=np.fromfile(r/f'mesh-{mesh["id"]}.vbo',dtype='<f4');i=np.fromfile(r/f'mesh-{mesh["id"]}.ibo',dtype='<u4')
  assert len(v)%6==0 and np.isfinite(v).all() and len(i)==mesh['indices'] and max(i)<len(v)//6
for name in ['ground','bark','rock']:
 a=np.fromfile(r/f'{name}.rgba',dtype=np.uint8);assert a.size==2*512*512*4
 normal=a[512*512*4:].reshape(-1,4)[:,:3].astype(float)/127.5-1
 assert np.max(abs(np.linalg.norm(normal,axis=1)-1))<.02
 assert np.std(a.reshape(-1,4)[:,3])>1
print('PASS: scene transforms, mesh bounds/indices, 6 texture payloads and normalized normal maps')
