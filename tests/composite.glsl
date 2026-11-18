#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
in vec2 uv;out vec4 frag;
uniform sampler2D uDirect;uniform sampler2D uAmbient;uniform sampler2D uDepth;
uniform float uExposure;
uniform float uTurbidity;uniform int uAA;
uniform sampler2D uNormal;uniform int uContact;uniform vec2 uProjectionScale;
uniform int uWater,uWaterSSR;uniform float uWaterTime;uniform float uWaveStrength;
uniform vec3 uCameraEye,uCameraRight,uCameraUp,uCameraForward;
// Perspective reconstruction uses Camera's shared 0.1 / 250.0 metre clipping planes.
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
uniform vec3 uSunDirection;
uniform vec3 uMoonDirection;
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
 vec4 cloud=uCloudCached!=0?cachedCloud(direction):cloudVolume(origin,direction);
 return cloud.rgb+background*cloud.a;
}
vec3 celestialSky(vec3 direction){
 vec3 d=normalize(direction);float h=max(d.y,0.0);
 vec3 night=mix(vec3(.006,.009,.018),vec3(.0007,.0015,.004),sqrt(h));
 vec3 day=mix(vec3(.38,.49,.60),vec3(.045,.18,.42),sqrt(h));
 float dusk=exp(-abs(uSunDirection.y)*12.0)*(1.0-h)*(1.0-h);
 vec3 color=mix(night,day,uDaylight)+vec3(.48,.115,.025)*dusk*pow(max(dot(d,uSunDirection)*.5+.5,0.0),6.0);
 float lat=uLatitude;vec3 q=vec3(d.x,d.y*cos(lat)+d.z*sin(lat),-d.y*sin(lat)+d.z*cos(lat));
 float a=uStarAngle; q=vec3(cos(a)*q.x+sin(a)*q.y,-sin(a)*q.x+cos(a)*q.y,q.z);
 vec2 uv=vec2(fract(atan(q.y,q.x)/6.28318530718),asin(clamp(q.z,-1.0,1.0))/3.14159265359+.5);
 color+=pointStars(q)*(1.0-smoothstep(0.0,.3,uDaylight))*smoothstep(0.0,.12,d.y);
 float sunDot=dot(d,uSunDirection),moonDot=dot(d,uMoonDirection);
 float aa=max(uSkyPixelAngle*.006,.0000001);
 float sunRadius=.006,sunEdge=cos(sunRadius),mu=sqrt(clamp(1.0-(1.0-sunDot*sunDot)/(sunRadius*sunRadius),0.0,1.0));
 vec3 sunNormal=(d-uSunDirection*sunDot)/sunRadius;
 float granulation=.94+.06*skyNoise(sunNormal*70.0);
 color+=vec3(12.0,9.0,5.5)*(.4+.6*mu)*granulation*smoothstep(sunEdge-aa,sunEdge+aa,sunDot)*smoothstep(-.02,.01,d.y);
 // Fixed surface coordinates on the fictional moon; sunlight defines the terminator.
 float radius=.007,disc=1.0-moonDot*moonDot;
 if(moonDot>0.0&&disc<radius*radius){
 vec3 tangent=normalize(cross(abs(uMoonDirection.y)>.95?vec3(1,0,0):vec3(0,1,0),uMoonDirection)),up=cross(uMoonDirection,tangent);
 vec2 xy=vec2(dot(d,tangent),dot(d,up))/radius;float z=sqrt(max(0.0,1.0-dot(xy,xy)));vec3 surface=vec3(xy,z);
 float relief=moonRelief(surface),e=.003;
 vec2 gradient=vec2(moonRelief(normalize(surface+vec3(e,0,0)))-relief,moonRelief(normalize(surface+vec3(0,e,0)))-relief)/e;
 vec3 normal=normalize(tangent*(xy.x-gradient.x*.045)+up*(xy.y-gradient.y*.045)-uMoonDirection*z);
 float incidence=max(dot(normal,uSunDirection),0.0);
 float maria=smoothstep(.36,.65,skyNoise(surface*4.0));float albedo=mix(.18,.42,maria)+relief;
 vec3 lunar=vec3(.94,.93,.89)*albedo*(.006+incidence*2.0);
 color=mix(color,lunar,smoothstep(0.0,.03,d.y));
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
 vec3 reflection=cloudSky(celestialSky(reflected),point,reflected);
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
 float waterPath=min(30.0,waterDepth/max(dot(n,v),.12));
 vec3 transmission=exp(-(vec3(.65,.27,.18)+vec3(uTurbidity))*waterPath);
 vec3 body=bed*transmission+vec3(.018,.085,.075)*(1.0-transmission)*uSkyAmbient;
 float fresnel=waterFresnel(dot(n,v));
 vec3 halfVector=normalize(v+uSkyLightDirection);
 vec3 sun=uLightColor*pow(max(dot(n,halfVector),0.0),512.0)*.8;
 result=mix(body,reflection,fresnel)+sun;return true;
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
 vec3 waterColor;float waterCoverage;
 if(waterSurface(uv,waterColor,waterCoverage))c=mix(c,pow(aces(waterColor*uExposure),vec3(1.0/2.2)),waterCoverage);
 frag=vec4(c,1.0);
}
