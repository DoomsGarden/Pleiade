#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;
in vec2 uv;out vec4 frag;
uniform sampler2D uDirect;uniform sampler2D uAmbient;uniform sampler2D uDepth;
uniform float uExposure;
uniform float uTurbidity;uniform int uAA;
uniform sampler2D uNormal;uniform int uContact;uniform vec2 uProjectionScale;
uniform int uWater,uWaterSSR;uniform float uWaterTime;uniform float uWaveStrength;
uniform vec3 uCameraEye,uCameraRight,uCameraUp,uCameraForward;
// Perspective reconstruction uses Camera's shared 0.1 / 250.0 metre clipping planes.
uniform vec2 uDepthRange;
float cameraDepth(float depth){
 return uDepthRange.x*uDepthRange.y/(uDepthRange.x+(1.0-depth)*(uDepthRange.y-uDepthRange.x));
}

vec3 viewPosition(vec2 p,float depth){
 float z=cameraDepth(depth);
 return vec3((p*2.0-1.0)*z/uProjectionScale,-z);
}
float ambientContact(vec2 p){
 float d=texture(uDepth,p).r;
 if(uContact==0||d>=.999999)return 1.0;
 vec3 origin=viewPosition(p,d),normal=normalize(texture(uNormal,p).xyz);
 // World-space radius, deterministic samples: no animated noise or temporal history.
 float occlusion=0.0;
 for(int i=0;i<12;i++){
   float t=float(i)+.5,angle=t*2.39996323;
   float radius=t<6.0 ? .38 : 1.25;
   vec2 projected=radius*uProjectionScale/(2.0*max(-origin.z,.1));
   vec2 q=p+vec2(cos(angle),sin(angle))*sqrt((mod(float(i),6.0)+.5)/6.0)*projected;
   if(any(lessThan(q,vec2(0.0)))||any(greaterThan(q,vec2(1.0))))continue;
   float sd=texture(uDepth,q).r;if(sd>=.999999)continue;
   vec3 delta=viewPosition(q,sd)-origin;float dist=length(delta);
   float facing=max(dot(normal,delta/max(dist,.001))-.12,0.0);
   occlusion+=facing*(1.0-smoothstep(radius*.25,radius,dist));
 }
 return clamp(1.0-occlusion*(2.0/12.0),.35,1.0);
}

// Coast coordinates in metres. The WebGL basin bends along z; the native demo
// has a different shoreline profile. Positive x leads toward dry ground.
vec2 shoreCoordinates(vec2 p,bool curved){
 float bank=curved?1.60*sin(p.y*.091)+.68*sin(p.y*.19)
                  :-(.12*sin(p.y*.15)+.02*sin(p.y*.4))/.08;
 return vec2(p.x-bank,p.y);
}
float shoreBankSlope(float z,bool curved){
 return curved?.1456*cos(z*.091)+.1292*cos(z*.19)
               :-(.018*cos(z*.15)+.008*cos(z*.4))/.08;
}
float shoreShallow(float x){
 return 1.0/(1.0+exp(clamp(-.44*(x+2.35),-24.0,24.0)));
}
float swellPhase(vec2 q,float time){
 // Raising the spatial frequency in shallow water slows the advancing crest.
 return 1.48*(q.x+4.0*shoreShallow(q.x))+.19*q.y+.54*sin(q.y*.36)-1.86*time;
}
vec3 lakeWave(vec2 p,float time,float strength,bool curved){
 vec2 q=shoreCoordinates(p,curved);
 float shallow=shoreShallow(q.x), delta=.44*shallow*(1.0-shallow);
 float phase=swellPhase(q,time);
 float secondary=2.82*(q.x+1.8*shallow)-.51*q.y-2.91*time+.77;
 float capillary=5.3*q.x+.98*q.y-3.75*time;
 float group=q.y*.48+q.x*.29-time*.39;
 float envelope=.83+.17*cos(group);
 float base=.045+.025*shallow;
 float amplitude=base*envelope;
 float height=amplitude*sin(phase)+.017*sin(secondary)+.0035*sin(capillary);
 float dx=(.025*delta*envelope-base*.0493*sin(group))*sin(phase)+amplitude*1.48*(1.0+4.0*delta)*cos(phase)
          +.017*2.82*(1.0+1.8*delta)*cos(secondary)+.01855*cos(capillary);
 float dz=amplitude*(.19+.1944*cos(q.y*.36))*cos(phase)-base*.0816*sin(group)*sin(phase)
          -.00867*cos(secondary)+.00343*cos(capillary);
 return strength*vec3(height,dx,dz-shoreBankSlope(p.y,curved)*dx);
}
float recentShoreWater(vec2 p,float time,float strength,bool curved){
 float now=lakeWave(p,time,strength,curved).x;
 float last=lakeWave(p,max(0.0,time-.36),strength,curved).x-.018;
 float prior=lakeWave(p,max(0.0,time-.78),strength,curved).x-.042;
 return max(now,max(last,prior));
}


float foamHash(vec2 p){
 vec3 h=fract(vec3(p.x,p.y,p.x)*vec3(.1031,.1030,.0973));
 h+=dot(h,h.yzx+33.33);
 return fract((h.x+h.y)*h.z);
}
float foamNoise(vec2 p){
 vec2 cell=floor(p),f=fract(p);
 f=f*f*(3.0-2.0*f);
 return mix(mix(foamHash(cell),foamHash(cell+vec2(1.0,0.0)),f.x),
            mix(foamHash(cell+vec2(0.0,1.0)),foamHash(cell+1.0),f.x),f.y);
}
// Broad, fragmented patches with holes. The fine texture fades into its
// average coverage when the cell size projects to less than a pixel.
float foamLace(vec2 flow,float density,float footprint){
 float broad=foamNoise(flow*2.5);
 vec2 warped=flow+vec2(broad-.5,foamNoise(flow*1.45+8.3)-.5)*.26;
 float fine=foamNoise(warped*12.0+5.1);
 float pores=mix(fine,.54,smoothstep(.022,.13,footprint));
 float texture=.62*broad+.38*pores;
 float threshold=mix(.79,.30,clamp(density,0.0,1.0));
 return smoothstep(threshold-.095,threshold+.095,texture)*smoothstep(.012,.22,density);
}
float lakeFoam(vec2 p,float time,float strength,bool curved,float footprint){
 if(strength<=.001)return 0.0;
 vec2 q=shoreCoordinates(p,curved);
 // Foam is generated near the depth at which this swell overturns. Evaluate
 // the crest at its generation point and time, then carry the patch inland.
 float line=-.35-1.55*strength+.60*(foamNoise(vec2(q.y*.84,4.2))-.5);
 float runup=q.x-line;
 float age=max(0.0,runup/.72);
 float arrival=swellPhase(vec2(line,q.y-age*.22),time-age);
 float crest=sin(arrival);
 float patchiness=foamNoise(vec2(q.y*.57-arrival*.14, time*.24));
 float breaking=smoothstep(.42,.90,crest)*mix(.49,1.18,patchiness);
 float lingering=.19*smoothstep(-.82,.52,sin(arrival-.77));
 float drift=exp(-age*.58)*smoothstep(-.52,.12,runup)*(1.0-smoothstep(1.8,3.1,age));
 float density=drift*(breaking+lingering)*smoothstep(.025,.67,strength);
 if(density<.012)return 0.0;
 return .78*foamLace(vec2(q.x-time*.71,q.y-time*.11),density,footprint);
}
vec3 applyWaterFoam(vec3 water,float foam,vec3 light,float ambient,float sun){
 // Foam is a diffuse mixture of air and water. It hides the sharp surface
 // reflection and catches the sun even when the water below looks dark.
 vec3 body=vec3(.60,.674,.674)*(ambient*.56+.045)+light*(.113+.09*sun);
 return mix(water,body,clamp(foam,0.0,.93));
}



uniform int uWaterMode;

float channelCenter(float z){
 return -3.3+3.1*sin(z*.056)+1.3*sin(z*.133+.9);
}
float channelSlope(float z){
 return .1736*cos(z*.056)+.1729*cos(z*.133+.9);
}
float waterLevel(float z){
 return uWaterMode==0?0.0:.44+z*.010+(uWaterMode==2?3.65*smoothstep(-2.5,1.7,z):0.0);
}
float waterSlope(float z){
 if(uWaterMode==0)return 0.0;
 float s=clamp((z+2.5)/4.2,0.0,1.0);
 return .010+(uWaterMode==2?3.65*6.0*s*(1.0-s)/4.2:0.0);
}
vec3 waveField(vec2 p){
 if(uWaterMode==0)return lakeWave(p,uWaterTime,uWaveStrength,true);
 float x=p.x-channelCenter(p.y),z=p.y;
 // A constant phase translates in -z, following the current.
 float a=.86*x+1.34*z+2.75*uWaterTime;
 float b=-1.94*x+2.56*z+4.21*uWaterTime+.9;
 float c=3.60*x+4.24*z+6.86*uWaterTime;
 float h=.029*sin(a)+.010*sin(b)+.004*sin(c);
 float dx=.02494*cos(a)-.0194*cos(b)+.0144*cos(c);
 float dz=.03886*cos(a)+.0256*cos(b)+.01696*cos(c);
 return uWaveStrength*vec3(h,dx,dz-channelSlope(z)*dx);
}

float waterHeight(vec2 p){return waterLevel(p.y)+waveField(p).x;}
vec3 fromView(vec3 p){return uCameraEye+uCameraRight*p.x+uCameraUp*p.y-uCameraForward*p.z;}
uniform vec3 uSunDirection;
uniform vec3 uMoonDirection;
uniform float uSunRadius;
uniform float uMoonRadius;
uniform vec3 uLightColor;
uniform vec3 uSkyLightDirection;
uniform float uDaylight;
uniform float uSkyAmbient;
uniform float uStarAngle;
uniform float uLatitude;
uniform sampler2D uStars;
uniform float uSkyPixelAngle;
uniform float uWorldSeed;
uniform float uCloudAmount;
uniform float uCloudTime;
uniform vec3 uSkyObserver;
float skyHash(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7))+uWorldSeed)*43758.5453);}
float skyNoise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(mix(skyHash(i),skyHash(i+vec3(1,0,0)),f.x),mix(skyHash(i+vec3(0,1,0)),skyHash(i+vec3(1,1,0)),f.x),f.y),mix(mix(skyHash(i+vec3(0,0,1)),skyHash(i+vec3(1,0,1)),f.x),mix(skyHash(i+vec3(0,1,1)),skyHash(i+vec3(1,1,1)),f.x),f.y),f.z);}
float moonRelief(vec3 p){
 vec3 cell=floor(p*9.0);float crater=0.0;
 for(int z=-1;z<=1;z++)for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
 vec3 c=cell+vec3(float(x),float(y),float(z));vec3 centre=c+vec3(skyHash(c),skyHash(c+23.0),skyHash(c+47.0));
 float radius=.08+.62*skyHash(c+81.0),r=length(p*9.0-centre)/radius;
 crater+= .025*exp(-pow((r-1.0)*8.0,2.0))-.025*(1.0-smoothstep(.0,1.0,r));}
 return crater+.02*skyNoise(p*60.0);
}
vec3 pointStars(vec3 q){
 vec2 uv=vec2(fract(atan(q.y,q.x)/6.28318530718),asin(clamp(q.z,-1.0,1.0))/3.14159265359+.5);
 ivec2 size=textureSize(uStars,0),cell=ivec2(floor(uv*vec2(size)));vec3 sum=vec3(0);
 float sigma=max(uSkyPixelAngle*.42,.00001);
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
 ivec2 at=cell+ivec2(x,y);at.x=(at.x+size.x)%size.x;at.y=clamp(at.y,0,size.y-1);vec4 star=texelFetch(uStars,at,0);
 if(star.z<=0.0)continue;vec3 axis=vec3(cos(star.y)*cos(star.x),cos(star.y)*sin(star.x),sin(star.y));float distance=length(q-axis)/sigma;
 vec3 tint=mix(vec3(1.0,.78,.60),vec3(.70,.85,1.0),star.w);
 float flux=pow(star.z,3.0)*3.0;
 sum+=tint*flux*(exp(-.5*distance*distance)+.018*star.z*exp(-.04*distance*distance));}
 return sum;
}
float cloudDensity(vec3 p,float base,float top){
 float height=(p.y-base)/(top-base);if(height<=0.0||height>=1.0)return 0.0;
 vec3 q=p*.0012+vec3(uCloudTime*.0008,0,uCloudTime*.0003);
 float shape=skyNoise(q)*.70+skyNoise(q*2.7)*.30;
 return smoothstep(1.0-uCloudAmount*.70,1.0-uCloudAmount*.70+.17,shape)*smoothstep(0.0,.18,height)*(1.0-smoothstep(.65,1.0,height));
}
vec4 cloudVolume(vec3 origin,vec3 direction){
 if(uCloudAmount<=0.0)return vec4(0,0,0,1);vec3 d=normalize(direction),accum=vec3(0);float trans=1.0;
 // A bounded atmospheric volume: two height layers, integrated front to back.
 for(int layer=0;layer<2;layer++){
 int index=d.y>=0.0?layer:1-layer;float base=index==0?900.0:3200.0,top=index==0?1700.0:3650.0;
 float dy=abs(d.y)<.0001?(d.y<0.0?-.0001:.0001):d.y;
 float a=(base-origin.y)/dy,b=(top-origin.y)/dy;
 float entry=max(0.0,min(a,b)),end=min(6000.0,max(a,b));if(end<=entry)continue;
 float stepSize=(end-entry)/24.0;
 for(int i=0;i<24;i++){
 vec3 pos=origin+d*(entry+(float(i)+.25+.5*skyHash(floor(d*2000.0)))*stepSize);float den=cloudDensity(pos,base,top);
 float extinction=den*.004,alpha=1.0-exp(-extinction*stepSize);
 float toward=cloudDensity(pos+uSkyLightDirection*220.0,base,top);
 float light=exp(-toward*2.0),forward=pow(max(dot(d,uSkyLightDirection),0.0),12.0);
 vec3 scatter=vec3(.10,.14,.20)*uSkyAmbient+uLightColor*.16*(light+.35*forward);
 accum+=trans*alpha*scatter;trans*=1.0-alpha;if(trans<.015)break;
 }}return vec4(accum,trans);
}
uniform sampler2D uCloudMap;
uniform int uCloudCached;
vec4 cachedCloud(vec3 direction){
 vec3 d=normalize(direction);vec2 size=vec2(textureSize(uCloudMap,0));
 vec2 at=vec2(fract(atan(d.z,d.x)/6.28318530718),asin(clamp(d.y,-1.0,1.0))/3.14159265359+.5)*size-.5;
 ivec2 base=ivec2(floor(at)),dim=ivec2(size);vec2 f=fract(at);
 ivec2 a=ivec2((base.x+dim.x)%dim.x,clamp(base.y,0,dim.y-1)),b=ivec2((base.x+1+dim.x)%dim.x,clamp(base.y+1,0,dim.y-1));
 return mix(mix(texelFetch(uCloudMap,a,0),texelFetch(uCloudMap,ivec2(b.x,a.y),0),f.x),mix(texelFetch(uCloudMap,ivec2(a.x,b.y),0),texelFetch(uCloudMap,b,0),f.x),f.y);
}
vec3 cloudSky(vec3 background,vec3 origin,vec3 direction){
 if(uCloudAmount<=0.0)return background;
 // Reflections viewed from high above the surface sample the clouds from the shading point.
 bool nearObserver=abs(origin.y-uSkyObserver.y)<96.0;
 vec4 cloud=uCloudCached!=0&&nearObserver?cachedCloud(direction):cloudVolume(origin,direction);
 return cloud.rgb+background*cloud.a;
}
vec3 celestialSky(vec3 direction){
 vec3 d=normalize(direction);float h=max(d.y,0.0);
 vec3 night=mix(vec3(.006,.009,.018),vec3(.0007,.0015,.004),sqrt(h));
 vec3 day=mix(vec3(.38,.49,.60),vec3(.045,.18,.42),sqrt(h));
 float dusk=exp(-abs(uSunDirection.y)*12.0)*(1.0-h)*(1.0-h);
 vec3 color=mix(night,day,uDaylight)+vec3(.48,.115,.025)*dusk*pow(max(dot(d,uSunDirection)*.5+.5,0.0),6.0);
 if(uDaylight<.3&&d.y>0.0){
  float lat=uLatitude;vec3 q=vec3(d.x,d.y*cos(lat)+d.z*sin(lat),-d.y*sin(lat)+d.z*cos(lat));
  float a=uStarAngle;q=vec3(cos(a)*q.x+sin(a)*q.y,-sin(a)*q.x+cos(a)*q.y,q.z);
  color+=pointStars(q)*(1.0-smoothstep(0.0,.3,uDaylight))*smoothstep(0.0,.12,d.y);
 }
 // Chord distance keeps disc edges stable even when their radius is a fraction of a degree.
 float pixel=max(uSkyPixelAngle*.45,.000001);
 float sunEdge=2.0*sin(uSunRadius*.5),sunDistance=length(d-uSunDirection);
 float sunAA=min(pixel,sunEdge*.55);
 if(sunDistance<sunEdge+sunAA&&d.y>-.02){
  vec3 sunNormal=(d-uSunDirection*dot(d,uSunDirection))/sin(uSunRadius);
  float mu=sqrt(clamp(1.0-dot(sunNormal,sunNormal),0.0,1.0));
  float granulation=.94+.06*skyNoise(sunNormal*70.0);
  float coverage=1.0-smoothstep(sunEdge-sunAA,sunEdge+sunAA,sunDistance);
  color+=vec3(12.0,9.0,5.5)*(.4+.6*mu)*granulation*coverage*smoothstep(-.02,.01,d.y);
 }
 // Fixed surface coordinates on the fictional moon; the same disc occludes the sky behind it.
 float moonEdge=2.0*sin(uMoonRadius*.5),moonDistance=length(d-uMoonDirection);
 float moonAA=min(pixel,moonEdge*.55);
 if(d.y>0.0&&moonDistance<moonEdge+moonAA){
 vec3 tangent=normalize(cross(abs(uMoonDirection.y)>.95?vec3(1,0,0):vec3(0,1,0),uMoonDirection)),up=cross(uMoonDirection,tangent);
 vec2 xy=vec2(dot(d,tangent),dot(d,up))/sin(uMoonRadius);xy/=max(1.0,length(xy));
 float z=sqrt(max(0.0,1.0-dot(xy,xy)));vec3 surface=vec3(xy,z);
 float relief=moonRelief(surface),e=.003;
 vec2 gradient=vec2(moonRelief(normalize(surface+vec3(e,0,0)))-relief,moonRelief(normalize(surface+vec3(0,e,0)))-relief)/e;
 vec3 normal=normalize(tangent*(xy.x-gradient.x*.045)+up*(xy.y-gradient.y*.045)-uMoonDirection*z);
 float incidence=max(dot(normal,uSunDirection),0.0);
 float maria=smoothstep(.36,.65,skyNoise(surface*4.0));float albedo=mix(.18,.42,maria)+relief;
 vec3 lunar=vec3(.94,.93,.89)*albedo*(.006+incidence*2.0);
 float coverage=1.0-smoothstep(moonEdge-moonAA,moonEdge+moonAA,moonDistance);
 color=mix(color,lunar,coverage*smoothstep(0.0,.03,d.y));
 }

 return color;
}

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
 if(dir.z>.00001)reach=min(reach,(-uDepthRange.x-.02-start.z)/dir.z);
 if(reach<=.05||start.z>=-uDepthRange.x)return false;
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

uniform sampler2D uWaterDepth;
uniform vec3 uDrift[10];
uniform int uUnderwater;
uniform float uFlowTime;
uniform vec3 uWaterContacts[10];
uniform int uContactCount;

// Integral of the local slowness along the channel. Foam travels with the
// current and speeds up over the lip. Elapsed time cannot invert the flow.
float channelTravel(float z){
 if(uWaterMode!=2)return z/1.39;
 float section=clamp(z+3.15,0.0,5.15);
 return (z-section)/1.41+section/3.55;
}
vec2 waterFlow(vec2 p){
 if(uWaterMode==0)return normalize(vec2(1.0,-shoreBankSlope(p.y,true)));
 return normalize(vec2(-channelSlope(p.y),-1.0));
}
float obstacleFoam(vec2 at,vec2 delta,float radius,float time,float strength,float footprint){
 vec2 flow=waterFlow(at),across=vec2(-flow.y,flow.x);
 float downstream=dot(delta,flow),side=dot(delta,across);
 // Air entrainment is strongest where the water meets the rock and fades
 // along the wake. The wake widens as the bubbles leave the obstruction.
 float lengthScale=max(.36,radius*2.7);
 float width=max(.12,radius*.63)+max(0.0,downstream)*.28;
 float wake=exp(-abs(downstream-radius*.20)/lengthScale-pow(abs(side)/width,1.65));
 wake*=smoothstep(-radius*1.8,-radius*.5,downstream);
 float heartbeat=uWaterMode==0?.46+.54*smoothstep(.02,.96,sin(swellPhase(shoreCoordinates(at,true),time))):.77;
 float density=wake*strength*heartbeat;
 if(density<.026)return 0.0;
 vec2 moving=vec2(side,downstream-uFlowTime*(uWaterMode==0?.64:1.41));
 return .83*foamLace(moving,density,footprint);
}
float channelFoam(vec2 p,float slope,float footprint){
 float side=p.x-channelCenter(p.y),stream=channelTravel(p.y)+uFlowTime;
 float cascade=smoothstep(.20,.83,slope);
 vec2 advected=vec2(side,stream);
 float white=0.0;
 if(cascade>.01){
  float sheet=foamNoise(vec2(side*4.6,stream*1.65)+2.9);
  white=.69*cascade*foamLace(advected*vec2(1.06,.86),cascade*(.40+.53*sheet),footprint);
 }
 if(uWaterMode==2){
  // The plume begins at the foot and expands downstream; it is fed
  // continuously by new foam but its texture travels and breaks apart.
  float distance=-2.30-p.y;
  float width=1.55+clamp(distance,0.0,4.0)*.24;
  float plume=exp(-max(0.0,distance)*.51)*smoothstep(-.84,.32,distance);
  plume*=1.0-smoothstep(width*.54,width*1.75,abs(side));
  if(plume>.014){
   float entrainment=.66+.34*foamNoise(advected*vec2(1.6,1.72)+9.4);
   white=max(white,.92*foamLace(advected,plume*entrainment,footprint));
  }
 }
 for(int i=0;i<10;i++){
  vec2 d=p-uDrift[i].xz;
  float radius=dot(d*vec2(3.9,1.7),d*vec2(3.9,1.7));
  if(radius>=1.0)continue;
  float density=(1.0-smoothstep(.15,1.0,radius))*.44;
  white=max(white,.66*foamLace(d+float(i)*7.7,density,footprint));
 }
 return white;
}
bool waterSurface(vec2 screen,out vec3 result,out float coverage){
 if(uWater==0)return false;
 float depth=texture(uDepth,screen).r,surfaceDepth=texture(uWaterDepth,screen).r;
 if(surfaceDepth>=.999999||cameraDepth(surfaceDepth)>=cameraDepth(depth)-.002)return false;
 vec3 point=fromView(viewPosition(screen,surfaceDepth));
 float slope=waterSlope(point.z);
 vec3 wave=waveField(point.xz);
 float footprint=length(point-uCameraEye)*uSkyPixelAngle;
 float foam=0.0;
 for(int i=0;i<10;i++){
  if(i>=uContactCount)break;
  vec3 obstacle=uWaterContacts[i];
  vec2 delta=point.xz-obstacle.xy;
  float distance=length(delta);
  if(distance>obstacle.z+2.0)continue;
  float edge=distance-obstacle.z;
  float spread=exp(-max(0.0,edge)*2.3)*smoothstep(-.10,.09,edge);
  float phase=21.0*edge-uFlowTime*5.2+float(i)*2.31;
  float energy=max(uWaveStrength,uWaterMode==0?0.0:.43);
  float clear=1.0-smoothstep(.025,.17,footprint);
  wave.yz+=delta/max(distance,.001)*.095*energy*spread*cos(phase)*clear;
  foam=max(foam,obstacleFoam(obstacle.xy,delta,obstacle.z,uWaterTime,energy,footprint));
 }
 // High frequency slopes fade with distance; the broad reflection follows the wave.
 float rippleFilter=mix(1.0,.30,smoothstep(.05,.26,footprint));
 vec3 n=normalize(vec3(-wave.y*rippleFilter,1.0,-slope-wave.z*rippleFilter));
 vec3 v=normalize(uCameraEye-point);
 bool below=dot(v,n)<0.0;
 if(below)n=-n;
 vec3 reflected=reflect(-v,n);
 vec3 receiver=depth<.999999?fromView(viewPosition(screen,depth)):point-v*12.0;
 float rayPath=length(receiver-point),path=rayPath;
 // Reproject a short segment of the refracted ray in camera space.
 vec3 bent=refract(-v,n,below?1.333:1.0/1.333);
 vec3 sampleView=toView(point+bent*min(path,1.65));
 vec2 shift=sampleView.xy/(max(.1,-sampleView.z))*uProjectionScale*.5+.5-screen;
 vec2 refracted=clamp(screen+clamp(shift,vec2(-.034),vec2(.034)),vec2(.001),vec2(.999));
 float rd=texture(uDepth,refracted).r,refractWeight=0.0;
 if(rd<.999999&&rd>surfaceDepth&&dot(bent,bent)>.01){
  vec3 hit=fromView(viewPosition(refracted,rd));
  float depthBelow=waterHeight(hit.xz)-hit.y;
  float balance=length(hit-point)/max(.01,rayPath);
  refractWeight=smoothstep(.008,.20,depthBelow)*smoothstep(.025,.25,rayPath)
                *(1.0-smoothstep(1.25,2.0,balance));
  path=mix(rayPath,length(hit-point),refractWeight);
 }
 vec3 bed=texture(uDirect,screen).rgb+texture(uAmbient,screen).rgb;
 if(refractWeight>.001){
  vec3 displaced=texture(uDirect,refracted).rgb+texture(uAmbient,refracted).rgb;
  bed=mix(bed,displaced,refractWeight);
 }
 vec3 trans=exp(-(vec3(.51,.25,.17)+uTurbidity)*min(path,15.0));
 vec3 body=bed*trans+vec3(.021,.084,.105)*(1.0-trans)*mix(.11,1.0,uDaylight);
 vec3 environment=cloudSky(celestialSky(reflected),point,reflected);
 vec2 hitUV;float confidence;
 if(!below&&uWaterSSR!=0&&waterReflection(point+n*.06,reflected,hitUV,confidence)){
  environment=mix(environment,texture(uDirect,hitUV).rgb+texture(uAmbient,hitUV).rgb,confidence);
 }
 float fresnel=waterFresnel(dot(n,v));
 vec3 h=normalize(v+uSkyLightDirection);
 float nt=max(dot(n,h),0.0);
 float sharpness=mix(425.0,105.0,max(uWaveStrength,smoothstep(.04,.23,footprint)))*mix(1.0,.58,smoothstep(.1,.9,slope));
 float glint=pow(nt,sharpness)*.61+pow(nt,max(35.0,sharpness*.22))*.071;
 coverage=smoothstep(.003,.070,rayPath);
 if(below){
  result=mix(vec3(.013,.050,.058),environment,.45)*mix(.13,.65,uDaylight);
 }else{
  foam=max(foam,uWaterMode==0?lakeFoam(point.xz,uWaterTime,uWaveStrength,true,footprint)
                            :channelFoam(point.xz,slope,footprint));
  coverage=max(coverage,foam*smoothstep(.004,.052,rayPath));
  vec3 water=mix(body,environment,fresnel)+uLightColor*glint;
  result=applyWaterFoam(water,foam,uLightColor,uSkyAmbient,max(dot(n,uSkyLightDirection),0.0));
 }
 return true;
}

vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.0,1.0);}
vec3 mapped(vec2 p,float contact){
 float depth=texture(uDepth,p).r;
 vec3 c=texture(uDirect,p).rgb+texture(uAmbient,p).rgb*contact;
 if(depth>=.999999)c=celestialSky(uCameraForward+uCameraRight*((p.x*2.0-1.0)/uProjectionScale.x)+uCameraUp*((p.y*2.0-1.0)/uProjectionScale.y));
 return pow(aces(c*uExposure),vec3(1.0/2.2));
}
vec3 opaqueNeighbour(vec2 p,float centreDepth,vec3 centre,float contact){
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
 vec3 ray=normalize(uCameraForward+uCameraRight*((uv.x*2.0-1.0)/uProjectionScale.x)+uCameraUp*((uv.y*2.0-1.0)/uProjectionScale.y));
 c=pow(aces(cloudSky(celestialSky(ray),uCameraEye,ray)*uExposure),vec3(1.0/2.2));
 }else{
 float contact=ambientContact(uv);c=mapped(uv,contact);
 if(uAA!=0){vec2 px=1.0/vec2(textureSize(uDepth,0));
 vec3 avg=(opaqueNeighbour(uv+vec2(px.x,0),depth,c,contact)+opaqueNeighbour(uv-vec2(px.x,0),depth,c,contact)+opaqueNeighbour(uv+vec2(0,px.y),depth,c,contact)+opaqueNeighbour(uv-vec2(0,px.y),depth,c,contact))*.25;
 c=mix(c,avg,.45*smoothstep(.045,.22,length(c-avg)));}
 }

 if(uUnderwater!=0&&depth<.999999){
  float fog=1.0-exp(viewPosition(uv,depth).z*.155);
  c=mix(c,vec3(.046,.185,.218)*mix(.33,1.0,uDaylight),fog);
 }
 vec3 waterColor;float waterCoverage;
 if(waterSurface(uv,waterColor,waterCoverage))c=mix(c,pow(aces(waterColor*uExposure),vec3(1.0/2.2)),waterCoverage);
 frag=vec4(c,1.0);
}
