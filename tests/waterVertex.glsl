#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
uniform float uWaterTime;
uniform float uWaveStrength;
uniform int uWaterMode;
float channelCenter(float z){return -3.3+3.1*sin(z*.056)+1.3*sin(z*.133+.9);}
float waterLevel(float z){
 return uWaterMode==0?0.0:.44+z*.010+(uWaterMode==2?3.65*smoothstep(-2.5,1.7,z):0.0);
}
vec3 waveField(vec2 p){
 float t=uWaterTime, run=uWaterMode==0?.0:.85;
 vec2 k1=vec2(.748,.395),k2=vec2(-.47,1.94),k3=vec2(2.79,-1.61),k4=vec2(6.8,3.87);
 float a=dot(p,k1)+t*(1.04+run*.395),b=dot(p,k2)+t*(1.72+run*1.94);
 float c=dot(p,k3)+t*(2.88-run*1.61),d=dot(p,k4)+t*(3.94+run*3.87);
 float h=.038*sin(a)+.017*sin(b)+.010*sin(c)+.0035*sin(d);
 vec2 slope=.038*k1*cos(a)+.017*k2*cos(b)+.010*k3*cos(c)+.0035*k4*cos(d);
 return vec3(h,slope)*uWaveStrength;
}
layout(location=0) in vec3 aPosition;
uniform mat4 uVP;
void main(){
 vec3 p=aPosition;
 p.y+=waveField(p.xz).x;
 gl_Position=uVP*vec4(p,1.0);
}
