#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in mat4 aModel;
layout(location=6) in vec4 aColor;
layout(location=7) in vec4 aSurface;
uniform mat4 uVP;
uniform float uTime;
out vec3 rasterPosition;
out vec3 rasterLocal;
flat out vec2 coverage;
flat out mat4 objectModel;
flat out mat3 objectNormals;
flat out float atlasView;
out vec3 worldNormal;
out vec3 color;
flat out vec3 surface;
void main(){
 vec4 p=aModel*vec4(aPosition,1.0);
 coverage=vec2(aSurface.y,aColor.a);objectModel=aModel;atlasView=aSurface.w;
 if(aSurface.z>9.5){
   float az=mod(aSurface.w,16.0)*6.2831853/16.0,el=-1.5707963+floor(aSurface.w/16.0)*3.14159265/4.0;
   vec3 d=vec3(cos(el)*sin(az),sin(el),cos(el)*cos(az)),r=vec3(cos(az),0.0,-sin(az)),u=cross(d,r);
   p=aModel*vec4(r*aPosition.x+u*aPosition.y,1.0);
 }
 // Orthogonal rotation + nonuniform scale: inverse transpose without per-vertex inverse.
 vec3 scale2=vec3(dot(aModel[0].xyz,aModel[0].xyz),dot(aModel[1].xyz,aModel[1].xyz),dot(aModel[2].xyz,aModel[2].xyz));
 objectNormals=mat3(aModel[0].xyz/scale2.x,aModel[1].xyz/scale2.y,aModel[2].xyz/scale2.z);
 worldNormal=normalize(objectNormals*aNormal);
 if(aSurface.w>0.5 && aSurface.w<1.5 && aSurface.z<9.5){
   float x=p.x,z=p.z;
   p.y=2.0*sin(x*.04)*cos(z*.045)+.65*sin(x*.095+z*.075);
   float dx=.08*cos(x*.04)*cos(z*.045)+.06175*cos(x*.095+z*.075);
   float dz=-.09*sin(x*.04)*sin(z*.045)+.04875*cos(x*.095+z*.075);
   worldNormal=normalize(vec3(-dx,1.0,-dz));
 } else if(aSurface.w>1.5 && aSurface.z<9.5){
   float tip=aPosition.y+.5;
   p.x+=sin(uTime*1.8+p.x*.35+p.z*.2)*.16*tip*tip;
 }
 rasterLocal=aPosition;
 rasterPosition=p.xyz;
 color=aColor.rgb;surface=aSurface.xyz;gl_Position=uVP*p;
}