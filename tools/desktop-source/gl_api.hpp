#pragma once
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <stdexcept>
#include <string>

#define PL_GL_FUNCTIONS(X)                                                                              \
    X(const GLubyte *, GetString, (GLenum))                                                             \
    X(GLenum, GetError, ())                                                                             \
    X(void, Viewport, (GLint, GLint, GLsizei, GLsizei))                                                 \
    X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))                                           \
    X(void, Clear, (GLbitfield))                                                                        \
    X(void, Enable, (GLenum))                                                                           \
    X(void, Disable, (GLenum))                                                                          \
    X(void, DepthFunc, (GLenum))                                                                        \
    X(void, BlendFunc, (GLenum, GLenum))                                                                \
    X(void, PolygonOffset, (GLfloat, GLfloat))                                                          \
    X(void, GenVertexArrays, (GLsizei, GLuint *))                                                       \
    X(void, DeleteVertexArrays, (GLsizei, const GLuint *))                                              \
    X(void, BindVertexArray, (GLuint))                                                                  \
    X(void, GenBuffers, (GLsizei, GLuint *))                                                            \
    X(void, DeleteBuffers, (GLsizei, const GLuint *))                                                   \
    X(void, BindBuffer, (GLenum, GLuint))                                                               \
    X(void, BufferData, (GLenum, GLsizeiptr, const void *, GLenum))                                     \
    X(void, EnableVertexAttribArray, (GLuint))                                                          \
    X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *))             \
    X(void, VertexAttribDivisor, (GLuint, GLuint))                                                      \
    X(GLuint, CreateShader, (GLenum))                                                                   \
    X(void, ShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *))                      \
    X(void, CompileShader, (GLuint))                                                                    \
    X(void, GetShaderiv, (GLuint, GLenum, GLint *))                                                     \
    X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))                                   \
    X(void, DeleteShader, (GLuint))                                                                     \
    X(GLuint, CreateProgram, ())                                                                        \
    X(void, AttachShader, (GLuint, GLuint))                                                             \
    X(void, LinkProgram, (GLuint))                                                                      \
    X(void, GetProgramiv, (GLuint, GLenum, GLint *))                                                    \
    X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))                                  \
    X(void, DeleteProgram, (GLuint))                                                                    \
    X(void, UseProgram, (GLuint))                                                                       \
    X(GLint, GetUniformLocation, (GLuint, const GLchar *))                                              \
    X(void, UniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat *))                             \
    X(void, Uniform1i, (GLint, GLint))                                                                  \
    X(void, Uniform1f, (GLint, GLfloat))                                                                \
    X(void, Uniform2f, (GLint, GLfloat, GLfloat))                                                       \
    X(void, Uniform3f, (GLint, GLfloat, GLfloat, GLfloat))                                              \
    X(void, DrawElementsInstanced, (GLenum, GLsizei, GLenum, const void *, GLsizei))                    \
    X(void, DrawArrays, (GLenum, GLint, GLsizei))                                                       \
    X(void, GenFramebuffers, (GLsizei, GLuint *))                                                       \
    X(void, DeleteFramebuffers, (GLsizei, const GLuint *))                                              \
    X(void, BindFramebuffer, (GLenum, GLuint))                                                          \
    X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))                              \
    X(GLenum, CheckFramebufferStatus, (GLenum))                                                         \
    X(void, DrawBuffers, (GLsizei, const GLenum *))                                                     \
    X(void, DrawBuffer, (GLenum))                                                                       \
    X(void, ReadBuffer, (GLenum))                                                                       \
    X(void, GenTextures, (GLsizei, GLuint *))                                                           \
    X(void, DeleteTextures, (GLsizei, const GLuint *))                                                  \
    X(void, BindTexture, (GLenum, GLuint))                                                              \
    X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *))  \
    X(void, TexImage3D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    X(void, TexSubImage2D,                                                                              \
      (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *))                    \
    X(void, TexParameteri, (GLenum, GLenum, GLint))                                                     \
    X(void, TexParameterfv, (GLenum, GLenum, const GLfloat *))                                          \
    X(void, GenerateMipmap, (GLenum))                                                                   \
    X(void, ActiveTexture, (GLenum))                                                                    \
    X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *))                       \
    X(void, PixelStorei, (GLenum, GLint))                                                               \
    X(void, GenQueries, (GLsizei, GLuint *))                                                            \
    X(void, DeleteQueries, (GLsizei, const GLuint *))                                                   \
    X(void, BeginQuery, (GLenum, GLuint))                                                               \
    X(void, EndQuery, (GLenum))                                                                         \
    X(void, GetQueryObjectiv, (GLuint, GLenum, GLint *))                                                \
    X(void, GetQueryObjectui64v, (GLuint, GLenum, GLuint64 *))                                          \
    X(void, Finish, ())

namespace pl {
struct GL {
#define DECL(ret, name, args) ret(APIENTRY *name) args = nullptr;
    PL_GL_FUNCTIONS(DECL)
#undef DECL
    void load() {
#define LOAD(ret, name, args)                                                                           \
    name = reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress("gl" #name));                         \
    if (!name)                                                                                          \
        throw std::runtime_error("Missing OpenGL function: gl" #name);
        PL_GL_FUNCTIONS(LOAD)
#undef LOAD
    }
    void check(const char *stage) {
        GLenum e = GetError();
        if (e != GL_NO_ERROR)
            throw std::runtime_error(std::string(stage) + ": OpenGL error " + std::to_string(e));
    }
};
} // namespace pl
