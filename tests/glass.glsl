#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
in vec3 rasterPosition;
in vec3 worldNormal;
in vec3 rasterLocal;
out vec4 frag;
uniform sampler2D uOpaqueDepth;
uniform vec3 uEye,uSkyLightDirection;
uniform float uDaylight;
void main(){
 ivec2 at=ivec2(gl_FragCoord.xy);
 float opaque=texelFetch(uOpaqueDepth,at,0).r;
 if(gl_FragCoord.z>=opaque-.000002)discard;
 vec3 n=normalize(worldNormal);
 vec3 v=normalize(uEye-rasterPosition);
 // The pane has negligible distortion. Its transmitted color comes from the actual interior.
 vec3 reflected=reflect(-v,n);
 float horizon=smoothstep(-.28,.11,reflected.y);
 vec3 sky=mix(vec3(.65,1.02,1.44),vec3(.34,.62,1.07),sqrt(max(0.0,reflected.y)));
 vec3 exterior=mix(vec3(.12,.19,.21),sky,horizon)*mix(.025,1.0,uDaylight);
 float glint=pow(max(0.0,dot(reflected,uSkyLightDirection)),190.0);
 exterior+=glint*.31;
 // Both air-glass interfaces contribute when the panes reflect an exposed exterior.
 float fresnel=.08+.92*pow(1.0-clamp(dot(n,v),0.0,1.0),5.0);
 frag=vec4(exterior,mix(.055,1.0,fresnel));
}
