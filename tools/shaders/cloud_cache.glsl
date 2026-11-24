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
 // Reflections viewed from high above the surface sample the clouds from the shading point.
 bool nearObserver=abs(origin.y-uSkyObserver.y)<96.0;
 vec4 cloud=uCloudCached!=0&&nearObserver?cachedCloud(direction):cloudVolume(origin,normalize(direction),cloudReach);
 return cloud.rgb+background*cloud.a;
}
vec4 cloudToSurface(vec3 origin,vec3 ray,float distance){
 if(uCloudAmount<=0.0)return clearCloud;
 vec2 lower=cloudInterval(origin.y,ray.y,cloudHeights(0));
 vec2 upper=cloudInterval(origin.y,ray.y,cloudHeights(1));
 bool hitsLower=lower.x<lower.y,hitsUpper=upper.x<upper.y;
 if((!hitsLower||distance<=lower.x)&&(!hitsUpper||distance<=upper.x))return clearCloud;
 // The panorama represents the entire ray. A surface can reuse it only
 // when it is behind every intersected layer.
 if(uCloudCached!=0&&(!hitsLower||distance>=lower.y)&&(!hitsUpper||distance>=upper.y))return cachedCloud(ray);
 return cloudVolume(origin,ray,distance);
}
