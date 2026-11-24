#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
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
const float cloudReach=6000.0;
const vec4 clearCloud=vec4(0.0,0.0,0.0,1.0);
vec2 cloudHeights(int layer){return layer==0?vec2(900.0,1700.0):vec2(3200.0,3650.0);}
vec2 cloudInterval(float y,float dy,vec2 heights){
 if(abs(dy)<.000001)return y>heights.x&&y<heights.y?vec2(0.0,cloudReach):vec2(0.0);
 vec2 hits=(heights-y)/dy;
 return clamp(vec2(min(hits.x,hits.y),max(hits.x,hits.y)),0.0,cloudReach);
}
vec4 cloudVolume(vec3 origin,vec3 d,float distance){
 if(uCloudAmount<=0.0)return clearCloud;
 vec3 accum=vec3(0.0);float trans=1.0;
 // A bounded atmospheric volume: two height layers, integrated front to back.
 for(int layer=0;layer<2;layer++){
 vec2 heights=cloudHeights(d.y>=0.0?layer:1-layer),interval=cloudInterval(origin.y,d.y,heights);
 float end=min(distance,interval.y);if(end<=interval.x)continue;
 // Keep the same samples up to the last, possibly partial, step.
 float stepSize=(interval.y-interval.x)/24.0;
 float jitter=.25+.5*skyHash(floor(d*2000.0));
 for(int i=0;i<24;i++){
 float start=interval.x+float(i)*stepSize;
 float segment=min(stepSize,end-start);if(segment<=0.0)break;
 vec3 pos=origin+d*(start+jitter*segment);
 float extinction=cloudDensity(pos,heights.x,heights.y)*.004;
 float alpha=1.0-exp(-extinction*segment);
 float toward=cloudDensity(pos+uSkyLightDirection*220.0,heights.x,heights.y);
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
 vec4 cloud=uCloudCached!=0&&nearObserver?cachedCloud(direction):cloudVolume(origin,normalize(direction),cloudReach);
 return cloud.rgb+background*cloud.a;
}
vec4 cloudToSurface(vec3 origin,vec3 ray,float distance){
 if(uCloudAmount<=0.0)return clearCloud;
 vec2 lower=cloudInterval(origin.y,ray.y,cloudHeights(0));
 vec2 upper=cloudInterval(origin.y,ray.y,cloudHeights(1));
 bool hitsLower=lower.x<lower.y,hitsUpper=upper.x<upper.y;
 if((!hitsLower||distance<=lower.x)&&(!hitsUpper||distance<=upper.x))return clearCloud;
 // The panorama represents the entire ray. A surface can reuse it only
 // when it is behind every intersected layer.
 if(uCloudCached!=0&&(!hitsLower||distance>=lower.y)&&(!hitsUpper||distance>=upper.y))return cachedCloud(ray);
 return cloudVolume(origin,ray,distance);
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
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.0,1.0);}
uniform vec2 uDepthRange;
float cameraDepth(float depth){
 return uDepthRange.x*uDepthRange.y/(uDepthRange.x+(1.0-depth)*(uDepthRange.y-uDepthRange.x));
}
in vec3 rasterPosition;
in vec3 worldNormal;
out vec4 frag;
uniform sampler2D uOpaqueDepth;
uniform vec3 uEye;
uniform float uExposure;
void main(){
 ivec2 at=ivec2(gl_FragCoord.xy);
 float opaque=texelFetch(uOpaqueDepth,at,0).r;
 if(opaque<.999999&&cameraDepth(gl_FragCoord.z)>=cameraDepth(opaque)-.004)discard;
 vec3 n=normalize(worldNormal);
 vec3 v=normalize(uEye-rasterPosition);
 // The pane has negligible distortion. Its transmitted color comes from the actual interior.
 vec3 reflected=reflect(-v,n);
 float horizon=smoothstep(-.08,.012,reflected.y);
 vec3 exterior=vec3(.12,.19,.21)*mix(.025,1.0,uDaylight);
 if(horizon>0.0){
  vec3 sky=cloudSky(celestialSky(reflected),rasterPosition,reflected);
  exterior=mix(exterior,sky,horizon);
 }
 // Both air-glass interfaces contribute when the panes reflect an exposed exterior.
 float fresnel=.08+.92*pow(1.0-clamp(dot(n,v),0.0,1.0),5.0);
 vec3 displayColor=pow(aces(exterior*uExposure),vec3(1.0/2.2));
 frag=vec4(displayColor,mix(.055,1.0,fresnel));
}
