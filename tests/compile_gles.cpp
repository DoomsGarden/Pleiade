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
 auto*w=SDL_CreateWindow("GLES validation",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!w){std::cerr<<SDL_GetError();return 4;}auto c=SDL_GL_CreateContext(w);if(!c){std::cerr<<SDL_GetError();return 1;}
 #define GLFN(name,type) auto name=(type)SDL_GL_GetProcAddress(#name)
 GLFN(glCreateShader,PFNGLCREATESHADERPROC);GLFN(glShaderSource,PFNGLSHADERSOURCEPROC);GLFN(glCompileShader,PFNGLCOMPILESHADERPROC);GLFN(glGetShaderiv,PFNGLGETSHADERIVPROC);GLFN(glGetShaderInfoLog,PFNGLGETSHADERINFOLOGPROC);GLFN(glCreateProgram,PFNGLCREATEPROGRAMPROC);GLFN(glAttachShader,PFNGLATTACHSHADERPROC);GLFN(glLinkProgram,PFNGLLINKPROGRAMPROC);GLFN(glGetProgramiv,PFNGLGETPROGRAMIVPROC);GLFN(glGetProgramInfoLog,PFNGLGETPROGRAMINFOLOGPROC);
 auto shader=[&](const char*name,GLenum type){std::ifstream f(std::string(argv[1])+"/"+name+".glsl");std::stringstream ss;ss<<f.rdbuf();auto text=ss.str();const char*s=text.c_str();auto id=glCreateShader(type);glShaderSource(id,1,&s,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::cerr<<name<<": "<<log;exit(2);}return id;};
 for(auto pair:{std::pair{"geometry","material"},std::pair{"geometry","shadow"},std::pair{"fullscreen","composite"},std::pair{"fullscreen","cloud"}}){auto p=glCreateProgram();glAttachShader(p,shader(pair.first,GL_VERTEX_SHADER));glAttachShader(p,shader(pair.second,GL_FRAGMENT_SHADER));glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[8192];glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::cerr<<log;return 3;}std::cout<<pair.first<<" + "<<pair.second<<" GLSL ES 3.00 PASS\n";}
 SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();
}
