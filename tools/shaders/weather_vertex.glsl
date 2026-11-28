uniform vec3 uCameraEye;
uniform mat4 uVP;
uniform float uTime,uWind,uScreenHeight;
uniform int uWeatherKind;
uniform vec3 uLeafOrigin;
out float variation;
out float particleOpacity;
float randomValue(float x){return fract(sin(x*12.9898)*43758.5453);}
void main(){
 float id=float(gl_VertexID);
 float a=randomValue(id+2.0),b=randomValue(id+91.0),c=randomValue(id+810.0);
 vec3 p;
 if(uWeatherKind==3){
  float fall=fract(a+uTime*.20*(.66+.45*b));
  p=uLeafOrigin;
  p.x+=(b-.5)*3.0+fall*uWind*.94;
  p.z+=(c-.5)*3.0+sin(fall*6.28318+a*9.0)*.36;
  p.y+=3.7-fall*5.9;
  particleOpacity=1.0;
 }else{
  const vec3 extent=vec3(19.0,17.0,19.0);
  bool rain=uWeatherKind==1;
  float falling=rain?mix(5.7,9.0,b):mix(.45,1.2,b);
  float drift=rain?mix(.60,1.4,a):mix(.12,.55,a);
  float sway=sin(uTime*1.7+c*6.28318)*(rain?.035:.28);
  vec3 path=vec3(b-.5,5.8/17.0-a,c-.5)+vec3(drift*uTime,-falling*uTime,sway)/extent;

  // The trajectory is in world coordinates. The eye only selects the
  // repeating copy within the active box, never moving that copy with it.
  vec3 low=uCameraEye-vec3(9.5,11.2,9.5);
  vec3 offset=mod(fract(path)*extent-low,extent);
  p=low+offset;
  vec3 distanceToEdge=min(offset,extent-offset);
  vec3 fade=smoothstep(vec3(0.0),vec3(1.25,1.2,1.25),distanceToEdge);
  particleOpacity=fade.x*fade.y*fade.z;
 }
 gl_Position=uVP*vec4(p,1.0);
 gl_PointSize=clamp((uWeatherKind==3?12.0:8.0)*(uScreenHeight/640.0)/max(gl_Position.w*.38,1.0),1.0,11.0);
 variation=c;
}
