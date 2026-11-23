#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;

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
uniform float uWaterTime;
uniform float uWaveStrength;
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
layout(location=0) in vec3 aPosition;
uniform mat4 uVP;
void main(){
 vec3 p=aPosition;
 p.y+=waveField(p.xz).x;
 gl_Position=uVP*vec4(p,1.0);
}
