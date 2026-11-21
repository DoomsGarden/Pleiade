uniform sampler2D uWaterDepth;
uniform vec3 uDrift[10];
uniform int uUnderwater;
uniform float uFlowTime;
uniform vec3 uWaterContacts[10];
uniform int uContactCount;
bool waterSurface(vec2 screen,out vec3 result,out float coverage){
 if(uWater==0)return false;
 float depth=texture(uDepth,screen).r, surfaceDepth=texture(uWaterDepth,screen).r;
 if(surfaceDepth>=.999999 || surfaceDepth>=depth-.0000002)return false;
 vec3 point=fromView(viewPosition(screen,surfaceDepth));
 float slope=0.0;
 if(uWaterMode!=0)slope=.010;
 if(uWaterMode==2){
  float s=clamp((point.z+2.5)/4.2,0.0,1.0);
  slope+=3.65*6.0*s*(1.0-s)/4.2;
 }
 vec3 wave=waveField(point.xz);
 float contactFoam=0.0;
 for(int i=0;i<10;i++){
  if(i>=uContactCount)break;
  vec3 obstacle=uWaterContacts[i];
  vec2 delta=point.xz-obstacle.xy;
  if(uWaterMode!=0)delta.y*=delta.y<0.0?.71:1.35;
  float radius=length(delta),edge=radius-obstacle.z;
  float spread=exp(-max(0.0,edge)*2.3)*smoothstep(-.10,.09,edge);
  float phase=21.0*edge-uFlowTime*5.2+float(i)*2.31;
  float breakup=sin(delta.x*52.0+delta.y*27.0+uFlowTime*3.0)*.18;
  float energy=max(uWaveStrength,uWaterMode==0?0.0:.38);
  wave.yz+=normalize(delta+vec2(.00001))*.095*energy*spread*cos(phase);
  float crest=smoothstep(.52,.88,sin(phase)+breakup);
  contactFoam=max(contactFoam,spread*crest*energy*.63);
 }
 vec3 n=normalize(vec3(-wave.y,1.0,-slope-wave.z)),v=normalize(uCameraEye-point);
 bool below=dot(v,n)<0.0;
 if(below)n=-n;
 vec3 reflected=reflect(-v,n);
 float path=depth<.999999?max(.0,surfaceDepth<depth?length(fromView(viewPosition(screen,depth))-point):.0):12.0;
 vec2 refracted=clamp(screen+n.xz*.010*min(path,1.6),vec2(.001),vec2(.999));
 float rd=texture(uDepth,refracted).r;
 if(rd>=.999999)refracted=screen;
 else{
  vec3 hit=fromView(viewPosition(refracted,rd));
  if(hit.y>waterLevel(hit.z)+waveField(hit.xz).x+.005||rd<surfaceDepth)refracted=screen;
 }
 vec3 bed=texture(uDirect,refracted).rgb+texture(uAmbient,refracted).rgb;
 vec3 trans=exp(-(vec3(.51,.25,.17)+uTurbidity)*min(path,15.0));
 vec3 body=bed*trans+vec3(.021,.084,.105)*(1.0-trans)*mix(.11,1.0,uDaylight);
 vec3 environment=cloudSky(celestialSky(reflected),point,reflected);
 vec2 hitUV;float confidence;
 if(!below&&uWaterSSR!=0&&waterReflection(point+n*.06,reflected,hitUV,confidence)){
  environment=mix(environment,texture(uDirect,hitUV).rgb+texture(uAmbient,hitUV).rgb,confidence);
 }
 float fresnel=waterFresnel(dot(n,v));
 vec3 h=normalize(v+uSkyLightDirection);
 float nt=max(dot(n,h),0.0);
 float glint=pow(nt,mix(560.0,160.0,uWaveStrength))*.68
             +pow(nt,mix(140.0,38.0,uWaveStrength))*.085;
 float foam=contactFoam;
 if(uWaterMode!=0&&!below){
  vec2 uv=vec2(point.x-channelCenter(point.z),point.z);
  // Foam advects in channel space even with the free-surface waves disabled.
  float cascade=smoothstep(.18,.66,slope);
  float downstream=uv.y+uFlowTime*mix(1.35,3.7,cascade);
  float stream=sin(uv.x*18.0+sin(downstream*.73)*1.7)
               +.59*sin(uv.x*33.0-downstream*1.45)+.36*sin(downstream*11.0+uv.x*3.0);
  float feather=max(.11,fwidth(stream)*.67);
  float whitewater=smoothstep(.8-feather,1.29+feather,stream);
  float bubbles=.045*smoothstep(.47,.8,sin(downstream*3.4+sin(uv.x*6.3)));
  foam=max(foam,bubbles+cascade*(.09+.53*whitewater));
  if(uWaterMode==2){
   float fallDistance=(uv.y+2.85)/1.33;
   float foot=exp(-fallDistance*fallDistance)*(1.0-smoothstep(1.0,4.3,abs(uv.x)));
   float agitation=sin(uv.x*27.0+cos(downstream*13.0))+sin(downstream*17.0-uv.x*6.2);
   foam=max(foam,foot*(.32+.54*smoothstep(-.43,1.15,agitation)));
  }
  for(int i=0;i<10;i++){
   vec2 d=point.xz-uDrift[i].xz;
   foam=max(foam,.42*(1.0-smoothstep(.18,1.0,length(d*vec2(5.6,2.05)))));
  }
 }
 coverage=smoothstep(.003,.070,path);
 if(below){
  result=mix(vec3(.013,.050,.058),environment,.45)*mix(.13,.65,uDaylight);
 }else{
  result=mix(body,environment,fresnel)+uLightColor*glint;
  result=mix(result,vec3(.51,.65,.70)*mix(.16,1.0,uDaylight),foam);
 }
 return true;
}
