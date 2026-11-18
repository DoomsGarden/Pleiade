#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <fstream>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
int main(int argc,char**argv){
 SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_Init(SDL_INIT_VIDEO);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 auto*w=SDL_CreateWindow("GLES validation",0,0,480,270,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!w){std::cerr<<SDL_GetError();return 4;}auto c=SDL_GL_CreateContext(w);if(!c){std::cerr<<SDL_GetError();return 1;}
 #define GLFN(name,type) auto name=(type)SDL_GL_GetProcAddress(#name)
 GLFN(glCreateShader,PFNGLCREATESHADERPROC);GLFN(glShaderSource,PFNGLSHADERSOURCEPROC);GLFN(glCompileShader,PFNGLCOMPILESHADERPROC);GLFN(glGetShaderiv,PFNGLGETSHADERIVPROC);GLFN(glGetShaderInfoLog,PFNGLGETSHADERINFOLOGPROC);GLFN(glCreateProgram,PFNGLCREATEPROGRAMPROC);GLFN(glAttachShader,PFNGLATTACHSHADERPROC);GLFN(glLinkProgram,PFNGLLINKPROGRAMPROC);GLFN(glGetProgramiv,PFNGLGETPROGRAMIVPROC);GLFN(glGetProgramInfoLog,PFNGLGETPROGRAMINFOLOGPROC);
 auto shader=[&](const char*name,GLenum type){std::ifstream f(std::string(argv[1])+"/"+name+".glsl");std::stringstream ss;ss<<f.rdbuf();auto text=ss.str();const char*s=text.c_str();auto id=glCreateShader(type);glShaderSource(id,1,&s,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::cerr<<name<<": "<<log;exit(2);}return id;};

 GLFN(glUseProgram,PFNGLUSEPROGRAMPROC);GLFN(glGetUniformLocation,PFNGLGETUNIFORMLOCATIONPROC);GLFN(glUniform1f,PFNGLUNIFORM1FPROC);GLFN(glUniform1i,PFNGLUNIFORM1IPROC);GLFN(glUniform3f,PFNGLUNIFORM3FPROC);GLFN(glGenVertexArrays,PFNGLGENVERTEXARRAYSPROC);GLFN(glBindVertexArray,PFNGLBINDVERTEXARRAYPROC);GLFN(glActiveTexture,PFNGLACTIVETEXTUREPROC);GLFN(glGenFramebuffers,PFNGLGENFRAMEBUFFERSPROC);GLFN(glBindFramebuffer,PFNGLBINDFRAMEBUFFERPROC);GLFN(glFramebufferTexture2D,PFNGLFRAMEBUFFERTEXTURE2DPROC);GLFN(glCheckFramebufferStatus,PFNGLCHECKFRAMEBUFFERSTATUSPROC);
 auto make=[&](const char*name){auto p=glCreateProgram();glAttachShader(p,shader("fullscreen",GL_VERTEX_SHADER));glAttachShader(p,shader(name,GL_FRAGMENT_SHADER));glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok)exit(3);return p;};auto volume=make("cloud"),view=make("cloud_view");GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
 GLuint tex,fbo;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,256,128,0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return 4;
 auto uniforms=[&](GLuint p,float altitude){glUseProgram(p);glUniform1f(glGetUniformLocation(p,"uWorldSeed"),12345);glUniform1f(glGetUniformLocation(p,"uCloudAmount"),.85);glUniform1f(glGetUniformLocation(p,"uCloudTime"),360);glUniform1f(glGetUniformLocation(p,"uSkyAmbient"),1);glUniform3f(glGetUniformLocation(p,"uSkyObserver"),0,altitude,0);glUniform3f(glGetUniformLocation(p,"uSkyLightDirection"),.2,.8,.3);glUniform3f(glGetUniformLocation(p,"uLightColor"),4.4,3.95,3.2);};
 for(float altitude:{2.f,1300.f,4200.f}){
 glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,256,128);uniforms(volume,altitude);glDrawArrays(GL_TRIANGLES,0,3);glFinish();auto buildStart=std::chrono::steady_clock::now();glDrawArrays(GL_TRIANGLES,0,3);glFinish();std::cout<<"altitude "<<altitude<<" atlas ms "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-buildStart).count()<<"\n";
 glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,480,270);uniforms(view,altitude);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex);glUniform1i(glGetUniformLocation(view,"uCloudMap"),0);
 for(int cache:{0,1}){glUniform1i(glGetUniformLocation(view,"uCloudCached"),cache);glDrawArrays(GL_TRIANGLES,0,3);glFinish();auto start=std::chrono::steady_clock::now();for(int repeat=0;repeat<3;repeat++)glDrawArrays(GL_TRIANGLES,0,3);glFinish();std::cout<<"cache "<<cache<<" avg ms "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/3<<"\n";
 unsigned char pixels[480*270*3];glReadPixels(0,0,480,270,GL_RGB,GL_UNSIGNED_BYTE,pixels);std::ofstream out(std::string(argv[1])+"/results/cloud-08/"+std::to_string(int(altitude))+"-"+std::to_string(cache)+".ppm",std::ios::binary);out<<"P6\n480 270\n255\n";for(int y=269;y>=0;y--)out.write((char*)pixels+y*480*3,480*3);
 }}SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();}
