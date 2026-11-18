#pragma once
#include "../common/graphics.h"
#include <orbis/Pigletv2VSH.h>

class GpuScene
{
public:
    struct Vertex { float x,y,u,v; };
    struct Sprite { float x,y,w,h,u0,v0,u1,v1,opacity; };
    bool Init(bool contextOnly=false, void (*report)(const char *)=nullptr);
    void Begin();
    bool SuspendForNativeGpu(bool releaseContext=true);
    bool ResumeAfterNativeGpu();
    bool Present();
    void DrawPixel(int x,int y,Color c) { DrawRectangle(x,y,1,1,c); }
    void DrawRectangle(int x,int y,int w,int h,Color c);
    void FrameBufferFill(Color c);
    GLuint createTexture(int w,int h,const unsigned char *rgba,bool repeat);
    void drawMesh(GLuint texture,const Vertex *vertices,unsigned count);
    bool updateTextureRegion(GLuint texture,int x,int y,int w,int h,const unsigned char *rgba);
    void drawSprites(GLuint texture,const Sprite *sprites,unsigned count,bool additive=true);
    void drawSolidMesh(const Vertex *vertices,unsigned count,Color color,float opacity);
    const char *error() const { return message; }
    void Shutdown();
private:
    static constexpr unsigned maxVertices=49152;
    Vertex *batch=nullptr;
    unsigned used=0;
    GLuint program=0,atlas=0,buffer=0,externalFramebuffer=0;
    bool nativeContextReleased=false;
    GLint attribute=-1,textureMatrix=-1,modelMatrix=-1,opacityUniform=-1;
    unsigned batchColor=0;
    EGLDisplay display=EGL_NO_DISPLAY;
    EGLSurface surface=EGL_NO_SURFACE;
    EGLContext context=EGL_NO_CONTEXT;
    char message[256]={};
    bool fail(const char *step,int code);
    void flush();
};
