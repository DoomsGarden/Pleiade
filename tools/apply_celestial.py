from pathlib import Path
import json
p=Path(__file__).resolve().parents[1]; path=p/'dist/assets/shaders.json';s=json.loads(path.read_text()); common=(p/'tools/shaders/celestial.glsl').read_text()
cloud_end=common.index('vec3 celestialSky(')
cloud_prefix=common[:cloud_end]
depth=(p/'tools/shaders/depth.glsl').read_text()
shader_dir=p/'tools/shaders'
water_profile=(shader_dir/'water_profile.glsl').read_text()
s['cloud']='#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\nin vec2 uv;out vec4 frag;\n'+cloud_prefix+"\nvoid main(){float lon=uv.x*6.28318530718,lat=(uv.y-.5)*3.14159265359;vec3 ray=vec3(cos(lat)*cos(lon),sin(lat),cos(lat)*sin(lon));frag=cloudVolume(uSkyObserver,ray,cloudReach);}"
common=cloud_prefix+(p/'tools/shaders/cloud_cache.glsl').read_text()+common[cloud_end:]
# Run after prepare_assets.py, which produces the base shaders.
for name in ['material','composite']:
 t=s[name];at=t.index('void main') if name=='material' else t.index('vec3 toView')
 material_light="""uniform vec3 uLightColor;
uniform float uSkyAmbient;
uniform float uDaylight;
vec3 atmosphereFog(vec3 d){float h=max(d.y,0.0);return mix(mix(vec3(.006,.009,.018),vec3(.0007,.0015,.004),sqrt(h)),mix(vec3(.38,.49,.60),vec3(.045,.18,.42),sqrt(h)),uDaylight);}
"""
 shore_material="""uniform int uShoreWaterMode;
float shoreSnowExposure(vec3 position){
 if(uShoreWaterMode<0)return 1.0;
 bool lake=uShoreWaterMode==0;
 // These edges match the water mesh in dist/watershed.js.
 if(lake?shoreCoordinates(position.xz,true).x>3.5:abs(position.x-channelCenter(position.z))>channelWaterExtent(position.z,uShoreWaterMode))return 1.0;
 // Snow buildup follows the wave envelope rather than appearing between crests.
 float crest=uWaveStrength*(lake?.091:.043);
 return smoothstep(crest,crest+.12,position.y-waterLevel(position.z,uShoreWaterMode));
}
float channelWetness(vec3 position){
 float pool=uShoreWaterMode==2?plungePool(position.z):0.0;
 float across=abs(position.x-channelCenter(position.z));
 float edge=channelWaterExtent(position.z,uShoreWaterMode);
 float edgeWet=1.0-smoothstep(edge-.18,edge+.14+.60*pool,across);
 float above=position.y-waterLevel(position.z,uShoreWaterMode);
 float spray=1.0-smoothstep(.035,.24+.42*pool,above);
 return edgeWet*spray;
}
"""
 t=t[:at]+(material_light+water_profile+shore_material if name=='material' else common)+'\n'+t[at:]
 if name=='material':
  t=t.replace('const vec3 lightDirection=vec3(-.4562,.7603,.4562);','uniform vec3 lightDirection;')
  t=t.replace('vec3(.074,.082,.067),sky(reflectRay)*2.05', 'vec3(.074,.082,.067)*mix(.12,1.0,uDaylight),atmosphereFog(reflectRay)*mix(.82,2.05,uDaylight)')
  t=t.replace('vec3(4.4,3.95,3.2)','uLightColor').replace('base*vec3(.35,.40,.20)','base*uLightColor*vec3(.08,.10,.06)')
  t=t.replace('float fog=1.0-exp','ambient*=uSkyAmbient;\n float fog=1.0-exp').replace('direct=mix(direct,vec3(.29,.36,.42),fog)','direct=mix(direct,atmosphereFog(normalize(worldPosition-uEye)),fog)')
  t=t.replace(' bool ground=',""" if(uShoreWaterMode>0){
  bool bank=surf.z>3.5&&surf.z<4.5;
  bool rock=(surf.z>8.5&&surf.z<9.5)||abs(layer-11.0)<.5;
  if(bank||rock){
   float wet=channelWetness(worldPosition);
   if(wet>0.0){
    float texture=mix(.72,1.0,noise2(worldPosition.xz*1.8+worldPosition.y*.76));
    wet*=texture;
    base*=mix(1.0,rock?.63:.53,wet);
    rough=mix(rough,rock?.30:.43,wet);
   }
  }
 }
 bool ground=""")
  t=t.replace('base=mix(base,vec3(.63,.70,.74),snow);','if(snow>0.0)snow*=shoreSnowExposure(worldPosition);\n  base=mix(base,vec3(.63,.70,.74),snow);')
 else:
  t=t.replace('vec3 reflection=mix(vec3(.29,.36,.42),vec3(.095,.20,.33),clamp(reflected.y,0.0,1.0));','vec3 reflection=cloudSky(celestialSky(reflected),point,reflected);')
  t=t.replace('vec3(.018,.085,.075)*(1.0-transmission)','vec3(.018,.085,.075)*(1.0-transmission)*uSkyAmbient')
  t=t.replace('v+vec3(-.4562,.7603,.4562)','v+uSkyLightDirection').replace('vec3(4.4,3.95,3.2)','uLightColor')
 if name=='composite':
  t=t.replace('uniform float uExposure;', 'uniform float uExposure;\nuniform float uTurbidity;')
  t=t.replace('float z=(2.0*.1*250.0)/(250.0+.1-(depth*2.0-1.0)*(250.0-.1));',
              'float z=cameraDepth(depth);')
  t=t.replace('vec3 viewPosition(',depth+'\nvec3 viewPosition(')
  t=t.replace('(-.12-start.z)', '(-uDepthRange.x-.02-start.z)').replace('start.z>=-.1', 'start.z>=-uDepthRange.x')
  t=t.replace('vec3 transmission=exp(-vec3(.65,.27,.18)*waterDepth);', 'float waterPath=min(30.0,waterDepth/max(dot(n,v),.12));\n vec3 transmission=exp(-(vec3(.65,.27,.18)+vec3(uTurbidity))*waterPath);')
  # Reject sky neighbours and depth discontinuities when smoothing opaque geometry.
  at=t.index('vec3 mapped(')
  t=t[:at]+"""vec3 mapRadiance(vec3 linear){return pow(aces(linear*uExposure),vec3(1.0/2.2));}
vec3 mapped(vec2 p,float contact,vec4 cloud){
 vec3 surface=texture(uDirect,p).rgb+texture(uAmbient,p).rgb*contact;
 return mapRadiance(cloud.rgb+surface*cloud.a);
}
vec3 opaqueNeighbour(vec2 p,float centreDepth,vec3 centre,float contact,vec4 cloud){
 float depth=texture(uDepth,p).r;
 if(depth>=.999999)return centre;
 float z=-viewPosition(p,depth).z;
 float cz=-viewPosition(uv,centreDepth).z;
 if(abs(z-cz)>max(.03,.015*cz))return centre;
 return mapped(p,contact,cloud);
}
void main(){
 float depth=texture(uDepth,uv).r;
 vec3 ray=normalize(uCameraForward+uCameraRight*((uv.x*2.0-1.0)/uProjectionScale.x)+uCameraUp*((uv.y*2.0-1.0)/uProjectionScale.y));
 vec3 c;
 if(depth>=.999999){
 c=mapRadiance(cloudSky(celestialSky(ray),uCameraEye,ray));
 }else{
 vec4 cloud=cloudToSurface(uCameraEye,ray,length(viewPosition(uv,depth)));
 float contact=ambientContact(uv);c=mapped(uv,contact,cloud);
 // Use the centre ray for the antialiasing taps. They only filter nearby surfaces.
 if(uAA!=0){vec2 px=1.0/vec2(textureSize(uDepth,0));
 vec3 avg=(opaqueNeighbour(uv+vec2(px.x,0),depth,c,contact,cloud)+opaqueNeighbour(uv-vec2(px.x,0),depth,c,contact,cloud)+opaqueNeighbour(uv+vec2(0,px.y),depth,c,contact,cloud)+opaqueNeighbour(uv-vec2(0,px.y),depth,c,contact,cloud))*.25;
 c=mix(c,avg,.45*smoothstep(.045,.22,length(c-avg)));}
 }
 vec3 waterColor;float waterCoverage,waterDistance;
 if(waterSurface(uv,waterColor,waterCoverage,waterDistance)){
  vec4 cloud=cloudToSurface(uCameraEye,ray,waterDistance);
  c=mix(c,mapRadiance(cloud.rgb+waterColor*cloud.a),waterCoverage);
 }
 frag=vec4(c,1.0);
}
"""

 s[name]=t
waves=water_profile+(shader_dir/'waves.glsl').read_text()
webgl='#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n'
s['waterVertex']=webgl+lake_waves+waves+(shader_dir/'water_vertex.glsl').read_text()
s['depthOnly']=webgl+'void main(){}\n'
tone_map=s['composite'][s['composite'].index('vec3 aces('):s['composite'].index('vec3 mapRadiance(')]
s['glass']=webgl+common+tone_map+depth+(shader_dir/'glass.glsl').read_text()
s['weatherVertex']=webgl+(shader_dir/'weather_vertex.glsl').read_text()
s['weather']=webgl+(shader_dir/'weather.glsl').read_text()
t=s['composite']
a=t.index('float waterHeight(');b=t.index('vec3 fromView',a)
common_waves=waves.replace('uniform float uWaterTime;','').replace('uniform float uWaveStrength;','')
t=t[:a]+common_waves+'\nfloat waterHeight(vec2 p){return waterLevel(p.y,uWaterMode)+waveField(p).x;}\n'+t[b:]
a=t.index('bool waterSurface(');b=t.index('vec3 aces(',a)
s['composite']=t[:a]+(shader_dir/'watershed.glsl').read_text()+'\n'+t[b:]
s['composite']=s['composite'].replace(' vec3 waterColor;float waterCoverage,waterDistance;', '''
 if(uUnderwater!=0&&depth<.999999){
  float fog=1.0-exp(viewPosition(uv,depth).z*.155);
  c=mix(c,vec3(.046,.185,.218)*mix(.33,1.0,uDaylight),fog);
 }
 vec3 waterColor;float waterCoverage,waterDistance;''')
for name,t in s.items():(p/'tests'/f'{name}.glsl').write_text(t)
path.write_text(json.dumps(s))
