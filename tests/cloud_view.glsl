#version 300 es
precision highp float;precision highp int;precision highp sampler2D;
in vec2 uv;out vec4 frag;
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

void main(){vec3 ray=normalize(vec3((uv.x-.5)*2.0,uv.y*1.2-.1,1.0));vec3 c=cloudSky(vec3(.12,.25,.45),uSkyObserver,ray);c=clamp((c*(2.51*c+.03))/(c*(2.43*c+.59)+.14),0.0,1.0);frag=vec4(pow(c,vec3(1.0/2.2)),1.0);}