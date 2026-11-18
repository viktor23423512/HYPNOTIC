#include "gpu_scene.h"
#include "texture_mapping.h"
#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>

struct ShaderBlob { char *name; unsigned char hash[16]; uint64_t length; unsigned char *code; };
// Shader table is resolved after loading its module, not as a data import.

bool GpuScene::fail(const char *step,int code)
{
    snprintf(message,sizeof(message),"GPU %s ERROR %08X",step,(unsigned)code);
    printf("%s\n",message);
    return false;
}
bool GpuScene::Init(bool contextOnly,void (*report)(const char *))
{
    if(report) report("GPU 2 - LOADING PIGLET");
    int shaderModule=-1;
    int pigletModule=-1;
    char path[256];
    const char *sandbox=sceKernelGetFsSandboxRandomWord();
    if(!sandbox) return fail("SANDBOX",-1);
    const char *modules[]={"libScePigletv2VSH.sprx","libScePrecompiledShaders.sprx"};
    for(int module=0;module<(contextOnly?1:2);++module)
    {
        const char *name=modules[module];
        snprintf(path,sizeof(path),"/%s/common/lib/%s",sandbox,name);
        int result=0;
        int handle=(int)sceKernelLoadStartModule(path,0,nullptr,0,nullptr,&result);
        if(handle<0) return fail(name,handle);
        char moduleInfo[128];
        snprintf(moduleInfo,sizeof(moduleInfo),"GPU MODULE %d HANDLE %08X START %08X",module,(unsigned)handle,(unsigned)result);
        if(report) report(moduleInfo);
        if(result<0) return fail("MODULE START",result);
        if(module==0) pigletModule=handle;
        if(module==1) shaderModule=handle;
    }
    if(report) report("GPU 3 - CONFIGURING PIGLET");
    OrbisPglConfig cfg={};
    cfg.size=sizeof(cfg);
    // Published compositor-only configuration from flatz/ps4_gl_test.
    // Avoid the larger flexible-memory configuration in this compatibility test.
    cfg.flags=ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT;
    cfg.processOrder=1;
    cfg.systemSharedMemorySize=0x2000000;
    cfg.videoSharedMemorySize=0xA000000;
    cfg.unk_0x5C=2;
    if(!scePigletSetConfigurationVSH(&cfg)) return fail("CONFIG",-1);
    if(report) report("GPU 4A - LOOKING UP EGL DISPLAY");
    eglGetError(); // Clear earlier EGL state before measuring this operation.
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint displayError=eglGetError();
    if(display==EGL_NO_DISPLAY)
    {
        // Check the loaded module's export directly if the import returned zero.
        typedef EGLDisplay (*GetDisplay)(EGLNativeDisplayType);
        GetDisplay resolved=nullptr;
        const int rc=sceKernelDlsym(pigletModule,"eglGetDisplay",(void **)&resolved);
        char lookupInfo[128];
        snprintf(lookupInfo,sizeof(lookupInfo),"GPU GETDISPLAY SYMBOL RC %08X ADDRESS %016llX",(unsigned)rc,
            (unsigned long long)(uintptr_t)resolved);
        if(report) report(lookupInfo);
        if(rc==0 && resolved)
        {
            display=resolved(EGL_DEFAULT_DISPLAY);
            displayError=eglGetError();
        }
    }
    char detail[192];
    snprintf(detail,sizeof(detail),"GPU DISPLAY %016llX EGL %08X",
        (unsigned long long)(uintptr_t)display,(unsigned)displayError);
    if(report) report(detail);
    if(display==EGL_NO_DISPLAY)
    {
        snprintf(message,sizeof(message),"GPU DISPLAY LOOKUP RETURNED ZERO - EGL %08X",(unsigned)displayError);
        printf("%s\n",message);
        return false;
    }
    if(report) report("GPU 4B - CALLING EGL INITIALIZE");
    EGLint major=-1,minor=-1;
    const EGLBoolean initialized=eglInitialize(display,&major,&minor);
    const EGLint initError=eglGetError();
    snprintf(detail,sizeof(detail),"GPU EGL INIT RETURN %u STATUS %08X VERSION %d.%d",
        (unsigned)initialized,(unsigned)initError,major,minor);
    if(report) report(detail);
    if(initialized!=EGL_TRUE)
    {
        snprintf(message,sizeof(message),"GPU INITIALIZE RETURN %u EGL %08X VERSION %d.%d",
            (unsigned)initialized,(unsigned)initError,major,minor);
        printf("%s\n",message);
        return false;
    }
    if(!eglBindAPI(EGL_OPENGL_ES_API)) return fail("EGL API",eglGetError());
    const EGLint attrs[]={EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,
        EGL_DEPTH_SIZE,0,EGL_STENCIL_SIZE,0,EGL_SAMPLE_BUFFERS,0,EGL_SAMPLES,0,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_NONE};
    EGLConfig config={}; EGLint count=0;
    if(!eglChooseConfig(display,attrs,&config,1,&count) || count<1) return fail("EGL CONFIG",eglGetError());
    OrbisPglWindow window={0,1920,1080,0};
    const EGLint winAttrs[]={EGL_RENDER_BUFFER,EGL_BACK_BUFFER,EGL_NONE};
    surface=eglCreateWindowSurface(display,config,&window,winAttrs);
    if(surface==EGL_NO_SURFACE) return fail("EGL SURFACE",eglGetError());
    const EGLint ctxAttrs[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,ctxAttrs);
    if(context==EGL_NO_CONTEXT || !eglMakeCurrent(display,surface,surface,context)) return fail("EGL CONTEXT",eglGetError());
    if(!eglSwapInterval(display,1)) return fail("VSYNC",eglGetError());
    if(report) report("GPU 5 - EGL CONTEXT AND VSYNC READY");
    if(contextOnly) return true;
    ShaderBlob *entries=nullptr;
    int symbolResult=sceKernelDlsym(shaderModule,"scePrecompiledShaderEntries",(void **)&entries);
    if(symbolResult<0 || !entries) return fail("SHADER TABLE SYMBOL",symbolResult);
    GLuint shaders[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
    const char *names[]={"texmap/v_2.vert","texmap/f_2.frag"};
    for(int i=0;i<2;++i)
    {
        ShaderBlob *found=nullptr;
        for(ShaderBlob *s=entries;s<entries+4096 && s->name;++s)
            if(!strcmp(s->name,names[i])) { found=s; break; }
        if(!found) return fail("SHADER NOT FOUND",i);
        glShaderBinary(1,&shaders[i],0,found->code,(GLsizei)found->length);
        GLint compiled=0; glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&compiled);
        if(!compiled) return fail("SHADER BINARY",glGetError());
    }
    program=glCreateProgram();
    glAttachShader(program,shaders[0]); glAttachShader(program,shaders[1]); glLinkProgram(program);
    GLint linked=0; glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked) { glGetProgramInfoLog(program,sizeof(message),nullptr,message); printf("GPU LINK %s\n",message); return false; }
    glDeleteShader(shaders[0]); glDeleteShader(shaders[1]); glUseProgram(program);
    attribute=glGetAttribLocation(program,"a_vertex");
    GLint projection=glGetUniformLocation(program,"u_projectionMatrix");
    GLint model=glGetUniformLocation(program,"u_modelViewMatrix");
    modelMatrix=model;
    GLint texture=glGetUniformLocation(program,"u_textureSpaceMatrix");
    textureMatrix=texture;
    GLint opacity=glGetUniformLocation(program,"u_opacity");
    opacityUniform=opacity;
    GLint sampler=glGetUniformLocation(program,"s_sampler");
    if(attribute<0 || projection<0 || model<0 || texture<0 || opacity<0 || sampler<0) return fail("SHADER LOCATIONS",-1);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float ortho[16]={2.0f/1920,0,0,0,0,-2.0f/1080,0,0,0,0,-1,0,-1,1,0,1};
    glUniformMatrix4fv(projection,1,GL_FALSE,ortho);
    glUniformMatrix4fv(model,1,GL_FALSE,identity); glUniformMatrix4fv(texture,1,GL_FALSE,identity);
    glUniform1f(opacity,1); glUniform1i(sampler,0);
    batch=new Vertex[maxVertices];
    unsigned char *pixels=new unsigned char[256*256*4];
    for(unsigned i=0;i<65536;++i)
    {
        pixels[i*4]=((i>>11)&31)*255/31;
        pixels[i*4+1]=((i>>5)&63)*255/63;
        pixels[i*4+2]=(i&31)*255/31; pixels[i*4+3]=255;
    }
    atlas=createTexture(256,256,pixels,false); delete[] pixels;
    if(!atlas) return false;
    glGenBuffers(1,&buffer);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
    sceSystemServiceHideSplashScreen();
    printf("GPU ready: %s / %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER));
    const GLenum setupError=glGetError();
    return setupError==GL_NO_ERROR || fail("SETUP",setupError);
}
GLuint GpuScene::createTexture(int w,int h,const unsigned char *data,bool repeat)
{
    GLuint id=0; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
    const GLenum error=glGetError();
    if(error!=GL_NO_ERROR || !id)
    {
        if(id) glDeleteTextures(1,&id);
        fail("TEXTURE UPLOAD",error);
        return 0;
    }
    return id;
}
void GpuScene::drawMesh(GLuint texture,const Vertex *vertices,unsigned count)
{
    if(!vertices || !count || count%3) return;
    flush(); glUseProgram(program); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float mapping[16]={};
    mapping[12]=vertices[0].u; mapping[13]=vertices[0].v; mapping[15]=1;
    glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glVertexAttribPointer(attribute,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glEnableVertexAttribArray(attribute);
    if(texture==atlas)
    {
        glBufferData(GL_ARRAY_BUFFER,count*sizeof(Vertex),vertices,GL_STREAM_DRAW);
        glUniformMatrix4fv(modelMatrix,1,GL_FALSE,identity);
        glUniformMatrix4fv(textureMatrix,1,GL_FALSE,mapping);
        glDrawArrays(GL_TRIANGLES,0,count);
    }
    else
    {
        const Vertex local[3]={{0,0,0,0},{1,0,1,0},{0,1,0,1}};
        glBufferData(GL_ARRAY_BUFFER,sizeof(local),local,GL_STREAM_DRAW);
        for(unsigned first=0;first<count;first+=3)
        {
            float model[16];
            if(!triangleLocalMatrices(vertices+first,model,mapping)) continue;
            glUniformMatrix4fv(modelMatrix,1,GL_FALSE,model);
            glUniformMatrix4fv(textureMatrix,1,GL_FALSE,mapping);
            glDrawArrays(GL_TRIANGLES,0,3);
        }
        glUniformMatrix4fv(modelMatrix,1,GL_FALSE,identity);
    }
}
bool GpuScene::updateTextureRegion(GLuint texture,int x,int y,int w,int h,const unsigned char *rgba)
{
    flush(); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexSubImage2D(GL_TEXTURE_2D,0,x,y,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    const GLenum error=glGetError();
    return error==GL_NO_ERROR || fail("ATLAS UPDATE",error);
}
void GpuScene::drawSprites(GLuint texture,const Sprite *sprites,unsigned count,bool additive)
{
    if(!texture || !count) return;
    flush(); glUseProgram(program); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture);
    const Vertex quad[6]={{0,0,0,0},{1,0,1,0},{0,1,0,1},{1,0,1,0},{1,1,1,1},{0,1,0,1}};
    glBindBuffer(GL_ARRAY_BUFFER,buffer); glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STREAM_DRAW);
    glVertexAttribPointer(attribute,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glEnableVertexAttribArray(attribute);
    // Non-additive sprites contain premultiplied RGB, matching the shader's
    // opacity multiplication. Existing callers retain additive rendering.
    glEnable(GL_BLEND); glBlendFunc(GL_ONE,additive?GL_ONE:GL_ONE_MINUS_SRC_ALPHA);
    float model[16]={},uv[16]={}; model[10]=model[15]=uv[10]=uv[15]=1;
    for(unsigned i=0;i<count;++i)
    {
        const Sprite &s=sprites[i];
        if(s.opacity<=0.002f || s.w<=0 || s.h<=0 || s.x>=1920 || s.y>=1080 || s.x+s.w<=0 || s.y+s.h<=0) continue;
        model[0]=s.w; model[5]=s.h; model[12]=s.x; model[13]=s.y;
        uv[0]=s.u1-s.u0; uv[5]=s.v1-s.v0; uv[12]=s.u0; uv[13]=s.v0;
        glUniformMatrix4fv(modelMatrix,1,GL_FALSE,model);
        glUniformMatrix4fv(textureMatrix,1,GL_FALSE,uv);
        glUniform1f(opacityUniform,s.opacity); glDrawArrays(GL_TRIANGLES,0,6);
    }
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glUniformMatrix4fv(modelMatrix,1,GL_FALSE,identity); glUniform1f(opacityUniform,1);
    glDisable(GL_BLEND);
}
void GpuScene::drawSolidMesh(const Vertex *vertices,unsigned count,Color color,float opacity)
{
    if(!vertices || !count || count%3 || opacity<=0)return;
    flush();glUseProgram(program);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlas);
    const unsigned code=((color.r>>3)<<11)|((color.g>>2)<<5)|(color.b>>3);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float uv[16]={};uv[12]=((code&255)+0.5f)/256;uv[13]=((code>>8)+0.5f)/256;uv[15]=1;
    glUniformMatrix4fv(modelMatrix,1,GL_FALSE,identity);glUniformMatrix4fv(textureMatrix,1,GL_FALSE,uv);
    glUniform1f(opacityUniform,opacity);
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,count*sizeof(Vertex),vertices,GL_STREAM_DRAW);
    glVertexAttribPointer(attribute,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);glEnableVertexAttribArray(attribute);
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glDrawArrays(GL_TRIANGLES,0,count);glDisable(GL_BLEND);
    glUniform1f(opacityUniform,1);
}
void GpuScene::flush()
{
    if(!used) return;
    const unsigned count=used; used=0;
    drawMesh(atlas,batch,count);
}
void GpuScene::DrawRectangle(int x,int y,int w,int h,Color c)
{
    if(w<=0 || h<=0 || x>=1920 || y>=1080 || x+w<=0 || y+h<=0) return;
    const unsigned code=((c.r>>3)<<11)|((c.g>>2)<<5)|(c.b>>3);
    if(used && (code!=batchColor || used+6>maxVertices)) flush();
    batchColor=code;
    const float u=((code&255)+0.5f)/256,v=((code>>8)+0.5f)/256;
    const float a=x<0?0:(float)x,b=y<0?0:(float)y;
    const float right=x+w>1920?1920:(float)(x+w),bottom=y+h>1080?1080:(float)(y+h);
    Vertex quad[6]={{a,b,u,v},{right,b,u,v},{a,bottom,u,v},{right,b,u,v},{right,bottom,u,v},{a,bottom,u,v}};
    memcpy(batch+used,quad,sizeof(quad)); used+=6;
}
void GpuScene::FrameBufferFill(Color c)
{
    flush(); glClearColor(c.r/255.0f,c.g/255.0f,c.b/255.0f,1); glClear(GL_COLOR_BUFFER_BIT);
}
void GpuScene::Begin() { used=0; glViewport(0,0,1920,1080); FrameBufferFill(Color{0,0,0}); }
bool GpuScene::Present()
{
    flush(); GLenum error=glGetError();
    if(error) return fail("DRAW",error);
    return eglSwapBuffers(display,surface)==EGL_TRUE || fail("SWAP",eglGetError());
}
void GpuScene::Shutdown()
{
    if(display!=EGL_NO_DISPLAY)
    {
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if(surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
        if(context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
        eglTerminate(display);
    }
    delete[] batch; batch=nullptr;
}

// Keep Piglet as the sole display owner. Native rendering is offscreen and
// fully fenced before CPU-visible pixels are uploaded back to this context.
bool GpuScene::SuspendForNativeGpu(bool releaseContext){
 flush();glFinish();nativeContextReleased=false;
 if(!releaseContext)return glGetError()==GL_NO_ERROR || fail("MILKDROP GL FINISH",-1);
 if(eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT)!=EGL_TRUE)return fail("MILKDROP CONTEXT RELEASE",eglGetError());
 nativeContextReleased=true;return true;
}
bool GpuScene::ResumeAfterNativeGpu(){
 if(nativeContextReleased && eglMakeCurrent(display,surface,surface,context)!=EGL_TRUE)return fail("MILKDROP CONTEXT RESTORE",eglGetError());
 nativeContextReleased=false;
 // Re-emit the GL render state touched by the external native command stream.
 if(!externalFramebuffer)glGenFramebuffers(1,&externalFramebuffer);glBindFramebuffer(GL_FRAMEBUFFER,externalFramebuffer);glBindFramebuffer(GL_FRAMEBUFFER,0);
 glUseProgram(0);glUseProgram(program);glEnable(GL_DEPTH_TEST);glDisable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_CULL_FACE);glEnable(GL_SCISSOR_TEST);glDisable(GL_SCISSOR_TEST);glEnable(GL_BLEND);glDisable(GL_BLEND);glViewport(0,0,1,1);glViewport(0,0,1920,1080);
 return glGetError()==GL_NO_ERROR || fail("MILKDROP GL RESTORE",-1);
}
