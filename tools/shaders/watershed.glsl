uniform sampler2D uWaterDepth;
uniform vec3 uDrift[10];
uniform int uUnderwater;
uniform float uFlowTime;
uniform vec3 uWaterContacts[10];
uniform int uContactCount;

// Integral of the local slowness along the channel. Foam travels with the
// current and speeds up over the lip. Elapsed time cannot invert the flow.
float channelTravel(float z){
 if(uWaterMode!=2)return z/1.39;
 float section=clamp(z+3.15,0.0,5.15);
 return (z-section)/1.41+section/3.55;
}
vec2 waterFlow(vec2 p){
 if(uWaterMode==0)return normalize(vec2(1.0,-shoreBankSlope(p.y,true)));
 return normalize(vec2(-channelSlope(p.y),-1.0));
}
float obstacleFoam(vec2 at,vec2 delta,float radius,float time,float strength,float footprint){
 vec2 flow=waterFlow(at),across=vec2(-flow.y,flow.x);
 float downstream=dot(delta,flow),side=dot(delta,across);
 // Air entrainment is strongest where the water meets the rock and fades
 // along the wake. The wake widens as the bubbles leave the obstruction.
 float lengthScale=max(.36,radius*2.7);
 float width=max(.12,radius*.63)+max(0.0,downstream)*.28;
 float wake=exp(-abs(downstream-radius*.20)/lengthScale-pow(abs(side)/width,1.65));
 wake*=smoothstep(-radius*1.8,-radius*.5,downstream);
 float heartbeat=uWaterMode==0?.46+.54*smoothstep(.02,.96,sin(swellPhase(shoreCoordinates(at,true),time))):.77;
 float density=wake*strength*heartbeat;
 if(density<.026)return 0.0;
 vec2 moving=vec2(side,downstream-uFlowTime*(uWaterMode==0?.64:1.41));
 return .83*foamLace(moving,density,footprint);
}
float channelFoam(vec2 p,float slope,float footprint){
 float side=p.x-channelCenter(p.y),stream=channelTravel(p.y)+uFlowTime;
 float cascade=smoothstep(.20,.83,slope);
 vec2 advected=vec2(side,stream);
 float white=0.0;
 if(cascade>.01){
  float sheet=foamNoise(vec2(side*4.6,stream*1.65)+2.9);
  white=.69*cascade*foamLace(advected*vec2(1.06,.86),cascade*(.40+.53*sheet),footprint);
 }
 if(uWaterMode==2){
  // The plume begins at the foot and expands downstream; it is fed
  // continuously by new foam but its texture travels and breaks apart.
  float distance=-2.30-p.y;
  float width=1.55+clamp(distance,0.0,4.0)*.24;
  float plume=exp(-max(0.0,distance)*.51)*smoothstep(-.84,.32,distance);
  plume*=1.0-smoothstep(width*.54,width*1.75,abs(side));
  if(plume>.014){
   float entrainment=.66+.34*foamNoise(advected*vec2(1.6,1.72)+9.4);
   white=max(white,.92*foamLace(advected,plume*entrainment,footprint));
  }
 }
 for(int i=0;i<10;i++){
  vec2 d=p-uDrift[i].xz;
  float radius=dot(d*vec2(3.9,1.7),d*vec2(3.9,1.7));
  if(radius>=1.0)continue;
  float density=(1.0-smoothstep(.15,1.0,radius))*.44;
  white=max(white,.66*foamLace(d+float(i)*7.7,density,footprint));
 }
 return white;
}
bool waterSurface(vec2 screen,out vec3 result,out float coverage){
 if(uWater==0)return false;
 float depth=texture(uDepth,screen).r,surfaceDepth=texture(uWaterDepth,screen).r;
 if(surfaceDepth>=.999999||surfaceDepth>=depth-.0000002)return false;
 vec3 point=fromView(viewPosition(screen,surfaceDepth));
 float slope=waterSlope(point.z);
 vec3 wave=waveField(point.xz);
 float footprint=length(point-uCameraEye)*uSkyPixelAngle;
 float foam=0.0;
 for(int i=0;i<10;i++){
  if(i>=uContactCount)break;
  vec3 obstacle=uWaterContacts[i];
  vec2 delta=point.xz-obstacle.xy;
  float distance=length(delta);
  if(distance>obstacle.z+2.0)continue;
  float edge=distance-obstacle.z;
  float spread=exp(-max(0.0,edge)*2.3)*smoothstep(-.10,.09,edge);
  float phase=21.0*edge-uFlowTime*5.2+float(i)*2.31;
  float energy=max(uWaveStrength,uWaterMode==0?0.0:.43);
  float clear=1.0-smoothstep(.025,.17,footprint);
  wave.yz+=delta/max(distance,.001)*.095*energy*spread*cos(phase)*clear;
  foam=max(foam,obstacleFoam(obstacle.xy,delta,obstacle.z,uWaterTime,energy,footprint));
 }
 // High frequency slopes fade with distance; the broad reflection follows the wave.
 float rippleFilter=mix(1.0,.30,smoothstep(.05,.26,footprint));
 vec3 n=normalize(vec3(-wave.y*rippleFilter,1.0,-slope-wave.z*rippleFilter));
 vec3 v=normalize(uCameraEye-point);
 bool below=dot(v,n)<0.0;
 if(below)n=-n;
 vec3 reflected=reflect(-v,n);
 vec3 receiver=depth<.999999?fromView(viewPosition(screen,depth)):point-v*12.0;
 float rayPath=length(receiver-point),path=rayPath;
 // Reproject a short segment of the refracted ray in camera space.
 vec3 bent=refract(-v,n,below?1.333:1.0/1.333);
 vec3 sampleView=toView(point+bent*min(path,1.65));
 vec2 shift=sampleView.xy/(max(.1,-sampleView.z))*uProjectionScale*.5+.5-screen;
 vec2 refracted=clamp(screen+clamp(shift,vec2(-.034),vec2(.034)),vec2(.001),vec2(.999));
 float rd=texture(uDepth,refracted).r,refractWeight=0.0;
 if(rd<.999999&&rd>surfaceDepth&&dot(bent,bent)>.01){
  vec3 hit=fromView(viewPosition(refracted,rd));
  float depthBelow=waterHeight(hit.xz)-hit.y;
  float balance=length(hit-point)/max(.01,rayPath);
  refractWeight=smoothstep(.008,.20,depthBelow)*smoothstep(.025,.25,rayPath)
                *(1.0-smoothstep(1.25,2.0,balance));
  path=mix(rayPath,length(hit-point),refractWeight);
 }
 vec3 bed=texture(uDirect,screen).rgb+texture(uAmbient,screen).rgb;
 if(refractWeight>.001){
  vec3 displaced=texture(uDirect,refracted).rgb+texture(uAmbient,refracted).rgb;
  bed=mix(bed,displaced,refractWeight);
 }
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
 float sharpness=mix(425.0,105.0,max(uWaveStrength,smoothstep(.04,.23,footprint)))*mix(1.0,.58,smoothstep(.1,.9,slope));
 float glint=pow(nt,sharpness)*.61+pow(nt,max(35.0,sharpness*.22))*.071;
 coverage=smoothstep(.003,.070,rayPath);
 if(below){
  result=mix(vec3(.013,.050,.058),environment,.45)*mix(.13,.65,uDaylight);
 }else{
  foam=max(foam,uWaterMode==0?lakeFoam(point.xz,uWaterTime,uWaveStrength,true,footprint)
                            :channelFoam(point.xz,slope,footprint));
  coverage=max(coverage,foam*smoothstep(.004,.052,rayPath));
  vec3 water=mix(body,environment,fresnel)+uLightColor*glint;
  result=applyWaterFoam(water,foam,uLightColor,uSkyAmbient,max(dot(n,uSkyLightDirection),0.0));
 }
 return true;
}
