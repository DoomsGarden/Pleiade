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
