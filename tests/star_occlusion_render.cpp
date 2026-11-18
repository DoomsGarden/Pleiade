#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <fstream>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
int main(int argc,char**argv){
 SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_Init(SDL_INIT_VIDEO);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 auto*w=SDL_CreateWindow("GLES validation",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!w){std::cerr<<SDL_GetError();return 4;}auto c=SDL_GL_CreateContext(w);if(!c){std::cerr<<SDL_GetError();return 1;}
 #define GLFN(name,type) auto name=(type)SDL_GL_GetProcAddress(#name)
 GLFN(glCreateShader,PFNGLCREATESHADERPROC);GLFN(glShaderSource,PFNGLSHADERSOURCEPROC);GLFN(glCompileShader,PFNGLCOMPILESHADERPROC);GLFN(glGetShaderiv,PFNGLGETSHADERIVPROC);GLFN(glGetShaderInfoLog,PFNGLGETSHADERINFOLOGPROC);GLFN(glCreateProgram,PFNGLCREATEPROGRAMPROC);GLFN(glAttachShader,PFNGLATTACHSHADERPROC);GLFN(glLinkProgram,PFNGLLINKPROGRAMPROC);GLFN(glGetProgramiv,PFNGLGETPROGRAMIVPROC);GLFN(glGetProgramInfoLog,PFNGLGETPROGRAMINFOLOGPROC);
 auto shader=[&](const char*name,GLenum type){std::ifstream f(std::string(argv[1])+"/"+name+".glsl");std::stringstream ss;ss<<f.rdbuf();auto text=ss.str();const char*s=text.c_str();auto id=glCreateShader(type);glShaderSource(id,1,&s,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::cerr<<name<<": "<<log;exit(2);}return id;};

 GLFN(glUseProgram,PFNGLUSEPROGRAMPROC);GLFN(glGetUniformLocation,PFNGLGETUNIFORMLOCATIONPROC);GLFN(glUniform1f,PFNGLUNIFORM1FPROC);GLFN(glUniform1i,PFNGLUNIFORM1IPROC);GLFN(glUniform2f,PFNGLUNIFORM2FPROC);GLFN(glUniform3f,PFNGLUNIFORM3FPROC);GLFN(glGenVertexArrays,PFNGLGENVERTEXARRAYSPROC);GLFN(glBindVertexArray,PFNGLBINDVERTEXARRAYPROC);GLFN(glActiveTexture,PFNGLACTIVETEXTUREPROC);
 auto p=glCreateProgram();glAttachShader(p,shader("fullscreen",GL_VERTEX_SHADER));glAttachShader(p,shader("composite",GL_FRAGMENT_SHADER));glLinkProgram(p);glUseProgram(p);GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
 auto scalar=[&](const char*n,float x){glUniform1f(glGetUniformLocation(p,n),x);};auto vec=[&](const char*n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);};
 float rgba[64*64*4]={};float depth[64*64];for(int y=0;y<64;y++)for(int x=0;x<64;x++)depth[y*64+x]=1.f;
 GLuint depthTexture=0,starTexture=0;float stars[64*64*4];for(int y=0;y<64;y++)for(int x=0;x<64;x++){int k=(y*64+x)*4;stars[k]=(x+.5f)/64*6.2831853f;stars[k+1]=((y+.5f)/64-.5f)*3.14159265f;stars[k+2]=1;stars[k+3]=.5f;}
 for(int i=0;i<5;i++){GLuint t;glActiveTexture(GL_TEXTURE0+i);glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);if(i==2)depthTexture=t;if(i==4)starTexture=t;glTexImage2D(GL_TEXTURE_2D,0,i==2?GL_R32F:GL_RGBA32F,64,64,0,i==2?GL_RED:GL_RGBA,GL_FLOAT,i==2?depth:(i==4?stars:rgba));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
 const char*names[]={"uDirect","uAmbient","uDepth","uNormal","uStars"};for(int i=0;i<5;i++)glUniform1i(glGetUniformLocation(p,names[i]),i);
 glUniform1i(glGetUniformLocation(p,"uAA"),1);scalar("uExposure",1);scalar("uDaylight",0);vec("uCameraForward",0,0,1);vec("uCameraRight",1,0,0);vec("uCameraUp",0,1,0);vec("uSunDirection",0,0,1);vec("uMoonDirection",0,.3,.9539392);glUniform2f(glGetUniformLocation(p,"uProjectionScale"),1,1);

 scalar("uSkyPixelAngle",2.f/64);vec("uCameraForward",1,.4,1);
 glViewport(0,0,64,64);unsigned char clear[64*64*4],masked[64*64*4];glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,clear);
 unsigned char withoutStars[64*64*4];glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,starTexture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_RGBA,GL_FLOAT,rgba);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,withoutStars);int starPixels=0;for(int i=0;i<4096;i++)if(std::abs(int(clear[i*4])-int(withoutStars[i*4]))>2)starPixels++;glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_RGBA,GL_FLOAT,stars);std::cout<<"Star-contributing pixels: "<<starPixels<<"\n";
 int worst=0,nonzero=0;for(int offset=0;offset<8;offset++){
 for(int y=0;y<64;y++)for(int x=0;x<64;x++)depth[y*64+x]=((x+y+offset)%3==0)?.5f:1.f;
 glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,depthTexture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_RED,GL_FLOAT,depth);
 glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,masked);
 for(int i=0;i<4096;i++)for(int k=0;k<3;k++){if(clear[i*4+k]>30)nonzero++;int error=depth[i]<1?masked[i*4+k]:std::abs(int(masked[i*4+k])-int(clear[i*4+k]));worst=std::max(worst,error);}}
 std::cout<<"Eight cutout patterns: max sky/occlusion error "<<worst<<"; bright samples "<<nonzero<<"\n";
 SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();return worst!=0||nonzero==0||starPixels==0?5:0;}
