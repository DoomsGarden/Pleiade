uniform sampler2D uCloudMap;
uniform int uCloudCached;
vec4 cachedCloud(vec3 direction){
 vec3 d=normalize(direction);vec2 size=vec2(textureSize(uCloudMap,0));
 vec2 at=vec2(fract(atan(d.z,d.x)/6.28318530718),asin(clamp(d.y,-1.0,1.0))/3.14159265359+.5)*size-.5;
 ivec2 base=ivec2(floor(at)),dim=ivec2(size);vec2 f=fract(at);
 ivec2 a=ivec2((base.x+dim.x)%dim.x,clamp(base.y,0,dim.y-1)),b=ivec2((base.x+1+dim.x)%dim.x,clamp(base.y+1,0,dim.y-1));
 return mix(mix(texelFetch(uCloudMap,a,0),texelFetch(uCloudMap,ivec2(b.x,a.y),0),f.x),mix(texelFetch(uCloudMap,ivec2(a.x,b.y),0),texelFetch(uCloudMap,b,0),f.x),f.y);
}
vec3 cloudSky(vec3 background,vec3 origin,vec3 direction){
 if(uCloudAmount<=0.0)return background;
 vec4 cloud=uCloudCached!=0?cachedCloud(direction):cloudVolume(origin,direction);
 return cloud.rgb+background*cloud.a;
}
