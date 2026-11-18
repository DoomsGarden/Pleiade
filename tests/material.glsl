#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;
in vec3 rasterLocal;in vec3 rasterPosition;in vec3 worldNormal;in vec3 color;flat in vec3 surface;
in vec2 rasterUV;
flat in vec3 objectDimensions;
flat in vec2 coverage;
flat in float atlasView;
vec3 worldPosition,localPosition;
#ifdef DEPTH_IMPOSTOR
flat in mat4 objectModel;flat in mat3 objectNormals;
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
uniform vec3 lightDirection;
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
vec3 bumpNormal(vec3 n,float height){
 vec3 dx=dFdx(worldPosition),dy=dFdy(worldPosition);
 vec3 r1=cross(dy,n),r2=cross(n,dx);float det=dot(dx,r1);
 return normalize(abs(det)*n-sign(det)*(dFdx(height)*r1+dFdy(height)*r2));
}
uniform int uShadowSoft;
float visibility(vec3 n){
 if(uShadows==0)return 1.0;
 vec4 p=uLightVP*vec4(worldPosition,1.0);vec3 q=p.xyz/p.w*.5+.5;
 vec3 dx=dFdx(q),dy=dFdy(q);
 float determinant=dx.x*dy.y-dx.y*dy.x;
 vec2 gradient=vec2(0.0);
 bool regular=abs(determinant)>1e-12;
 if(regular)gradient=vec2(dy.y*dx.z-dx.y*dy.z,dx.x*dy.z-dy.x*dx.z)/determinant;
 if(q.z<0.0||q.z>1.0||any(lessThan(q.xy,vec2(0.0)))||any(greaterThan(q.xy,vec2(1.0))))return 1.0;
 float bias=regular?.000005:max(.0004*(1.0-dot(n,lightDirection)),.00012);
 vec2 size=vec2(textureSize(uShadow,0)),base=floor(q.xy*size);
 vec2 phase=q.xy*size-base-.5;
 float sum=0.0,total=0.0;
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
   vec2 offset=vec2(float(x),float(y)),centre=(base+offset+.5)/size;
   // The outer weight vanishes before the 3x3 neighbourhood changes.
   // This keeps the filter continuous across texel boundaries with nine reads.
   vec2 tent=max(vec2(0.0),vec2(1.5)-abs(offset-phase));
   float weight=uShadowSoft!=0?tent.x*tent.y:1.0;
   float receiver=q.z+dot(gradient,centre-q.xy);
   float visible=(receiver-bias<=texture(uShadow,centre).r)?1.0:0.0;
   if(any(lessThan(centre,vec2(0.0)))||any(greaterThan(centre,vec2(1.0))))visible=1.0;
   sum+=weight*visible;total+=weight;
 }
 return sum/max(total,.00001);
}

vec3 sky(vec3 d){return mix(vec3(.12,.15,.18),vec3(.28,.42,.61),clamp(d.y*.5+.5,0.0,1.0));}
PBRSurface biomeSurface(vec3 n,float layer){
 float patches=filteredNoise(worldPosition.xz*.16);
 vec2 ground=worldPosition.xz/(layer<10.5?2.9:2.1);
 if(layer<9.5){
  vec2 xz=worldPosition.xz;
  ground+=vec2(filteredNoise(xz*.14+vec2(2.1,9.5))-.5,patches-.5)*.54;
  PBRSurface sand=bakedSurface(ground,layer,n);
  sand.base*=.92+.15*patches;
  sand.normal=normalize(mix(n,sand.normal,.46+.41*patches));
  return sand;
 }
 PBRSurface surfaceMap=bakedSurface(ground,layer,n);
 if(layer<10.5){
  float snow=smoothstep(-5.0,3.0,worldPosition.y+(patches-.43)*4.0)*smoothstep(.48,.88,n.y+(patches-.5)*.16);
  return blendSurface(bakedSurface(ground,11.0,n),surfaceMap,snow);
 }
 return blendSurface(surfaceMap,bakedSurface(ground*.68,11.0,n),
   smoothstep(.52,.72,patches)*.48+(1.0-smoothstep(.54,.81,n.y))*.46);
}
uniform vec3 uLightColor;
uniform float uSkyAmbient;
uniform float uDaylight;
vec3 atmosphereFog(vec3 d){float h=max(d.y,0.0);return mix(mix(vec3(.006,.009,.018),vec3(.0007,.0015,.004),sqrt(h)),mix(vec3(.38,.49,.60),vec3(.045,.18,.42),sqrt(h)),uDaylight);}

void main(){
 worldPosition=rasterPosition;localPosition=rasterLocal;
 vec3 normal=worldNormal;vec3 surf=surface;
 if((surface.z>6.5&&surface.z<7.5)||abs(surface.z-10.0)<.1){
   float threshold=hash(vec3(floor(gl_FragCoord.xy),0.0));
   if(threshold<coverage.x||threshold>=coverage.y)discard;
   surf.y=0.0;
 }
#ifdef DEPTH_IMPOSTOR
 vec2 tile=vec2(mod(atlasView,16.0),floor(atlasView/16.0));
 vec2 pixel=clamp(floor((rasterLocal.xy+.5)*64.0),vec2(0.0),vec2(63.0));
 vec4 sampleData=texelFetch(uCanopy,ivec2(tile*64.0+pixel),0.0);
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
 bool foliage=(layer>=-.5&&layer<3.5)||(layer>=13.5&&layer<15.5);
 bool scanned=uMaterials!=0&&((surf.z>3.5&&surf.z<4.5)||(surf.z>4.5&&surf.z<5.5&&atlasView<-.5)||(surf.z>8.5&&surf.z<9.5));
 if(surf.z>=19.5){
  if(foliage&&dot(n,v)<0.0)n=-n;
  if(uSurfaceMaps!=0){
   PBRSurface material;
   if(surf.z>=39.5){
    material=biomeSurface(n,surf.z-40.0);
   } else if(layer>=6.5&&layer<12.5){
    material=bakedWorldSurface(n,layer,layer<7.5?1.8:2.1);
   } else {
    vec2 texUV=rasterUV;
    if((layer>=4.5&&layer<6.5)||(layer>=15.5&&layer<17.5))texUV.y*=max(.26,objectDimensions.y*1.37);
    if(abs(layer-13.0)<.5)texUV.y*=12.0;
    material=bakedSurface(texUV,layer,n);
    if(abs(layer-5.0)<.5&&abs(atlasView+3.0)<.1){
     float tint=smoothstep(.24,.76,rasterLocal.y+.5+(filteredNoise(worldPosition.xz*1.7)-.5)*.33);
     material=blendSurface(material,bakedSurface(texUV,6.0,n),tint);
    }
    if(abs(layer-16.0)<.5&&abs(atlasView+4.0)<.1){
     float lower=1.0-smoothstep(.065,.23,rasterLocal.y+.5+(filteredNoise(worldPosition.xz*2.8+worldPosition.y*vec2(.35,.55)) - .5)*.13);
     material=blendSurface(material,bakedSurface(texUV,5.0,n),lower*.93);
    }
    if((layer>=1.5&&layer<3.5)||(layer>=13.5&&layer<15.5)){
     if(texture(uSurfaceColor,vec3(texUV,layer)).a<(layer>=13.5?.28:.42))discard;
    }
   }
   base=material.base*color;n=material.normal;rough=material.rough;
  }else if(foliage){base*=vec3(.092,.23,.074);}
  else if(layer<6.5){base*=vec3(.15,.078,.047);}
  metal=0.0;
 }else if(scanned){
  PBRSurface material;
  if(surf.z<4.5){float fade=1.0-smoothstep(6.0,14.0,length(uEye-worldPosition));material=readPBR(uGroundColor,uGroundNormal,worldPosition.xz/3.2,n,v,.035/3.2*fade);}
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
   float grain=hash(floor(worldPosition*110.0));base*=.94+.12*grain;
   // Antialiased stone joints; use a plane appropriate to the face.
   vec3 an=abs(n);vec2 uv=an.y>.7?worldPosition.xz:an.x>.7?worldPosition.zy:worldPosition.xy;
   uv/=surf.z>1.5?2.0:1.25;
   vec2 edge=abs(fract(uv-.5)-.5),fw=max(fwidth(uv),vec2(.001));
   vec2 line=1.0-smoothstep(vec2(.009),vec2(.009)+fw,edge);
   base*=1.0-.23*max(line.x,line.y);
 }
 if(uWet!=0&&surf.z>3.5&&surf.z<4.5&&all(lessThan(abs(worldPosition.xz),vec2(24.0)))){
  float level=uWaveStrength*(.025*sin(worldPosition.x*.7+worldPosition.z*.35+uWetTime*1.1)+.012*sin(worldPosition.z*1.1-worldPosition.x*.2-uWetTime*.8));
  float wet=1.0-smoothstep(.025,.24,worldPosition.y-level);
  base*=mix(1.0,.48,wet);rough=mix(rough,.20,wet);
 }
 float nv=max(dot(n,v),.001),nl=max(dot(n,l),0.0),nh=max(dot(n,h),0.0),vh=max(dot(v,h),0.0);
 vec3 f0=mix(vec3(.04),base,metal),F=f0+(1.0-f0)*pow(1.0-vh,5.0);
 float a=rough*rough,a2=a*a,den=nh*nh*(a2-1.0)+1.0;
 float D=a2/max(PI*den*den,.00001),k=(rough+1.0)*(rough+1.0)/8.0;
 float G=(nv/(nv*(1.0-k)+k))*(nl/(nl*(1.0-k)+k));
 vec3 spec=D*G*F/max(4.0*nv*nl,.001);
 vec3 direct=((1.0-F)*(1.0-metal)*base/PI+spec)*uLightColor*nl*visibility(n);
 if(foliage||(surf.z>6.5&&surf.z<8.5))
   direct+=base*uLightColor*vec3(.08,.10,.06)*pow(max(dot(-n,l),0.0),2.0)*visibility(n);
 // Analytic environment approximation: no captured reflections or global illumination yet.
 vec3 env=sky(reflect(-v,n));vec3 ambient=(1.0-metal)*base*mix(vec3(.07,.085,.09),vec3(.20,.24,.29),n.y*.5+.5)+env*f0*(1.0-.6*rough);
 ambient*=uSkyAmbient;
 float fog=1.0-exp(-length(uEye-worldPosition)*uFogDensity);
 direct=mix(direct,atmosphereFog(normalize(worldPosition-uEye)),fog);ambient*=1.0-fog;
 if(uVisibilityEnabled!=0){
   vec2 uv=(worldPosition.xz+vec2(128.0))/256.0;
   vec2 state=texture(uVisibility,uv).rg;
   if(any(lessThan(uv,vec2(0.0)))||any(greaterThan(uv,vec2(1.0))))state=vec2(0.0);
   if(state.g<.001 && !(surf.z>3.5 && surf.z<4.5))discard;
   float visibilityWeight=mix(.12*state.g,1.0,state.r);
   direct=direct*visibilityWeight;ambient=ambient*visibilityWeight;
 }
 outDirect=vec4(direct,1.0);outAmbient=vec4(ambient,1.0);outNormal=vec4(mat3(uView)*n,rough);
}