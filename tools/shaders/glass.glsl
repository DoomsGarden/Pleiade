in vec3 rasterPosition;
in vec3 worldNormal;
out vec4 frag;
uniform sampler2D uOpaqueDepth;
uniform vec3 uEye;
uniform vec3 uGlassForward;
uniform int uOrthographic;
uniform float uExposure;
void main(){
 ivec2 at=ivec2(gl_FragCoord.xy);
 float opaque=texelFetch(uOpaqueDepth,at,0).r;
 if(gl_FragCoord.z>=opaque-.000002)discard;
 vec3 n=normalize(worldNormal);
 vec3 v=uOrthographic!=0?-uGlassForward:normalize(uEye-rasterPosition);
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
 vec3 displayColor=pow(aces(exterior*uExposure),vec3(1.0/2.2));
 frag=vec4(displayColor,mix(.055,1.0,fresnel));
}
