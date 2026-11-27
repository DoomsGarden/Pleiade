#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
uniform sampler2D uOpaqueDepth,uWaterDepth;
uniform int uWeatherKind,uWeatherWater;
in float variation;
in float particleOpacity;
out vec4 frag;
void main(){
 ivec2 pixel=ivec2(gl_FragCoord.xy);
 if(gl_FragCoord.z>=texelFetch(uOpaqueDepth,pixel,0).r-.000002)discard;
 if(uWeatherWater!=0){
  float surfaceDepth=texelFetch(uWaterDepth,pixel,0).r;
  if(surfaceDepth<.999999&&gl_FragCoord.z>=surfaceDepth)discard;
 }
 vec2 p=gl_PointCoord*2.0-1.0;
 float shape=length(p*(uWeatherKind==1?vec2(3.8,.85):uWeatherKind==3?vec2(.93,1.45):vec2(1.0)));
 float alpha=(1.0-smoothstep(.22,1.0,shape))*(uWeatherKind==3?.82:.46)*particleOpacity;
 vec3 color=uWeatherKind==3?mix(vec3(.65,.33,.10),vec3(.89,.64,.20),variation):vec3(.83,.90,.96);
 frag=vec4(color,alpha);
}
