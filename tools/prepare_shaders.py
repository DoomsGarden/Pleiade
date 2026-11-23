"""Regenerate the WebGL shader bundle and matching GLES test inputs."""

import re
import json
import runpy
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / "dist/assets"
shader = (root / "tools/desktop-source/shaders.hpp").read_text()
lake_waves=re.search(r'inline const char \*lakeWaves = R"GLSL\((.*?)\)GLSL";',shader,re.S).group(1)
foam_shading=re.search(r'inline const char \*foamShading = R"GLSL\((.*?)\)GLSL";',shader,re.S).group(1)
shaders={}
for name in ['geometry','shadow','material','fullscreen','composite']:
 s=re.search(r'inline const char \*'+name+r' = R"GLSL\((.*?)\)GLSL";',shader,re.S).group(1)
 s=s.replace('#version 430 core','#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\nprecision highp sampler2DArray;')
 # Desktop GLSL permits implicit int/float promotion; GLSL ES does not.
 if name!='fullscreen':
  held=[]
  def protect(m):
   held.append(m.group(0));return '@@H'+str(len(held)-1)+'@@'
  s=re.sub(r'\b(?:\d+\.?\d*|\.\d+)[eE][+-]?\d+|#version[^\n]*|layout\([^)]*\)|for\s*\([^)]*\)|\[\d+\]|textureSize\([^)]*\)|u(?:Shadows|Natural|Materials|SurfaceMaps|Wet|VisibilityEnabled|Water|WaterSSR|Contact|AA)\s*[!=<>]+\s*\d+',protect,s)
  s=re.sub(r'(?<![\w.])\d+(?![\w.])',lambda m:m.group(0)+'.0',s)
  for i,value in enumerate(held):s=s.replace('@@H'+str(i)+'@@',value)
 else:
  s=s.replace('p*2-1','p*2.0-1.0')
 s=s.replace('// INCLUDE_LAKE_WAVES',lake_waves).replace('// INCLUDE_WATER_FOAM',foam_shading)
 shaders[name]=s.replace('WEB_SHORE','true')
material=shaders['material'];a=material.index('float visibility(vec3 n){');b=material.index('\nvec3 sky',a)
shaders['material']=material[:a]+(Path(__file__).parent/'shaders/shadow_filter.glsl').read_text()+material[b:]
(out/'shaders.json').write_text(json.dumps(shaders))

runpy.run_path(str(root / "tools/apply_celestial.py"),init_globals={"lake_waves":lake_waves})
