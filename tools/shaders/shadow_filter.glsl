uniform int uShadowSoft;
float visibility(vec3 n){
 if(uShadows==0)return 1.0;
 vec4 p=uLightVP*vec4(worldPosition,1.0);vec3 q=p.xyz/p.w*.5+.5;
 vec3 dx=dFdx(q),dy=dFdy(q);
 float determinant=dx.x*dy.y-dx.y*dy.x;
 vec2 gradient=vec2(0.0);
 bool regular=abs(determinant)>1e-12;
 if(regular)gradient=vec2(dy.y*dx.z-dx.y*dy.z,dx.x*dy.z-dy.x*dx.z)/determinant;
 if(q.z<0.0||q.z>1.0||any(lessThan(q.xy,vec2(0.0)))||any(greaterThan(q.xy,vec2(1.0))))return 1.0;
 float bias=regular?.000005:max(.0004*(1.0-dot(n,lightDirection)),.00012);
 vec2 size=vec2(textureSize(uShadow,0)),base=floor(q.xy*size);
 vec2 phase=q.xy*size-base-.5;
 float sum=0.0,total=0.0;
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
   vec2 offset=vec2(float(x),float(y)),centre=(base+offset+.5)/size;
   // The outer weight vanishes before the 3x3 neighbourhood changes.
   // This keeps the filter continuous across texel boundaries with nine reads.
   vec2 tent=max(vec2(0.0),vec2(1.5)-abs(offset-phase));
   float weight=uShadowSoft!=0?tent.x*tent.y:1.0;
   // Silhouettes and nearly edge-on surfaces can give unstable plane slopes.
   float receiver=q.z+clamp(dot(gradient,centre-q.xy),-.007,.007);
   float visible=(receiver-bias<=texture(uShadow,centre).r)?1.0:0.0;
   if(any(lessThan(centre,vec2(0.0)))||any(greaterThan(centre,vec2(1.0))))visible=1.0;
   sum+=weight*visible;total+=weight;
 }
 vec2 padding=min(q.xy,1.0-q.xy);
 float insideMap=smoothstep(0.0,.055,min(padding.x,padding.y));
 return mix(1.0,sum/max(total,.00001),insideMap);
}
