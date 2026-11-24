in vec3 rasterPosition;
in vec3 worldNormal;
out vec4 frag;
uniform sampler2D uOpaqueDepth;
uniform vec3 uEye;
uniform float uExposure;
void main(){
 ivec2 at=ivec2(gl_FragCoord.xy);
 float opaque=texelFetch(uOpaqueDepth,at,0).r;
 if(opaque<.999999&&cameraDepth(gl_FragCoord.z)>=cameraDepth(opaque)-.004)discard;
 vec3 n=normalize(worldNormal);
 vec3 toEye=uEye-rasterPosition;
 vec3 v=normalize(toEye);
 // The pane has negligible distortion. Its transmitted color comes from the actual interior.
 vec3 reflected=reflect(-v,n);
 float horizon=smoothstep(-.08,.012,reflected.y);
 vec3 exterior=vec3(.12,.19,.21)*mix(.025,1.0,uDaylight);
 if(horizon>0.0){
  vec3 sky=cloudSky(celestialSky(reflected),rasterPosition,reflected);
  exterior=mix(exterior,sky,horizon);
 }
 // Both air-glass interfaces contribute when the panes reflect an exposed exterior.
 float fresnel=.08+.92*pow(1.0-clamp(dot(n,v),0.0,1.0),5.0);
 // The composite already contains the atmospheric path to the transmitted background.
 vec4 cloud=cloudToSurface(uEye,-v,length(toEye));
 vec3 displayColor=pow(aces((cloud.rgb+exterior*cloud.a)*uExposure),vec3(1.0/2.2));
 frag=vec4(displayColor,mix(.055,1.0,fresnel));
}
