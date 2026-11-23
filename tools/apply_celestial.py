from pathlib import Path
import json
p=Path(__file__).resolve().parents[1]; path=p/'dist/assets/shaders.json';s=json.loads(path.read_text()); common=(p/'tools/shaders/celestial.glsl').read_text()
cloud_prefix=common[:common.index('vec3 cloudSky(')]
s['cloud']='#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\nin vec2 uv;out vec4 frag;\n'+cloud_prefix+"\nvoid main(){float lon=uv.x*6.28318530718,lat=(uv.y-.5)*3.14159265359;vec3 ray=vec3(cos(lat)*cos(lon),sin(lat),cos(lat)*sin(lon));frag=cloudVolume(uSkyObserver,ray);}"
a=common.index('vec3 cloudSky(');b=common.index('vec3 celestialSky(',a)
common=common[:a]+(p/'tools/shaders/cloud_cache.glsl').read_text()+common[b:]
# Run after prepare_assets.py, which produces the base shaders.
for name in ['material','composite']:
 t=s[name];at=t.index('void main') if name=='material' else t.index('vec3 toView')
 material_light="""uniform vec3 uLightColor;
uniform float uSkyAmbient;
uniform float uDaylight;
vec3 atmosphereFog(vec3 d){float h=max(d.y,0.0);return mix(mix(vec3(.006,.009,.018),vec3(.0007,.0015,.004),sqrt(h)),mix(vec3(.38,.49,.60),vec3(.045,.18,.42),sqrt(h)),uDaylight);}
"""
 t=t[:at]+(material_light if name=='material' else common)+'\n'+t[at:]
 if name=='material':
  t=t.replace('const vec3 lightDirection=vec3(-.4562,.7603,.4562);','uniform vec3 lightDirection;')
  t=t.replace('vec3(.074,.082,.067),sky(reflectRay)*2.05', 'vec3(.074,.082,.067)*mix(.12,1.0,uDaylight),atmosphereFog(reflectRay)*mix(.82,2.05,uDaylight)')
  t=t.replace('vec3(4.4,3.95,3.2)','uLightColor').replace('base*vec3(.35,.40,.20)','base*uLightColor*vec3(.08,.10,.06)')
  t=t.replace('float fog=1.0-exp','ambient*=uSkyAmbient;\n float fog=1.0-exp').replace('direct=mix(direct,vec3(.29,.36,.42),fog)','direct=mix(direct,atmosphereFog(normalize(worldPosition-uEye)),fog)')
 else:
  t=t.replace('vec3 reflection=mix(vec3(.29,.36,.42),vec3(.095,.20,.33),clamp(reflected.y,0.0,1.0));','vec3 reflection=cloudSky(celestialSky(reflected),point,reflected);')
  t=t.replace('vec3(.018,.085,.075)*(1.0-transmission)','vec3(.018,.085,.075)*(1.0-transmission)*uSkyAmbient')
  t=t.replace('v+vec3(-.4562,.7603,.4562)','v+uSkyLightDirection').replace('vec3(4.4,3.95,3.2)','uLightColor')
  t=t.replace('c=mix(vec3(.54,.63,.69),vec3(.095,.20,.33),smoothstep(0.0,1.0,p.y))','c=celestialSky(uCameraForward+uCameraRight*((p.x*2.0-1.0)/uProjectionScale.x)+uCameraUp*((p.y*2.0-1.0)/uProjectionScale.y))')
 if name=='composite':
  t=t.replace('uniform float uExposure;', 'uniform float uExposure;\nuniform float uTurbidity;\nuniform int uOrthographic;')
  t=t.replace('vec3 viewPosition(vec2 p,float depth){', 'vec3 viewPosition(vec2 p,float depth){\n if(uOrthographic!=0)return vec3((p*2.0-1.0)/uProjectionScale,-mix(.1,250.0,depth));')
  t=t.replace('2.0*max(-origin.z,.1)', '2.0*(uOrthographic!=0?1.0:max(-origin.z,.1))')
  t=t.replace('vec3 transmission=exp(-vec3(.65,.27,.18)*waterDepth);', 'float waterPath=min(30.0,waterDepth/max(dot(n,v),.12));\n vec3 transmission=exp(-(vec3(.65,.27,.18)+vec3(uTurbidity))*waterPath);')
  # Reject sky neighbours and depth discontinuities when smoothing opaque geometry.
  at=t.index('void main(){',t.index('vec3 mapped('))
  t=t[:at]+"""vec3 opaqueNeighbour(vec2 p,float centreDepth,vec3 centre,float contact){
 float depth=texture(uDepth,p).r;
 if(depth>=.999999)return centre;
 float z=-viewPosition(p,depth).z;
 float cz=-viewPosition(uv,centreDepth).z;
 if(abs(z-cz)>max(.03,.015*cz))return centre;
 return mapped(p,contact);
}
void main(){
 float depth=texture(uDepth,uv).r;
 vec3 c;
 if(depth>=.999999){
 vec3 ray=uOrthographic!=0?uCameraForward:normalize(uCameraForward+uCameraRight*((uv.x*2.0-1.0)/uProjectionScale.x)+uCameraUp*((uv.y*2.0-1.0)/uProjectionScale.y));
 c=pow(aces(cloudSky(celestialSky(ray),uCameraEye,ray)*uExposure),vec3(1.0/2.2));
 }else{
 float contact=ambientContact(uv);c=mapped(uv,contact);
 if(uAA!=0){vec2 px=1.0/vec2(textureSize(uDepth,0));
 vec3 avg=(opaqueNeighbour(uv+vec2(px.x,0),depth,c,contact)+opaqueNeighbour(uv-vec2(px.x,0),depth,c,contact)+opaqueNeighbour(uv+vec2(0,px.y),depth,c,contact)+opaqueNeighbour(uv-vec2(0,px.y),depth,c,contact))*.25;
 c=mix(c,avg,.45*smoothstep(.045,.22,length(c-avg)));}
 }
 vec3 waterColor;float waterCoverage;
 if(waterSurface(uv,waterColor,waterCoverage))c=mix(c,pow(aces(waterColor*uExposure),vec3(1.0/2.2)),waterCoverage);
 frag=vec4(c,1.0);
}
"""

 s[name]=t
shader_dir=p/'tools/shaders'
waves=(shader_dir/'waves.glsl').read_text()
webgl='#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n'
s['waterVertex']=webgl+lake_waves+waves+(shader_dir/'water_vertex.glsl').read_text()
s['depthOnly']=webgl+'void main(){}\n'
tone_map=s['composite'][s['composite'].index('vec3 aces('):s['composite'].index('vec3 mapped(')]
s['glass']=webgl+common+tone_map+(shader_dir/'glass.glsl').read_text()
s['weatherVertex']=webgl+(shader_dir/'weather_vertex.glsl').read_text()
s['weather']=webgl+(shader_dir/'weather.glsl').read_text()
t=s['composite']
a=t.index('float waterHeight(');b=t.index('vec3 fromView',a)
common_waves=waves.replace('uniform float uWaterTime;','').replace('uniform float uWaveStrength;','')
t=t[:a]+common_waves+'\nfloat waterHeight(vec2 p){return waterLevel(p.y)+waveField(p).x;}\n'+t[b:]
a=t.index('bool waterSurface(');b=t.index('vec3 aces(',a)
s['composite']=t[:a]+(shader_dir/'watershed.glsl').read_text()+'\n'+t[b:]
s['composite']=s['composite'].replace(' vec3 waterColor;float waterCoverage;', '''
 if(uUnderwater!=0&&depth<.999999){
  float fog=1.0-exp(viewPosition(uv,depth).z*.155);
  c=mix(c,vec3(.046,.185,.218)*mix(.33,1.0,uDaylight),fog);
 }
 vec3 waterColor;float waterCoverage;''')
for name,t in s.items():(p/'tests'/f'{name}.glsl').write_text(t)
path.write_text(json.dumps(s))
