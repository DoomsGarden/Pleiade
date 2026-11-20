#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
uniform vec3 uCameraEye;
uniform mat4 uVP;
uniform float uTime,uWind,uScreenHeight;
uniform int uWeatherKind;
uniform vec3 uLeafOrigin;
out float variation;
float randomValue(float x){return fract(sin(x*12.9898)*43758.5453);}
void main(){
 float id=float(gl_VertexID);
 float a=randomValue(id+2.0),b=randomValue(id+91.0),c=randomValue(id+810.0);
 float rate=uWeatherKind==2?.23:uWeatherKind==1?1.67:.20;
 float fall=fract(a+uTime*rate*(.66+.45*b));
 vec3 p=uWeatherKind==3?uLeafOrigin:vec3(uCameraEye.x,clamp(uCameraEye.y,0.0,15.0),uCameraEye.z);
 float radius=uWeatherKind==3?3.0:16.0;
 p.x+=(b-.5)*radius+fall*uWind*.94;
 p.z+=(c-.5)*radius+sin(fall*6.28318+a*9.0)*(uWeatherKind==3?.36:.12);
 p.y+=(uWeatherKind==3?3.7:5.8)-fall*(uWeatherKind==3?5.9:13.0);
 gl_Position=uVP*vec4(p,1.0);
 gl_PointSize=clamp((uWeatherKind==3?12.0:8.0)*(uScreenHeight/640.0)/max(gl_Position.w*.38,1.0),1.0,11.0);
 variation=c;
}
