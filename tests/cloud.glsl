#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
in vec2 uv;out vec4 frag;
uniform vec3 uSunDirection;
uniform vec3 uMoonDirection;
uniform float uSunRadius;
uniform float uMoonRadius;
uniform vec3 uLightColor;
uniform vec3 uCloudLightColor;
uniform vec3 uSkyLightDirection;
uniform float uDaylight;
uniform float uSkyAmbient;
uniform float uCloudAmbient;
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
 // The cloud samples already estimate local extinction toward the light.
 vec3 scatter=vec3(.10,.14,.20)*uCloudAmbient+uCloudLightColor*.16*(light+.35*forward);
 accum+=trans*alpha*scatter;trans*=1.0-alpha;if(trans<.015)break;
 }}return vec4(accum,trans);
}

void main(){float lon=uv.x*6.28318530718,lat=(uv.y-.5)*3.14159265359;vec3 ray=vec3(cos(lat)*cos(lon),sin(lat),cos(lat)*sin(lon));frag=cloudVolume(uSkyObserver,ray,cloudReach);}