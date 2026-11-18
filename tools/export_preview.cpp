#define SDL_MAIN_HANDLED
#include "renderer.hpp"
#include "shore.hpp"
#include "vegetation_meshes.hpp"
#include <SDL.h>
#include <fstream>
#include <filesystem>
#include <iostream>
namespace pl {
struct PreviewExporter {
 static void write(Renderer&r,const std::string& dir){
  auto getSize=(PFNGLGETBUFFERPARAMETERIVPROC)SDL_GL_GetProcAddress("glGetBufferParameteriv");
  auto getData=(PFNGLGETBUFFERSUBDATAPROC)SDL_GL_GetProcAddress("glGetBufferSubData");
  std::filesystem::create_directories(dir);std::ofstream meta(dir+"/scene.json");meta<<"{\"meshes\":[";
  for(int i=0;i<meshCount;i++){
   auto&m=r.meshes[i];r.gl.BindVertexArray(m.vao);
   auto dump=[&](GLenum target,GLuint buffer,const std::string& name){r.gl.BindBuffer(target,buffer);GLint bytes=0;getSize(target,GL_BUFFER_SIZE,&bytes);std::vector<char> data(bytes);getData(target,0,bytes,data.data());std::ofstream f(dir+"/"+name,std::ios::binary);f.write(data.data(),bytes);};
   dump(GL_ARRAY_BUFFER,m.vbo,"mesh-"+std::to_string(i)+".vbo");dump(GL_ELEMENT_ARRAY_BUFFER,m.ebo,"mesh-"+std::to_string(i)+".ibo");
   if(i)meta<<",";meta<<"{\"id\":"<<i<<",\"indices\":"<<m.indices;
   auto far=vegetationFarMesh(static_cast<MeshType>(i));
   if(int(far)!=i)meta<<",\"far\":"<<int(far);
   meta<<"}";
  }
  auto scene=[&](const Scene&s){meta<<"[";bool first=true;for(auto&o:s.objects){
   if(!first)meta<<",";first=false;int mesh=int(o.mesh);Instance instance=o.gpu;
   if(o.mesh==MeshType::Rock)mesh=int(MeshType::RockDetail);
   if(o.mesh==MeshType::Branch&&instance.surface.w==0)instance.surface.w=-1;
   meta<<"{\"mesh\":"<<mesh;
   if(o.vegetation!=VegetationSection::None)
    meta<<",\"section\":"<<int(o.vegetation)<<",\"parentDistance\":"<<o.parentDistance;
   meta<<",\"instance\":[";
   const float* data=reinterpret_cast<const float*>(&instance);for(int j=0;j<24;j++){if(j)meta<<",";meta<<data[j];}meta<<"]}";
  }meta<<"]";};
  meta<<"],\"shore\":";scene(makeShoreScene());
  Scene b(0,false);b.add(MeshType::ShoreGround,{0,0,0},{1,1,1},{.2f,.17f,.1f},.9f,0,4);
  b.add(MeshType::Cube,{0,.55f,0},{10,1.2f,8},{.72f,.67f,.56f},.8f,0,2);
  for(float x:{-4.f,4.f})for(float z:{-3.f,3.f})b.add(MeshType::Cube,{x,2.8f,z},{.5f,3.3f,.5f},{.3f,.16f,.07f},.8f,0,5);
  b.add(MeshType::Cube,{0,4.5f,0},{10.5f,.4f,8.5f},{.36f,.34f,.3f},.8f,0,9);
  b.add(MeshType::Cube,{0,2.5f,-3.5f},{8,2.7f,.3f},{.6f,.58f,.53f},.85f,0,1);
  meta<<",\"building\":";scene(b);meta<<"}";
 }
};}
int main(int argc,char**argv){if(argc!=3)return 2;SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_Init(SDL_INIT_VIDEO);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 auto*w=SDL_CreateWindow("export",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);auto c=SDL_GL_CreateContext(w);if(!c)return 3;
 {pl::Renderer r(64,64,512,false,argv[1]);pl::PreviewExporter::write(r,argv[2]);}
 SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();
}
