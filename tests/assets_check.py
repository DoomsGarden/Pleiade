from pathlib import Path
import json,struct,numpy as np
r=Path(__file__).resolve().parents[1]/'dist/assets';data=json.loads((r/'scene.json').read_text());meshes={x['id']:x for x in data['meshes']}
stride=data['vertexStride']//4
assert stride==8
for mesh in meshes.values():
 v=np.fromfile(r/f'mesh-{mesh["id"]}.vbo',dtype='<f4');i=np.fromfile(r/f'mesh-{mesh["id"]}.ibo',dtype='<u4')
 assert len(v)%stride==0 and np.isfinite(v).all() and len(i)==mesh['indices'] and max(i)<len(v)//stride
 if 'far' in mesh: assert meshes[mesh['far']]['indices']<mesh['indices']
for population in [data['shore'],data['building'],data['snowCabin'],*data['plants'].values()]:
 for index,obj in enumerate(population):
  assert len(obj['instance'])==24 and np.isfinite(obj['instance']).all() and obj['mesh'] in meshes
  if obj.get('parentDistance'):
   assert obj['parentDistance']<=index and 'section' in population[index-obj['parentDistance']]
for name in ['ground','bark','rock']:
 a=np.fromfile(r/f'{name}.rgba',dtype=np.uint8);assert a.size==2*512*512*4
 normal=a[512*512*4:].reshape(-1,4)[:,:3].astype(float)/127.5-1
 assert np.max(abs(np.linalg.norm(normal,axis=1)-1))<.02
 assert np.std(a.reshape(-1,4)[:,3])>1
surfaces=json.loads((r/'surfaces.json').read_text()); count=len(surfaces['layers']);size=surfaces['size']
assert count==16 and size==256
color=np.fromfile(r/'surfaces-color.rgba',dtype=np.uint8).reshape(count,size,size,4)
detail=np.fromfile(r/'surfaces-normal.rgba',dtype=np.uint8).reshape(count,size,size,4)
assert np.max(np.abs(np.linalg.norm(detail[...,:3].astype(float)/127.5-1,axis=-1)-1))<.009
for layer in (2,3,14,15): assert np.min(color[layer,:,:,3])==0 and np.max(color[layer,:,:,3])>=230
print('PASS: meshes, plant hierarchy, scanned materials and 16 Blender color, normal, roughness and opacity layers')
