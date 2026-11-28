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
 return 1.10*(q.x+3.2*shoreShallow(q.x))+.115*q.y+.30*sin(q.y*.25)-1.48*time;
}
vec3 lakeWave(vec2 p,float time,float strength,bool curved){
 if(strength==0.0)return vec3(0.0);
 vec2 q=shoreCoordinates(p,curved);
 float shallow=shoreShallow(q.x), delta=.44*shallow*(1.0-shallow);
 float phase=swellPhase(q,time);
 float secondary=2.04*(q.x+1.3*shallow)-.36*q.y-2.55*time+.77;
 float ripple=3.58*q.x+.74*q.y-3.75*time;
 float group=q.y*.25+q.x*.19-time*.32;
 float envelope=.88+.12*cos(group);
 float base=.037+.016*shallow;
 float amplitude=base*envelope;
 // Dominant long crests carry the displacement. Shorter waves add detail
 // without folding the reflection into similarly strong intersecting cells.
 float height=amplitude*sin(phase)+.008*sin(secondary)+.0017*sin(ripple);
 float dx=(.016*delta*envelope-base*.0228*sin(group))*sin(phase)+amplitude*1.10*(1.0+3.2*delta)*cos(phase)
          +.008*2.04*(1.0+1.3*delta)*cos(secondary)+.006086*cos(ripple);
 float dz=amplitude*(.115+.075*cos(q.y*.25))*cos(phase)-base*.030*sin(group)*sin(phase)
          -.00288*cos(secondary)+.001258*cos(ripple);
 return strength*vec3(height,dx,dz-shoreBankSlope(p.y,curved)*dx);
}
float recentShoreWater(vec2 p,float time,float strength,bool curved){
 float now=lakeWave(p,time,strength,curved).x;
 float last=lakeWave(p,max(0.0,time-.36),strength,curved).x-.018;
 float prior=lakeWave(p,max(0.0,time-.78),strength,curved).x-.042;
 return max(now,max(last,prior));
}
float channelCenter(float z){
 return -3.3+3.1*sin(z*.056)+1.3*sin(z*.133+.9);
}
float plungePool(float z){
 float downstream=-z-2.5;
 return smoothstep(0.0,3.8,downstream)*(1.0-smoothstep(4.0,13.5,downstream));
}
float channelWaterExtent(float z,int mode){
 return 4.52+(mode==2?2.4*plungePool(z):0.0);
}
float waterLevel(float z,int mode){
 return mode==0?0.0:.44+z*.010+(mode==2?3.65*smoothstep(-2.5,.85,z):0.0);
}
uniform float uWaterTime;
uniform float uWaveStrength;
uniform int uWaterMode;

float channelSlope(float z){
 return .1736*cos(z*.056)+.1729*cos(z*.133+.9);
}
float waterSlope(float z){
 if(uWaterMode==0)return 0.0;
 float s=clamp((z+2.5)/3.35,0.0,1.0);
 return .010+(uWaterMode==2?3.65*6.0*s*(1.0-s)/3.35:0.0);
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
