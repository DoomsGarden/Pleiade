#pragma once
namespace pl::shader {
inline const char *geometry = R"GLSL(#version 430 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in mat4 aModel;
layout(location=6) in vec4 aColor;
layout(location=7) in vec4 aSurface;
layout(location=8) in vec2 aUV;
uniform mat4 uVP;
uniform float uTime;
uniform float uWind;
out vec3 rasterPosition;
out vec3 rasterLocal;
out vec2 rasterUV;
flat out vec3 objectDimensions;
flat out vec3 objectCenter;
flat out vec2 coverage;
flat out mat4 objectModel;
flat out mat3 objectNormals;
flat out float atlasView;
out vec3 worldNormal;
out vec3 color;
flat out vec3 surface;
void main(){
 vec4 p=aModel*vec4(aPosition,1);
 coverage=vec2(aSurface.y,aColor.a);objectModel=aModel;atlasView=aSurface.w;
 if(abs(aSurface.z-10.0)<.1){
   float az=mod(aSurface.w,16.0)*6.2831853/16.0,el=-1.5707963+floor(aSurface.w/16.0)*3.14159265/4.0;
   vec3 d=vec3(cos(el)*sin(az),sin(el),cos(el)*cos(az)),r=vec3(cos(az),0.0,-sin(az)),u=cross(d,r);
   p=aModel*vec4(r*aPosition.x+u*aPosition.y,1.0);
 }
 // Orthogonal rotation + nonuniform scale: inverse transpose without per-vertex inverse.
 vec3 scale2=vec3(dot(aModel[0].xyz,aModel[0].xyz),dot(aModel[1].xyz,aModel[1].xyz),dot(aModel[2].xyz,aModel[2].xyz));
 objectDimensions=sqrt(scale2);
 objectCenter=aModel[3].xyz;
 objectNormals=mat3(aModel[0].xyz/scale2.x,aModel[1].xyz/scale2.y,aModel[2].xyz/scale2.z);
 worldNormal=normalize(objectNormals*aNormal);
 if(aSurface.w>0.5 && aSurface.w<1.5 && aSurface.z<9.5){
   float x=p.x,z=p.z;
   p.y=2.0*sin(x*.04)*cos(z*.045)+.65*sin(x*.095+z*.075);
   float dx=.08*cos(x*.04)*cos(z*.045)+.06175*cos(x*.095+z*.075);
   float dz=-.09*sin(x*.04)*sin(z*.045)+.04875*cos(x*.095+z*.075);
   worldNormal=normalize(vec3(-dx,1.0,-dz));
 } else if(aSurface.w>1.5 && aSurface.z<9.5){
   float tip=aPosition.y+.5;
   p.x+=sin(uTime*1.8+p.x*.35+p.z*.2)*.16*tip*tip;
 }
 float leafLayer=aSurface.z-20.0;
 bool flexible=(leafLayer>=-.5&&leafLayer<4.5)||(leafLayer>=13.5&&leafLayer<15.5)||(leafLayer>=17.5&&leafLayer<19.5);
 if(flexible&&uWind>.0){
  float tip=clamp(aPosition.y+.5,0.0,1.0);
  vec2 phase=vec2(p.x*.51+p.z*.27,p.z*.64-p.x*.18);
  p.xz+=.038*uWind*tip*tip*sin(phase+uTime*vec2(1.63,2.21));
 }
 rasterLocal=aPosition;
 rasterUV=aUV;
 rasterPosition=p.xyz;
 color=aColor.rgb;surface=aSurface.xyz;gl_Position=uVP*p;
})GLSL";
inline const char *shadow = R"GLSL(#version 430 core
in vec2 rasterUV;
flat in vec3 surface;
flat in vec2 coverage;
uniform sampler2DArray uSurfaceColor;
uniform int uSurfaceMaps;
void main(){
 if(abs(surface.z-18.0)<.1)discard;
 if(surface.z>=19.5&&coverage.y<.9999){
   float dither=fract(52.9829189*fract(dot(floor(gl_FragCoord.xy),vec2(.06711056,.00583715))));
   if(coverage.y>=0.0?dither>=coverage.y:dither< -coverage.y)discard;
 }
 float layer=surface.z-20.0;
 bool cutout=(layer>=1.5&&layer<3.5)||(layer>=13.5&&layer<15.5)||(layer>=17.5&&layer<19.5);
 if(uSurfaceMaps!=0&&cutout&&texture(uSurfaceColor,vec3(rasterUV,layer)).a<(layer>=13.5?.28:.42))discard;
}
)GLSL";
inline const char *material = R"GLSL(#version 430 core
in vec3 rasterLocal;in vec3 rasterPosition;in vec3 worldNormal;in vec3 color;flat in vec3 surface;
in vec2 rasterUV;
flat in vec3 objectDimensions;
flat in vec3 objectCenter;
flat in vec2 coverage;
flat in float atlasView;
vec3 worldPosition,localPosition;
flat in mat3 objectNormals;
#ifdef DEPTH_IMPOSTOR
flat in mat4 objectModel;
uniform sampler2D uCanopy;uniform mat4 uVP;
#endif
layout(location=0) out vec4 outDirect;
layout(location=1) out vec4 outAmbient;
layout(location=2) out vec4 outNormal;
uniform vec3 uEye;
uniform mat4 uLightVP;
uniform mat4 uView;
uniform sampler2D uShadow;
uniform int uShadows;
uniform int uNatural;
uniform int uMaterials,uWet;uniform float uWetTime;uniform float uWaveStrength;
uniform sampler2D uGroundColor,uGroundNormal,uBarkColor,uBarkNormal,uRockColor,uRockNormal;
uniform sampler2DArray uSurfaceColor,uSurfaceNormal;
uniform int uSurfaceMaps;
uniform float uTerrainSeed;
uniform vec3 uClimate;
mat3 surfaceFrame(vec3 n,vec2 uv){
 vec3 dx=dFdx(worldPosition),dy=dFdy(worldPosition),p1=cross(dy,n),p2=cross(n,dx);
 vec2 tx=dFdx(uv),ty=dFdy(uv);
 vec3 t=p1*tx.x+p2*ty.x,b=p1*tx.y+p2*ty.y;
 float scale=inversesqrt(max(max(dot(t,t),dot(b,b)),1e-10));return mat3(t*scale,b*scale,n);
}
struct PBRSurface{vec3 base;vec3 normal;float rough;};
PBRSurface readPBR(sampler2D colorMap,sampler2D normalMap,vec2 uv,vec3 n,vec3 view,float relief){
 vec2 dx=dFdx(uv),dy=dFdy(uv);mat3 frame=surfaceFrame(n,uv);
 vec3 tangentView=transpose(frame)*view;
 if(relief>0.00001){
  vec2 stepUV=tangentView.xy/max(abs(tangentView.z),.25)*relief/8.0;
  float depth=0.0;
  for(int i=0;i<8;i++){
   float height=1.0-textureGrad(colorMap,uv,dx,dy).a;
   if(depth>=height)break;
   uv-=stepUV;depth+=.125;
  }
 }
 vec4 colorSample=textureGrad(colorMap,uv,dx,dy),normalSample=textureGrad(normalMap,uv,dx,dy);
 PBRSurface result;result.base=colorSample.rgb;result.normal=normalize(frame*(normalSample.xyz*2.0-1.0));result.rough=clamp(normalSample.a,.06,1.0);return result;
}
PBRSurface triplanarPBR(sampler2D c,sampler2D r,vec3 n,vec3 view,float meters){
 vec3 weights=pow(abs(n),vec3(4.0));weights/=max(weights.x+weights.y+weights.z,.001);
 PBRSurface x=readPBR(c,r,worldPosition.zy/meters,n,view,0.0),y=readPBR(c,r,worldPosition.xz/meters,n,view,0.0),z=readPBR(c,r,worldPosition.xy/meters,n,view,0.0);
 PBRSurface result;result.base=x.base*weights.x+y.base*weights.y+z.base*weights.z;result.normal=normalize(x.normal*weights.x+y.normal*weights.y+z.normal*weights.z);result.rough=x.rough*weights.x+y.rough*weights.y+z.rough*weights.z;return result;
}
PBRSurface bakedSurface(vec2 uv,float layer,vec3 n){
 vec3 key=vec3(uv,layer);
 vec3 base=texture(uSurfaceColor,key).rgb;
 if(uMaterials==0)return PBRSurface(base,n,.82);
 vec4 map=texture(uSurfaceNormal,key);
 vec3 detail=map.rgb*2.0-1.0;
 return PBRSurface(base,normalize(surfaceFrame(n,uv)*detail),map.a);
}
PBRSurface blendSurface(PBRSurface a,PBRSurface b,float blend){
 return PBRSurface(mix(a.base,b.base,blend),normalize(mix(a.normal,b.normal,blend)),mix(a.rough,b.rough,blend));
}
PBRSurface bakedWorldSurface(vec3 n,float layer,float meters){
 vec3 weights=pow(abs(n),vec3(4.0));weights/=max(weights.x+weights.y+weights.z,.001);
 bool verticalGrain=abs(layer-7.0)<.5&&abs(atlasView+5.0)<.1;
 PBRSurface x=bakedSurface((verticalGrain?worldPosition.yz:worldPosition.zy)/meters,layer,n);
 PBRSurface y=bakedSurface(worldPosition.xz/meters,layer,n);
 PBRSurface z=bakedSurface((verticalGrain?worldPosition.yx:worldPosition.xy)/meters,layer,n);
 return PBRSurface(x.base*weights.x+y.base*weights.y+z.base*weights.z,
   normalize(x.normal*weights.x+y.normal*weights.y+z.normal*weights.z),
   x.rough*weights.x+y.rough*weights.y+z.rough*weights.z);
}
uniform float uFogDensity;
uniform sampler2D uVisibility;uniform int uVisibilityEnabled;
const float PI=3.14159265;
const vec3 lightDirection=vec3(-.4562,.7603,.4562);
float hash(vec3 p){p=fract(p*.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
// Smooth, derivative-filtered procedural material detail, no external textures.
float noise2(vec2 p){
 vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);
 return mix(mix(hash(vec3(i,0.0)),hash(vec3(i+vec2(1.0,0.0),0.0)),f.x),
            mix(hash(vec3(i+vec2(0.0,1.0),0.0)),hash(vec3(i+1.0,0.0)),f.x),f.y);
}
float filteredNoise(vec2 p){
 float footprint=max(length(dFdx(p)),length(dFdy(p)));
 return mix(noise2(p),.5,smoothstep(.35,1.2,footprint));
}
vec2 terrainPhase(vec2 cell){
 float variant=floor(hash(vec3(cell,mod(uTerrainSeed,997.0)*.013+.12))*9.0);
 return vec2(mod(variant,3.0),floor(variant/3.0))/3.0;
}
vec3 terrainWeights(vec2 p,out vec2 a,out vec2 b,out vec2 c){
 // Skew the lattice so its zones do not read as aligned squares.
 vec2 tile=mat2(.94,.342,-.342,.94)*p*.136;
 vec2 cell=floor(tile), f=fract(tile);
 bool upper=f.x+f.y>1.0;
 a=terrainPhase(cell+vec2(upper?1.0:0.0));
 b=terrainPhase(cell+vec2(1.0,0.0)); c=terrainPhase(cell+vec2(0.0,1.0));
 vec3 weights=upper?vec3(f.x+f.y-1.0,1.0-f.y,1.0-f.x):vec3(1.0-f.x-f.y,f.x,f.y);
 weights=weights*weights*(3.0-2.0*weights);
 return weights/max(dot(weights,vec3(1.0)),.0001);
}
PBRSurface terrainSurface(vec2 position,float meters,float layer,vec3 n){
 vec2 a,b,c; vec3 w=terrainWeights(position,a,b,c);
 // All samples have the same metric UV derivatives. Only their weights change.
 vec2 uv=position/meters;
 vec2 dx=dFdx(uv),dy=dFdy(uv);
 mat3 frame=surfaceFrame(n,uv);
 vec3 color;
 vec4 detail;
 if(layer<0.0){
  color=w.x*textureGrad(uGroundColor,uv+a,dx,dy).rgb+w.y*textureGrad(uGroundColor,uv+b,dx,dy).rgb+w.z*textureGrad(uGroundColor,uv+c,dx,dy).rgb;
  detail=w.x*textureGrad(uGroundNormal,uv+a,dx,dy)+w.y*textureGrad(uGroundNormal,uv+b,dx,dy)+w.z*textureGrad(uGroundNormal,uv+c,dx,dy);
 }else{
  color=w.x*textureGrad(uSurfaceColor,vec3(uv+a,layer),dx,dy).rgb+w.y*textureGrad(uSurfaceColor,vec3(uv+b,layer),dx,dy).rgb+w.z*textureGrad(uSurfaceColor,vec3(uv+c,layer),dx,dy).rgb;
  if(uMaterials==0)return PBRSurface(color,n,.85);
  detail=w.x*textureGrad(uSurfaceNormal,vec3(uv+a,layer),dx,dy)+w.y*textureGrad(uSurfaceNormal,vec3(uv+b,layer),dx,dy)+w.z*textureGrad(uSurfaceNormal,vec3(uv+c,layer),dx,dy);
 }
 return PBRSurface(color,normalize(frame*(detail.rgb*2.0-1.0)),detail.a);
}
PBRSurface birchSurface(vec3 n){
 float t=clamp(rasterLocal.y+.5,0.0,1.0);
 // Derive the height and azimuth from the surface, independent of the bark atlas.
 vec2 axis=vec2(t*t*.18+.023*sin(PI*t*1.8)*t,-.051*sin(PI*t)*t);
 vec2 radial=rasterLocal.xz-(atlasView< -3.5?axis:vec2(0.0));
 float angle=atan(radial.y,radial.x);
 float columns=max(7.0,floor(objectDimensions.x*1.96/.089));
 float metres=t*objectDimensions.y;
 vec2 grid=vec2((angle/(2.0*PI)+.5)*columns,metres/.073);
 vec2 cell=floor(grid), f=fract(grid);
 float seed=hash(objectCenter*1.19);
 vec3 id=vec3(mod(cell.x,columns),cell.y,seed);
 vec3 random=vec3(hash(id+3.1),hash(id+17.6),hash(id+39.2));
 vec2 grain=vec2(worldPosition.x-.8*worldPosition.z,worldPosition.y);
 float band=filteredNoise(vec2(worldPosition.x+.7*worldPosition.z,worldPosition.y)*3.2);
 float paper=filteredNoise(grain*19.0);
 vec3 tone=mix(vec3(.38,.366,.329),vec3(.89,.874,.817),.28+.38*band+.26*paper);
 float peeling=filteredNoise(grain*vec2(7.8,21.0))+.18*(band-.5);
 float tornEdge=smoothstep(.54,.63,peeling)*(1.0-smoothstep(.68,.81,peeling))*smoothstep(.58,.76,band);
 tone=mix(tone,vec3(.39,.30,.245),tornEdge*.55);
 vec2 local=f-vec2(mix(.40,.60,random.x),mix(.20,.80,random.y));
 float markLength=mix(.083,.39,random.z);
 float width=mix(.025,.067,random.x);
 float wobble=local.x*(random.y-.5)*.15+.013*sin(local.x*37.0+random.x*9.0);
 vec2 shape=vec2(local.x/markLength,(local.y+wobble)/width);
 float footprint=max(fwidth(grid.y),length(fwidth(radial))*columns*.65);
 float lenticel=(1.0-smoothstep(.7,1.16+min(footprint*1.2,.75),pow(abs(shape.x),4.0)+shape.y*shape.y));
 lenticel*=smoothstep(.24,.55,hash(id+51.1)+.30*(band-.5))*(1.0-smoothstep(.33,1.1,footprint));
 tone=mix(tone,vec3(.081,.067,.053),lenticel*mix(.65,.93,random.z));
 // Dark torn patches and branch scars interrupt the otherwise pale papery bark.
 vec2 coarse=grid/vec2(5.0,8.0);
 coarse.x=grid.x*max(2.0,floor(columns/5.0))/columns;
 vec2 patchId=floor(coarse);
 float patchRank=hash(vec3(patchId,seed+.71));
 vec2 scar=fract(coarse)-vec2(.48+(.5-patchRank)*.22,.36+.30*hash(vec3(patchId,seed+9.0)));
 float edge=length(scar*vec2(1.0,2.1))+(paper-.5)*.12;
 float oldScar=(1.0-smoothstep(.072,.18,edge))*smoothstep(.74,.96,patchRank);
 tone=mix(tone,mix(vec3(.10,.081,.065),vec3(.35,.31,.256),paper),oldScar*.92);
 if(atlasView< -3.5){
  float lower=1.0-smoothstep(.045,.22,t+(band-.5)*.20);
  float cork=filteredNoise(vec2(worldPosition.x+worldPosition.z*.9,worldPosition.y)*11.5);
  tone=mix(tone,mix(vec3(.037,.029,.024),vec3(.21,.184,.147),smoothstep(.34,.65,cork)),lower);
 }
 return PBRSurface(tone,n,mix(.65,.93,oldScar));
}
PBRSurface timberSurface(vec3 n){
 vec3 axisX=normalize(objectNormals[0]),axisY=normalize(objectNormals[1]),axisZ=normalize(objectNormals[2]);
 vec3 p=rasterLocal*objectDimensions;
 vec3 an=abs(vec3(dot(n,axisX),dot(n,axisY),dot(n,axisZ)));
 if(objectDimensions.y>max(objectDimensions.x,objectDimensions.z)){p=p.yxz;an=an.yxz;}
 else if(objectDimensions.z>objectDimensions.x){p=p.zxy;an=an.zxy;}
 float seed=hash(objectCenter*.187);
 vec2 shift=vec2(seed,hash(objectCenter*.297+31.0));
 vec3 w=pow(an,vec3(9.0));w/=max(dot(w,vec3(1.0)),.001);
 PBRSurface endGrain=bakedSurface((p.yz/.47).yx+shift,7.0,n);
 PBRSurface faceA=bakedSurface(p.xz/vec2(1.46,.55)+shift,7.0,n);
 PBRSurface faceB=bakedSurface(p.xy/vec2(1.46,.55)+shift,7.0,n);
 vec2 rings=p.yz+vec2(.17,.095);
 float endTone=.85+.15*filteredNoise(vec2(length(rings)*52.0,atan(rings.y,rings.x)*2.8));
 float exposure=mix(.81,1.08,seed);
 return PBRSurface(exposure*(w.x*endGrain.base*endTone+w.y*faceA.base+w.z*faceB.base),
   normalize(w.x*endGrain.normal+w.y*faceA.normal+w.z*faceB.normal), w.x*endGrain.rough+w.y*faceA.rough+w.z*faceB.rough);
}
vec3 bumpNormal(vec3 n,float height){
 vec3 dx=dFdx(worldPosition),dy=dFdy(worldPosition);
 vec3 r1=cross(dy,n),r2=cross(n,dx);float det=dot(dx,r1);
 return normalize(abs(det)*n-sign(det)*(dFdx(height)*r1+dFdy(height)*r2));
}
float visibility(vec3 n){
 if(uShadows==0)return 1;
 vec4 p=uLightVP*vec4(worldPosition,1);vec3 q=p.xyz/p.w*.5+.5;
 // Different PCF taps see different depths on a sloping receiver. Compare
 // against the receiver plane at each actual nearest-filtered texel centre.
 // Derivatives precede the bounds branch and use geometry, not bump normals.
 vec3 dx=dFdx(q),dy=dFdy(q);
 float determinant=dx.x*dy.y-dx.y*dy.x;
 vec2 gradient=vec2(0.0);
 bool regular=abs(determinant)>1e-12;
 if(regular)gradient=vec2(dy.y*dx.z-dx.y*dy.z,dx.x*dy.z-dy.x*dx.z)/determinant;
 if(q.z<0.0||q.z>1.0||any(lessThan(q.xy,vec2(0.0)))||any(greaterThan(q.xy,vec2(1.0))))return 1.0;
 float bias=regular?.000005:max(.0004*(1.0-dot(n,lightDirection)),.00012);
 float sum=0.0;vec2 size=vec2(textureSize(uShadow,0));
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
   vec2 centre=(floor(q.xy*size)+vec2(x,y)+.5)/size;
   float receiver=q.z+dot(gradient,centre-q.xy);
   sum+=(receiver-bias<=texture(uShadow,centre).r)?1.0:0.0;
 }
 return sum/9.0;
}
vec3 sky(vec3 d){return mix(vec3(.12,.15,.18),vec3(.28,.42,.61),clamp(d.y*.5+.5,0.0,1.0));}
PBRSurface biomeSurface(vec3 n,float layer){
 float patches=filteredNoise(worldPosition.xz*.16);
 vec2 ground=worldPosition.xz;
 PBRSurface surfaceMap=terrainSurface(ground,layer<10.5?2.9:2.1,layer,n);
 if(layer<9.5){
  surfaceMap.base*=.92+.15*patches;
  surfaceMap.normal=normalize(mix(n,surfaceMap.normal,.46+.41*patches));
  return surfaceMap;
 }
 if(layer<10.5){
  float snow=smoothstep(-5.0,3.0,worldPosition.y+(patches-.43)*4.0)*smoothstep(.48,.88,n.y+(patches-.5)*.16);
  vec2 rockUV=worldPosition.xz+worldPosition.y*vec2(.52,.67);
  return blendSurface(terrainSurface(rockUV,2.3,11.0,n),surfaceMap,snow);
 }
 return blendSurface(surfaceMap,bakedSurface(ground*.325,11.0,n),
   smoothstep(.52,.72,patches)*.48+(1.0-smoothstep(.54,.81,n.y))*.46);
}
void main(){
 worldPosition=rasterPosition;localPosition=rasterLocal;
 vec3 normal=worldNormal;vec3 surf=surface;
 if(surface.z>=19.5&&coverage.y<.9999){
   float dither=fract(52.9829189*fract(dot(floor(gl_FragCoord.xy),vec2(.06711056,.00583715))));
   if(coverage.y>=0.0?dither>=coverage.y:dither< -coverage.y)discard;
 }
 if((surface.z>6.5&&surface.z<7.5)||abs(surface.z-10.0)<.1){
   float threshold=hash(vec3(floor(gl_FragCoord.xy),0.0));
   if(threshold<coverage.x||threshold>=coverage.y)discard;
   surf.y=0.0;
 }
#ifdef DEPTH_IMPOSTOR
 vec2 tile=vec2(mod(atlasView,16.0),floor(atlasView/16.0));
 vec2 pixel=clamp(floor((rasterLocal.xy+.5)*64.0),vec2(0.0),vec2(63.0));
 vec4 sampleData=texelFetch(uCanopy,ivec2(tile*64.0+pixel),0);
 if(sampleData.a<=0.0)discard;
 float az=tile.x*6.2831853/16.0,el=-1.5707963+tile.y*3.14159265/4.0;
 vec3 direction=vec3(cos(el)*sin(az),sin(el),cos(el)*cos(az)),right=vec3(cos(az),0.0,-sin(az)),up=cross(direction,right);
 localPosition=right*rasterLocal.x+up*rasterLocal.y+direction*(sampleData.a-.5);
 worldPosition=(objectModel*vec4(localPosition,1.0)).xyz;
 normal=objectNormals*sampleData.xyz;surf.z=7.0;
 vec4 clip=uVP*vec4(worldPosition,1.0);
 gl_FragDepth=clamp(clip.z/clip.w*.5+.5,0.0,1.0);
#endif
 vec3 n=normalize(normal),v=normalize(uEye-worldPosition),l=lightDirection,h=normalize(v+l);
 vec3 base=color;float rough=clamp(surf.x,.06,1.0),metal=surf.y;
 float layer=surf.z-20.0;
 bool foliage=(layer>=-.5&&layer<3.5)||(layer>=13.5&&layer<15.5)||(layer>=17.5&&layer<19.5);
 bool scanned=uMaterials!=0&&((surf.z>3.5&&surf.z<4.5)||(surf.z>4.5&&surf.z<5.5&&atlasView<-.5)||(surf.z>8.5&&surf.z<9.5));
 // Metre-scale curved tiles with overlaps, filtered when courses become subpixel.
 if(abs(surf.z-130.0)<.1){
   bool endSlope=abs((objectNormals[2].xyz*max(objectDimensions.z,.01)).x*n.x+
     (objectNormals[2].xyz*max(objectDimensions.z,.01)).z*n.z)>.27;
   vec2 axis=endSlope?vec2(rasterLocal.x*objectDimensions.x,rasterLocal.z*objectDimensions.z):
                      vec2(rasterLocal.z*objectDimensions.z,rasterLocal.x*objectDimensions.x);
   vec2 tile=axis/vec2(.22,.37), fringe=fwidth(tile);
   float density=1.0-smoothstep(.18,.73,max(fringe.x,fringe.y));
   float barrels=.5+.5*cos(tile.x*6.2831853);
   float course=smoothstep(.02,.18,fract(tile.y))*(1.0-.55*smoothstep(.83,.998,fract(tile.y)));
   float age=filteredNoise(worldPosition.xz*.74);
   base=color*(.77+.29*age+density*(.17*barrels+.14*course-.16));
   n=normalize(mix(n,bumpNormal(n,density*(barrels*.018+course*.008)),step(.2,n.y)));
   rough=mix(.74,.93,age);
 } else if(surf.z>=19.5){
  if(foliage&&dot(n,v)<0.0)n=-n;
  if(uSurfaceMaps!=0){
   PBRSurface material;
   if(surf.z>=39.5&&surf.z<99.5){
    material=biomeSurface(n,surf.z-40.0);
   } else if(abs(layer-16.0)<.5){
    material=birchSurface(n);
   } else if(abs(layer-7.0)<.5){
    material=timberSurface(n);
   } else if(layer>=6.5&&layer<12.5){
    material=bakedWorldSurface(n,layer,layer<7.5?1.8:2.1);
   } else {
    vec2 texUV=rasterUV;
    if((layer>=4.5&&layer<6.5)||abs(layer-17.0)<.5)texUV.y*=max(.26,objectDimensions.y*1.37);
    if(abs(layer-13.0)<.5)texUV.y*=12.0;
    material=bakedSurface(texUV,layer,n);
    if(abs(layer-5.0)<.5&&abs(atlasView+3.0)<.1){
     float tint=smoothstep(.24,.76,rasterLocal.y+.5+(filteredNoise(worldPosition.xz*1.7)-.5)*.33);
     material=blendSurface(material,bakedSurface(texUV,6.0,n),tint);
    }
    if((layer>=1.5&&layer<3.5)||(layer>=13.5&&layer<15.5)||(layer>=17.5&&layer<19.5)){
     if(texture(uSurfaceColor,vec3(texUV,layer)).a<(layer>=13.5?.28:.42))discard;
    }
   }
   base=material.base*color;n=material.normal;rough=material.rough;
  }else if(foliage){base*=vec3(.092,.23,.074);}
  else if(layer<6.5){base*=vec3(.15,.078,.047);}
  metal=0.0;
 }else if(scanned){
  PBRSurface material;
  if(surf.z<4.5){material=terrainSurface(worldPosition.xz,3.2,-1.0,n);material.base*=.87+.22*filteredNoise(worldPosition.xz*.32);}
  else if(surf.z<5.5)material=triplanarPBR(uBarkColor,uBarkNormal,n,v,1.0);
  else material=triplanarPBR(uRockColor,uRockNormal,n,v,2.4);
  base=material.base;
  if(surf.z>4.5&&surf.z<5.5&&atlasView<-1.5)base*=color;
  n=material.normal;rough=material.rough;metal=0.0;
 }else if(surf.z>3.5){
   vec3 an=abs(n);
   vec2 uv=an.y>.65?worldPosition.xz:an.x>.65?worldPosition.zy:worldPosition.xy;
#ifdef DEPTH_IMPOSTOR
   // Subpixel surface grain is not resolved in the far tier; retain baked normals/depth.
   float detail=.5;
#else
   float detail=filteredNoise(uv*22.0);
#endif
   float height=0.0;
   if(surf.z<4.5){
     vec2 g=worldPosition.xz;
     float patches=noise2(g*.15)*.65+noise2(g*.61)*.35;
     float grit=filteredNoise(g*11.0),fine=filteredNoise(g*65.0);
     base=mix(vec3(.075,.115,.027),vec3(.25,.19,.105),smoothstep(.39,.67,patches));
     base*=.67+.42*grit+.20*fine;
     height=(grit-.5)*.022+(fine-.5)*.004;
     rough=.94;
     if(uNatural!=0){
       float soil=filteredNoise(g*1.7),moss=filteredNoise(g*.36);
       float stones=smoothstep(.66,.82,filteredNoise(g*8.0));
       vec3 earth=mix(vec3(.09,.052,.022),vec3(.23,.17,.095),soil);
       vec3 turf=mix(vec3(.025,.048,.009),vec3(.095,.13,.025),moss);
       base=mix(turf,earth,smoothstep(.37,.63,patches));
       base=mix(base,vec3(.27,.255,.205),stones*.28);
       base*=.8+.3*grit;
       height+=(soil-.5)*.012+stones*.006;
     }
   } else if(surf.z<5.5){
     vec3 barkColor=atlasView<-1.5?color*vec3(.21,.13,.075):color;
     float grain=filteredNoise(uv*vec2(28.0,.65));
     float rings=sin(uv.x*65.0+noise2(uv*.7)*11.0);
     float fade=1.0-smoothstep(.3,1.2,fwidth(uv.x*65.0));
     base=barkColor*(.65+.5*grain+.12*rings*fade);
     height=(grain-.5)*.009+rings*fade*.0015;
     rough=.78;
     if(uNatural!=0){
       float fissure=filteredNoise(uv*vec2(65.0,3.0));
       float groove=smoothstep(.25,.50,fissure);
       base=mix(barkColor*.65,barkColor*1.05,groove)*(.8+.3*grain);
       height+=(groove-.5)*.004;
     }
   } else if(surf.z<6.5){
     vec2 tile=worldPosition.xz*vec2(2.6,3.5);
     tile.x+=mod(floor(tile.y),2.0)*.5;
     vec2 edge=abs(fract(tile-.5)-.5),fw=max(fwidth(tile),vec2(.001));
     vec2 seam=1.0-smoothstep(vec2(.025),vec2(.025)+fw,edge);
     float mortar=max(seam.x,seam.y);
     base=mix(color*(.7+.4*hash(vec3(floor(tile),1.0))),vec3(.065,.045,.033),mortar);
     height=(1.0-mortar)*.012+detail*.001;
     rough=.88;
   } else if(surf.z<8.5){
     // Vary leaf/blade color and darken roots/interior without a texture atlas.
     float tip=clamp(localPosition.y+.5,0.0,1.0);
     base*=mix(.48,1.25,tip)*(.85+.25*detail);
     rough=.86;
     if(dot(n,v)<0.0)n=-n;
   } else {
     float rock=filteredNoise(uv*3.5);
     base*=.65+.6*rock+.18*detail;
     height=(rock-.5)*.028+(detail-.5)*.008;
   }
   if(surf.z<6.5||surf.z>8.5)n=bumpNormal(n,height);
 } else if(surf.z>2.5){
   float variation=.5+.5*sin(worldPosition.x*.37+sin(worldPosition.z*.25));
   base*=.85+.3*variation;
 } else if(surf.z>.5){
   float grain=hash(floor(worldPosition*110));base*=.94+.12*grain;
   // Antialiased stone joints; use a plane appropriate to the face.
   vec3 an=abs(n);vec2 uv=an.y>.7?worldPosition.xz:an.x>.7?worldPosition.zy:worldPosition.xy;
   uv/=surf.z>1.5?2.0:1.25;
   vec2 edge=abs(fract(uv-.5)-.5),fw=max(fwidth(uv),vec2(.001));
   vec2 line=1-smoothstep(vec2(.009),vec2(.009)+fw,edge);
   base*=1-.23*max(line.x,line.y);
 }
 if(uWet!=0&&surf.z>3.5&&surf.z<4.5){
  float level=uWaveStrength*(.025*sin(worldPosition.x*.7+worldPosition.z*.35+uWetTime*1.1)+.012*sin(worldPosition.z*1.1-worldPosition.x*.2-uWetTime*.8));
  float wet=1.0-smoothstep(.025,.24,worldPosition.y-level);
  base*=mix(1.0,.48,wet);rough=mix(rough,.20,wet);
 }
 bool ground=(surf.z>3.5&&surf.z<4.5)||surf.z>=39.5;
 if(ground){
  float drainage=smoothstep(.35,.62,filteredNoise(worldPosition.xz*.262));
  float snow=uClimate.y*smoothstep(.55,.92,n.y)*mix(.35,1.0,drainage);
  base=mix(base,vec3(.63,.70,.74),snow);
  base*=1.0-uClimate.z*.28;rough=mix(rough,.47,uClimate.z*.60);
 }
 if(foliage&&abs(atlasView+7.0)<.1){
  float fall=clamp(uClimate.x*(.64+.48*filteredNoise(worldPosition.xz*.85)),0.0,1.0);
  base=mix(base,vec3(.33,.18,.021)*(.51+.7*base.g),fall);
 }

 float nv=max(dot(n,v),.001),nl=max(dot(n,l),0.0),nh=max(dot(n,h),0.0),vh=max(dot(v,h),0.0);
 vec3 f0=mix(vec3(.04),base,metal),F=f0+(1-f0)*pow(1.0-vh,5.0);
 float a=rough*rough,a2=a*a,den=nh*nh*(a2-1)+1;
 float D=a2/max(PI*den*den,.00001),k=(rough+1)*(rough+1)/8;
 float G=(nv/(nv*(1-k)+k))*(nl/(nl*(1-k)+k));
 vec3 spec=D*G*F/max(4*nv*nl,.001);
 vec3 direct=((1-F)*(1-metal)*base/PI+spec)*vec3(4.4,3.95,3.2)*nl*visibility(n);
 if(foliage||(surf.z>6.5&&surf.z<8.5))
   direct+=base*vec3(.35,.40,.20)*pow(max(dot(-n,l),0.0),2.0)*visibility(n);
 // Analytic environment approximation: no captured reflections or global illumination yet.
 vec3 env=sky(reflect(-v,n));vec3 ambient=(1-metal)*base*mix(vec3(.07,.085,.09),vec3(.20,.24,.29),n.y*.5+.5)+env*f0*(1-.6*rough);
 if(abs(surf.z-18.0)<.1){
   vec2 panel=rasterLocal.xy;
   vec3 pane=normalize(n+vec3(.019*sin(panel.x*7.0+panel.y*3.0),.026*sin(panel.y*5.0-panel.x*2.0),0.0));
   vec3 reflectRay=reflect(-v,pane);
   float mirror=.042+.958*pow(1.0-max(0.0,dot(pane,v)),5.0);
   float horizon=smoothstep(-.095,.035,reflectRay.y+.013*sin(reflectRay.x*20.0));
   vec3 glazed=mix(vec3(.074,.082,.067),sky(reflectRay)*2.05,horizon);
   float glint=pow(max(dot(reflectRay,l),0.0),140.0);
   vec3 recess=base*(.27+.35*smoothstep(-.42,.29,panel.y));
   direct=mix(recess,glazed,mirror)+glint*mirror*vec3(2.0,1.8,1.48)*visibility(n);
   ambient=vec3(0.0);rough=.085;
 }
 float fog=1-exp(-length(uEye-worldPosition)*uFogDensity);
 direct=mix(direct,vec3(.29,.36,.42),fog);ambient*=1-fog;
 if(uVisibilityEnabled!=0){
   vec2 uv=(worldPosition.xz+vec2(128.0))/256.0;
   vec2 state=texture(uVisibility,uv).rg;
   if(any(lessThan(uv,vec2(0.0)))||any(greaterThan(uv,vec2(1.0))))state=vec2(0.0);
   if(state.g<.001 && !(surf.z>3.5 && surf.z<4.5))discard;
   float visibilityWeight=mix(.12*state.g,1.0,state.r);
   direct=direct*visibilityWeight;ambient=ambient*visibilityWeight;
 }
 outDirect=vec4(direct,1);outAmbient=vec4(ambient,1);outNormal=vec4(mat3(uView)*n,rough);
})GLSL";
inline const char *fullscreen = R"GLSL(#version 430 core
out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2-1,0,1);}
)GLSL";
inline const char *composite = R"GLSL(#version 430 core
in vec2 uv;out vec4 frag;
uniform sampler2D uDirect;uniform sampler2D uAmbient;uniform sampler2D uDepth;
uniform float uExposure;uniform int uAA;
uniform sampler2D uNormal;uniform int uContact;uniform vec2 uProjectionScale;
uniform int uWater,uWaterSSR;uniform float uWaterTime;uniform float uWaveStrength;
uniform vec3 uCameraEye,uCameraRight,uCameraUp,uCameraForward;
// Perspective reconstruction uses Camera's shared 0.1 / 250 metre clipping planes.
vec3 viewPosition(vec2 p,float depth){
 float z=(2.0*.1*250.0)/(250.0+.1-(depth*2.0-1.0)*(250.0-.1));
 return vec3((p*2.0-1.0)*z/uProjectionScale,-z);
}
float ambientContact(vec2 p){
 float d=texture(uDepth,p).r;
 if(uContact==0||d>=.999999)return 1.0;
 vec3 origin=viewPosition(p,d),normal=normalize(texture(uNormal,p).xyz);
 // World-space radius, deterministic samples: no animated noise or temporal history.
 float radius=1.25,occlusion=0.0;
 vec2 projected=radius*uProjectionScale/(2.0*max(-origin.z,.1));
 for(int i=0;i<12;i++){
   float t=float(i)+.5,angle=t*2.39996323;
   vec2 q=p+vec2(cos(angle),sin(angle))*sqrt(t/12.0)*projected;
   if(any(lessThan(q,vec2(0.0)))||any(greaterThan(q,vec2(1.0))))continue;
   float sd=texture(uDepth,q).r;if(sd>=.999999)continue;
   vec3 delta=viewPosition(q,sd)-origin;float dist=length(delta);
   float facing=max(dot(normal,delta/max(dist,.001))-.12,0.0);
   occlusion+=facing*(1.0-smoothstep(radius*.25,radius,dist));
 }
 return clamp(1.0-occlusion*(2.0/12.0),.35,1.0);
}
float waterHeight(vec2 p){return uWaveStrength*(.025*sin(p.x*.7+p.y*.35+uWaterTime*1.1)+.012*sin(p.y*1.1-p.x*.2-uWaterTime*.8));}
float groundHeight(vec2 p){return .08*p.x+.12*sin(p.y*.15)+.02*sin(p.x*.8+p.y*.4);}
vec3 fromView(vec3 p){return uCameraEye+uCameraRight*p.x+uCameraUp*p.y-uCameraForward*p.z;}
vec3 toView(vec3 p){vec3 d=p-uCameraEye;return vec3(dot(d,uCameraRight),dot(d,uCameraUp),-dot(d,uCameraForward));}
// Unpolarized dielectric Fresnel, air -> water (IOR 1.333).
float waterFresnel(float cosine){
 float c=clamp(cosine,0.0,1.0),eta=1.333;
 float transmitted=sqrt(max(0.0,1.0-(1.0-c*c)/(eta*eta)));
 float perpendicular=(c-eta*transmitted)/(c+eta*transmitted);
 float parallel=(eta*c-transmitted)/(eta*c+transmitted);
 return .5*(perpendicular*perpendicular+parallel*parallel);
}
// Bounded perspective-correct screen traversal. Not a full DDA or a scene ray tracer.
// It still cannot see occluded/off-screen geometry or guarantee thin-object hits.
bool waterReflection(vec3 origin,vec3 direction,out vec2 hitUV,out float confidence){
 vec3 start=toView(origin);
 vec3 dir=vec3(dot(direction,uCameraRight),dot(direction,uCameraUp),-dot(direction,uCameraForward));
 float reach=35.0;
 if(dir.z>.00001)reach=min(reach,(-.12-start.z)/dir.z);
 if(reach<=.05||start.z>=-.1)return false;
 vec3 finish=start+dir*reach;
 float k0=1.0/(-start.z),k1=1.0/(-finish.z);
 vec3 q0=start*k0,q1=finish*k1;
 vec2 p0=q0.xy*uProjectionScale*.5+.5,p1=q1.xy*uProjectionScale*.5+.5;
 if(any(lessThan(p0,vec2(.002)))||any(greaterThan(p0,vec2(.998))))return false;
 vec2 delta=p1-p0;float clip=1.0;
 if(delta.x>.000001)clip=min(clip,(.998-p0.x)/delta.x);
 if(delta.x<-.000001)clip=min(clip,(.002-p0.x)/delta.x);
 if(delta.y>.000001)clip=min(clip,(.998-p0.y)/delta.y);
 if(delta.y<-.000001)clip=min(clip,(.002-p0.y)/delta.y);
 p1=mix(p0,p1,clip);q1=mix(q0,q1,clip);k1=mix(k0,k1,clip);
 vec2 pixels=abs(p1-p0)*vec2(textureSize(uDepth,0));
 int steps=int(clamp(ceil(max(pixels.x,pixels.y)/2.0),1.0,48.0));
 float previous=0.0,previousDelta=-1.0;
 for(int i=1;i<=48;i++){
  if(i>steps)break;
  float alpha=float(i)/float(steps);vec2 at=mix(p0,p1,alpha);
  float depth=texture(uDepth,at).r;
  if(depth>=.999999){previous=alpha;previousDelta=-1.0;continue;}
  vec3 ray=mix(q0,q1,alpha)/mix(k0,k1,alpha);
  float difference=-ray.z+viewPosition(at,depth).z;
  if(difference>=0.0&&previousDelta<0.0){
   float low=previous,high=alpha;
   for(int refine=0;refine<5;refine++){
    float mid=(low+high)*.5;vec2 uvMid=mix(p0,p1,mid);
    float midDepth=texture(uDepth,uvMid).r;
    vec3 midRay=mix(q0,q1,mid)/mix(k0,k1,mid);
    float d=midDepth>=.999999?-1.0:-midRay.z+viewPosition(uvMid,midDepth).z;
    if(d>=0.0)high=mid;else low=mid;
   }
   hitUV=mix(p0,p1,high);float hitDepth=texture(uDepth,hitUV).r;
   vec3 surface=viewPosition(hitUV,hitDepth),hitRay=mix(q0,q1,high)/mix(k0,k1,high);
   float error=-hitRay.z+surface.z,thickness=.06+.002*abs(surface.z);
   vec3 worldSurface=fromView(surface);
   if(hitDepth<.999999&&error>=0.0&&error<thickness&&worldSurface.y>waterHeight(worldSurface.xz)+.015){
    float edge=min(min(hitUV.x,hitUV.y),min(1.0-hitUV.x,1.0-hitUV.y));
    confidence=smoothstep(.002,.06,edge)*(1.0-smoothstep(thickness*.5,thickness,error));
    return true;
   }
  }
  previous=alpha;previousDelta=difference;
 }
 return false;
}

bool waterSurface(vec2 screen,out vec3 result,out float coverage){
 if(uWater==0)return false;
 vec3 ray=uCameraForward+uCameraRight*((screen.x*2.0-1.0)/uProjectionScale.x)+uCameraUp*((screen.y*2.0-1.0)/uProjectionScale.y);
 if(ray.y>=-.001||uCameraEye.y<=.05)return false;
 float t=-uCameraEye.y/ray.y;vec3 point=uCameraEye+ray*t;
 for(int i=0;i<2;i++){t=(waterHeight(point.xz)-uCameraEye.y)/ray.y;point=uCameraEye+ray*t;}
 if(t<=0.0||any(greaterThan(abs(point.xz),vec2(24.0))))return false;
 float depth=texture(uDepth,screen).r;
 if(depth<.999999&&t>=-viewPosition(screen,depth).z)return false;
 float waterDepth=waterHeight(point.xz)-groundHeight(point.xz);if(waterDepth<=0.0)return false;
 coverage=smoothstep(0.0,.045,waterDepth);
 float a=cos(point.x*.7+point.z*.35+uWaterTime*1.1),b=cos(point.z*1.1-point.x*.2-uWaterTime*.8);
 vec3 n=normalize(vec3(-uWaveStrength*(.0175*a-.0024*b),1.0,-uWaveStrength*(.00875*a+.0132*b))),v=normalize(uCameraEye-point);
 vec3 reflected=reflect(-v,n);
 vec3 reflection=mix(vec3(.29,.36,.42),vec3(.095,.20,.33),clamp(reflected.y,0.0,1.0));
 // The reflected direction follows the actual water normal; no independent image animation.
 vec2 hitUV;float confidence;
 if(uWaterSSR!=0&&waterReflection(point+n*.04,reflected,hitUV,confidence)){
  vec3 hitColor=texture(uDirect,hitUV).rgb+texture(uAmbient,hitUV).rgb;
  reflection=mix(reflection,hitColor,confidence);
 }
 vec2 refracted=clamp(screen+n.xz*.012*min(waterDepth,1.5),vec2(.001),vec2(.999));
 float rd=texture(uDepth,refracted).r;
 if(rd>=.999999||fromView(viewPosition(refracted,rd)).y>waterHeight(point.xz))refracted=screen;
 vec3 bed=texture(uDirect,refracted).rgb+texture(uAmbient,refracted).rgb;
 vec3 transmission=exp(-vec3(.65,.27,.18)*waterDepth);
 vec3 body=bed*transmission+vec3(.018,.085,.075)*(1.0-transmission);
 float fresnel=waterFresnel(dot(n,v));
 vec3 halfVector=normalize(v+vec3(-.4562,.7603,.4562));
 vec3 sun=vec3(4.4,3.95,3.2)*pow(max(dot(n,halfVector),0.0),512.0)*.8;
 result=mix(body,reflection,fresnel)+sun;return true;
}
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.0,1.0);}
vec3 mapped(vec2 p,float contact){
 float depth=texture(uDepth,p).r;
 vec3 c=texture(uDirect,p).rgb+texture(uAmbient,p).rgb*contact;
 if(depth>=.999999)c=mix(vec3(.54,.63,.69),vec3(.095,.20,.33),smoothstep(0.0,1.0,p.y));
 return pow(aces(c*uExposure),vec3(1.0/2.2));
}
void main(){
 float contact=ambientContact(uv);
 vec3 c=mapped(uv,contact);
 if(uAA!=0){
   vec2 px=1.0/vec2(textureSize(uDepth,0));
   vec3 a=mapped(uv+vec2(px.x,0.0),contact),b=mapped(uv-vec2(px.x,0.0),contact);
   vec3 d=mapped(uv+vec2(0.0,px.y),contact),e=mapped(uv-vec2(0.0,px.y),contact);
   vec3 avg=(a+b+d+e)*.25;
   float edge=length(c-avg);
   c=mix(c,avg,.45*smoothstep(.045,.22,edge));
 }
 vec3 waterColor;float waterCoverage;
 if(waterSurface(uv,waterColor,waterCoverage))c=mix(c,pow(aces(waterColor*uExposure),vec3(1.0/2.2)),waterCoverage);
 frag=vec4(c,1.0);
})GLSL";
inline const char *uiVert = R"GLSL(#version 430 core
layout(location=0) in vec2 aPosition;layout(location=1) in vec4 aColor;
uniform vec2 uSize;out vec4 color;
void main(){gl_Position=vec4(aPosition/uSize*vec2(2,-2)+vec2(-1,1),0,1);color=aColor;}
)GLSL";
inline const char *uiFrag = R"GLSL(#version 430 core
in vec4 color;out vec4 frag;void main(){frag=color;}
)GLSL";
} // namespace pl::shader
