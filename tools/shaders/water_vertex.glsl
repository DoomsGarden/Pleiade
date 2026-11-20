layout(location=0) in vec3 aPosition;
uniform mat4 uVP;
void main(){
 vec3 p=aPosition;
 p.y+=waveField(p.xz).x;
 gl_Position=uVP*vec4(p,1.0);
}
