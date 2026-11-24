uniform vec2 uDepthRange;
float cameraDepth(float depth){
 return uDepthRange.x*uDepthRange.y/(uDepthRange.x+(1.0-depth)*(uDepthRange.y-uDepthRange.x));
}
