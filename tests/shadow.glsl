#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;
in vec2 rasterUV;
flat in vec3 surface;
uniform sampler2DArray uSurfaceColor;
uniform int uSurfaceMaps;
void main(){
 if(abs(surface.z-18.0)<.1)discard;
 float layer=surface.z-20.0;
 bool cutout=(layer>=1.5&&layer<3.5)||(layer>=13.5&&layer<15.5)||(layer>=17.5&&layer<19.5);
 if(uSurfaceMaps!=0&&cutout&&texture(uSurfaceColor,vec3(rasterUV,layer)).a<(layer>=13.5?.28:.42))discard;
}
