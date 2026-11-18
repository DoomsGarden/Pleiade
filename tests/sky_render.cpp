#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
int main(int argc,char**argv){
 SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_Init(SDL_INIT_VIDEO);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 auto*w=SDL_CreateWindow("GLES validation",0,0,960,540,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!w){std::cerr<<SDL_GetError();return 4;}auto c=SDL_GL_CreateContext(w);if(!c){std::cerr<<SDL_GetError();return 1;}
 #define GLFN(name,type) auto name=(type)SDL_GL_GetProcAddress(#name)
 GLFN(glCreateShader,PFNGLCREATESHADERPROC);GLFN(glShaderSource,PFNGLSHADERSOURCEPROC);GLFN(glCompileShader,PFNGLCOMPILESHADERPROC);GLFN(glGetShaderiv,PFNGLGETSHADERIVPROC);GLFN(glGetShaderInfoLog,PFNGLGETSHADERINFOLOGPROC);GLFN(glCreateProgram,PFNGLCREATEPROGRAMPROC);GLFN(glAttachShader,PFNGLATTACHSHADERPROC);GLFN(glLinkProgram,PFNGLLINKPROGRAMPROC);GLFN(glGetProgramiv,PFNGLGETPROGRAMIVPROC);GLFN(glGetProgramInfoLog,PFNGLGETPROGRAMINFOLOGPROC);
 auto shader=[&](const char*name,GLenum type){std::ifstream f(std::string(argv[1])+"/"+name+".glsl");std::stringstream ss;ss<<f.rdbuf();auto text=ss.str();const char*s=text.c_str();auto id=glCreateShader(type);glShaderSource(id,1,&s,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::cerr<<name<<": "<<log;exit(2);}return id;};

 GLFN(glUseProgram,PFNGLUSEPROGRAMPROC);GLFN(glGetUniformLocation,PFNGLGETUNIFORMLOCATIONPROC);GLFN(glUniform1f,PFNGLUNIFORM1FPROC);GLFN(glUniform1i,PFNGLUNIFORM1IPROC);GLFN(glUniform3fv,PFNGLUNIFORM3FVPROC);GLFN(glGenVertexArrays,PFNGLGENVERTEXARRAYSPROC);GLFN(glBindVertexArray,PFNGLBINDVERTEXARRAYPROC);GLFN(glGenerateMipmap,PFNGLGENERATEMIPMAPPROC);
 auto p=glCreateProgram();glAttachShader(p,shader("fullscreen",GL_VERTEX_SHADER));glAttachShader(p,shader("sky_test",GL_FRAGMENT_SHADER));glLinkProgram(p);glUseProgram(p);GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
 GLuint tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);std::ifstream in(std::string(argv[1])+"/results/sky-04/stars.rgba",std::ios::binary);std::string data((std::istreambuf_iterator<char>(in)),{});glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2048,1024,0,GL_RGBA,GL_UNSIGNED_BYTE,data.data());glGenerateMipmap(GL_TEXTURE_2D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glUniform1i(glGetUniformLocation(p,"uStars"),0);glViewport(0,0,960,540);
 auto scalar=[&](const char*n,float x){glUniform1f(glGetUniformLocation(p,n),x);};auto vec=[&](const char*n,float x,float y,float z){float v[]={x,y,z};glUniform3fv(glGetUniformLocation(p,n),1,v);};
 auto capture=[&](const char*n){glDrawArrays(GL_TRIANGLES,0,3);unsigned char pixels[960*540*3];glReadPixels(0,0,960,540,GL_RGB,GL_UNSIGNED_BYTE,pixels);std::ofstream out(std::string(argv[1])+"/results/sky-04/"+n+".ppm",std::ios::binary);out<<"P6\n960 540\n255\n";for(int y=539;y>=0;y--)out.write((char*)pixels+y*960*3,960*3);};
vec("uSunDirection",1.2246467991473532e-16,0.8525245220595057,0.5226872289306592);
vec("uMoonDirection",0.6587909608748374,-0.5754106936835126,-0.4846617412838229);
scalar("uDaylight",1);
scalar("uStarAngle",3.141592653589793);
scalar("uLatitude",0.55);
capture("hour-12");
vec("uSunDirection",-1,1.566062140702201e-16,9.601608627976974e-17);
vec("uMoonDirection",0.6930368863332333,0.6628125797249261,0.28351253647753444);
scalar("uDaylight",0.37500000000000044);
scalar("uStarAngle",4.71238898038469);
scalar("uLatitude",0.55);
capture("hour-18");
vec("uSunDirection",0,-0.8525245220595057,-0.5226872289306592);
vec("uMoonDirection",-0.5391071096363214,0.7757518453452437,0.3279826196355843);
scalar("uDaylight",0);
scalar("uStarAngle",0);
scalar("uLatitude",0.55);
capture("hour-0");
vec("uSunDirection",0.7071067811865475,-0.6028258706760969,-0.3695956840164745);
vec("uMoonDirection",-0.9775775234410808,0.210024718616516,-0.015225085643635838);
scalar("uDaylight",0);
scalar("uStarAngle",0.7853981633974483);
scalar("uLatitude",0.55);
capture("hour-3");
SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();}