#pragma once
#include <math.h>

// Same set-0 descriptor ABI as the hardware-confirmed 0.08 test.
static const char feedback_frag[]=R"glsl(#version 450
layout(set=0,binding=0) uniform sampler2D history;
layout(set=0,binding=1,std140) uniform Parameters { vec4 control; vec4 transform; vec4 palette; } params;
layout(location=0) out vec4 outColor;
void main() {
    vec2 uv=gl_FragCoord.xy/vec2(512.0,288.0);
    if(params.control.x<0.5) {
        outColor=vec4(textureLod(history,uv,0.0).rgb*params.control.w,1.0);
        return;
    }
    float dt=params.control.y,t=params.control.z;
    if(params.control.x>2.5) {
        float a=params.transform.y,c=cos(a),s=sin(a);
        vec2 p=(uv-0.5)*vec2(1.0,-0.5625)*params.transform.x;
        vec2 q=vec2(c*p.x-s*p.y,s*p.x+c*p.y)-params.transform.zw;
        q=vec2(q.x,-q.y/0.5625)+0.5;
        outColor=vec4(textureLod(history,fract(q),0.0).rgb*params.control.w,1.0);return;
    }
    vec2 p=uv-0.5;
    float angle=params.transform.y;
    float c=cos(angle),s=sin(angle);
    vec2 q=mat2(c,-s,s,c)*p*params.transform.x+0.5;
    vec3 color=textureLod(history,clamp(q,vec2(0.0),vec2(1.0)),0.0).rgb;
    float inside=step(0.0,q.x)*step(q.x,1.0)*step(0.0,q.y)*step(q.y,1.0);
    color=max(vec3(0.0),color*inside*params.control.w-vec3(dt*0.12));
    if(params.control.x>1.5) { outColor=vec4(color,1.0);return; }
    vec2 a=vec2(0.5+0.29*cos(t*0.89),0.5+0.29*sin(t*1.13));
    vec2 b=vec2(0.5+0.29*cos(t*1.21+2.1),0.5+0.34*sin(t*0.77+1.0));
    vec2 d=vec2(0.5+0.33*cos(t*0.63+4.2),0.5+0.29*sin(t*1.37+2.0));
    vec2 da=(uv-a)*vec2(1.7777778,1.0);
    vec2 db=(uv-b)*vec2(1.7777778,1.0);
    vec2 dd=(uv-d)*vec2(1.7777778,1.0);
    vec3 palette=clamp(params.palette.rgb,vec3(0.0),vec3(1.0));
    color+=dt*22.0*(exp(-dot(da,da)*2800.0)*palette
                 +exp(-dot(db,db)*2800.0)*palette.gbr
                 +exp(-dot(dd,dd)*2800.0)*palette.brg);
    outColor=vec4(color,1.0);
}
)glsl";

struct FeedbackSurface {
    GnmRenderTarget target;
    GnmTexture texture;
    unsigned char *data;
    size_t bytes;
    unsigned pitch;
    unsigned char *table;
};
static FeedbackSurface feedback[2]={};
static float *feedbackParams=nullptr;
static void *feedbackCommands=nullptr;
static volatile uint64_t *feedbackLabel=nullptr;
static const size_t feedbackCommandBytes=0x10000;

static GnmPsShader *warpPS=nullptr;
static const char warp_frag[]=R"glsl(#version 450
layout(set=0,binding=0) uniform sampler2D history;
layout(location=0) in vec4 data;
layout(location=0) out vec4 color;
void main(){ color=vec4(textureLod(history,data.xy,0.0).rgb*data.z,1.0); }
)glsl";
#include "texture_gpu.hpp"
#include "common_gpu.hpp"
#include "textured_shapes_gpu.hpp"
#include "legacy_final_gpu.hpp"
#include "preset_layers.hpp"
#include "performance_probe.hpp"
#include "curated_tour.hpp"
static void createWarpShader(PsbcTarget target){
    const PsbcResourceType binding[]={PSBC_RESOURCE_IMAGE_SAMPLER};
    PsbcCompileOptions options={target,PSBC_STAGE_FRAGMENT,"main",true};
    options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);options.resource_count=1;options.resources=binding;
    auto result=compileText(target,PSBC_STAGE_FRAGMENT,warp_frag,&options);
    warpPS=(GnmPsShader *)prepareShader(result,false);warpPS->registers.spibaryccntl=0;psbc_free_output(&result);
}
static void spatialStep(unsigned source,unsigned destination,const PresetMesh &mesh){
    if(source>=2 || destination>=2 || source==destination)fail("SPATIAL HISTORY ALIAS",-1);
    drawPresetGeometry(&feedback[destination].target,mesh,false,GNM_PT_TRILIST,false,warpPS,feedback[source].table);
    pixels=feedback[destination].data;pixelPitch=feedback[destination].pitch;
}
static void compositeStep(unsigned source,unsigned destination,unsigned stage){
    if(source>=2 || destination>=2 || source==destination || stage<1 || stage>2)fail("COMPOSITE ALIAS OR STAGE",-1);
    drawPresetGeometry(&feedback[destination].target,compositeQuad,false,GNM_PT_TRILIST,false,compositePrograms[stage-1],feedback[source].table);
    pixels=feedback[destination].data;pixelPitch=feedback[destination].pitch;
}
static void checkComposite(){
    for(unsigned stage=1;stage<=2;++stage){
        compositeStep(0,1,stage);bool passed=true;
        for(unsigned yi=0;yi<2;++yi)for(unsigned xi=0;xi<4;++xi){
            const unsigned x=64+xi*128,y=yi?224:64;const auto p=pixels+(y*pixelPitch+x)*4;
            const double red=(32*(1+xi))/255.0,blue=(32*(4-xi))/255.0;
            double expected[3];
            if(stage==1){expected[0]=5*pow(red,1.7);expected[1]=5*pow(red*.9,2);expected[2]=5*pow(red*.7,2.2);}
            else {const double low[]={.3,.2,.4},high[]={1,.8,.2},subtract=(.5-std::fabs(blue-.5))*2;for(unsigned c=0;c<3;++c)expected[c]=(low[c]+(high[c]-low[c])*blue-subtract)*1.333;}
            for(unsigned c=0;c<3;++c){const int byte=(int)(std::max(0.0,std::min(1.0,expected[c]))*255+.5);if(abs((int)p[c]-byte)>4)passed=false;
                printf("MILKDROP HLSL GPU %s X %u Y %u CHANNEL %u GOT %u EXPECT %d\n",compositeNames[stage],x,y,c,p[c],byte);}
        }
        printf("MILKDROP HLSL GPU %s %s\n",compositeNames[stage],passed?"PASS":"FAIL");
        if(!passed){note("HLSL GPU %s FAIL - SEND SCREENSHOT AND KLOG",compositeNames[stage]);hold(true);}
    }
}
static void pipelineWarpStep(unsigned source,unsigned destination,const PresetMesh &mesh,const float bands[3]){
    if(source>=2 || destination>=2 || source==destination)fail("FULL PRESET HISTORY ALIAS",-1);
    const float uniforms[]={512,288,1.0f/512,1.0f/288,bands[0],bands[1],bands[2],(bands[0]+bands[1]+bands[2])/3};
    memcpy(feedbackParams,uniforms,sizeof(uniforms));
    drawPresetGeometry(&feedback[destination].target,mesh,false,GNM_PT_TRILIST,false,pipelineWarpPS,feedback[source].table);
    pixels=feedback[destination].data;pixelPitch=feedback[destination].pitch;
}
static void pipelineCompositeStep(unsigned source,unsigned destination){
    if(source>=2 || destination>=2 || source==destination)fail("FULL COMPOSITE HISTORY ALIAS",-1);
    drawPresetGeometry(&feedback[destination].target,compositeQuad,false,GNM_PT_TRILIST,false,pipelineCompositePS,feedback[source].table);
    pixels=feedback[destination].data;pixelPitch=feedback[destination].pitch;
}
static void checkFullPipeline(){
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;p[0]=(unsigned)(255.0*x/(width-1)+.5);p[1]=(unsigned)(255.0*y/(height-1)+.5);p[2]=128;p[3]=255;}
    for(unsigned mode=0;mode<2;++mode){
        const float bands[]={1,1,mode?2.0f:0.0f};const double strength=mode?.030:.005;
        pipelineWarpStep(0,1,pipelineQuad,bands);bool ok=true;
        for(unsigned yi=0;yi<2;++yi)for(unsigned xi=0;xi<2;++xi){
            const unsigned x=xi?400:80,y=yi?220:60;const auto p=pixels+(y*pixelPitch+x)*4;
            const double inputX=(x+1.0)/(width-1),inputY=(y+1.0)/(height-1);
            const double sampleX=x+(inputX-.37)*strength*width,sampleY=y+(inputY-.37)*strength*height;
            const double expected[]={255.0*sampleX/(width-1)-1.02,255.0*sampleY/(height-1)-1.02,128-1.02};
            for(unsigned c=0;c<3;++c){const int value=(int)(expected[c]+.5);if(abs((int)p[c]-value)>2)ok=false;printf("MILKDROP FULL WARP CHECK MODE %u X %u Y %u C %u GOT %u EXPECT %d\n",mode,x,y,c,p[c],value);}
        }
        if(!ok){rowCount=0;note("MILKDROP 0.50 - FULL SHADER STARTUP CHECK");note("FULL HLSL WARP CHECK FAIL - SEND SCREENSHOT AND KLOG");hold(true);}
    }
    pipelineCompositeStep(0,1);
    const unsigned x=200,y=160;const auto p=pixels+(y*pixelPitch+x)*4;const auto reference=feedback[0].data+(y*feedback[0].pitch+x)*4;
    for(unsigned c=0;c<3;++c)if(abs((int)p[c]-(int)reference[c])>1){note("FULL COMPOSITE CHECK FAIL");hold(true);}
    printf("MILKDROP FULL PRESET WARP TREBLE RESPONSE AND COMPOSITE CHECKS PASS\n");
    // Restore the original asymmetric fixture for the established decay checks.
    for(unsigned yy=0;yy<height;++yy)for(unsigned xx=0;xx<width;++xx){auto q=feedback[0].data+(yy*feedback[0].pitch+xx)*4;q[0]=32*(1+xx/128);q[1]=yy<height/2?64:128;q[2]=32*(4-xx/128);q[3]=255;}
}
static bool checkSpatial(){
    SpatialWarp identity;PresetEquations::Output values={1,0,.5,1,1,1,0,0};
    if(!identity.build(values,0,1.0f/60,false))fail("IDENTITY SPATIAL MESH",-1);
    spatialStep(0,1,identity.mesh);
    bool passed=true;
    for(unsigned yi=0;yi<2;++yi)for(unsigned xi=0;xi<4;++xi){
        const unsigned x=64+xi*128,y=yi?224:64;auto p=pixels+(y*pixelPitch+x)*4;
        const unsigned expected[]={16*(1+xi),yi?64u:32u,16*(4-xi)};
        printf("MILKDROP SPATIAL IDENTITY X %u Y %u RGB %u %u %u EXPECT %u %u %u\n",x,y,p[0],p[1],p[2],expected[0],expected[1],expected[2]);
        for(unsigned c=0;c<3;++c)if(abs((int)p[c]-(int)expected[c])>2)passed=false;
    }
    if(!passed){note("GPU UV MESH FAIL - SEND SCREENSHOT AND KLOG");hold(true);}
    return passed;
}

static void createFeedback(GnmGpuMode mode)
{
    feedbackParams=(float *)gpuAlloc(48,16);
    feedbackCommands=gpuAlloc(feedbackCommandBytes,256);
    feedbackLabel=(volatile uint64_t *)gpuAlloc(8,8);
    if(!feedbackParams || !feedbackCommands || !feedbackLabel)fail("FEEDBACK CONTROL MEMORY",-1);
    memset(feedbackParams,0,48);
    for(unsigned i=0;i<2;++i) {
        auto &surface=feedback[i];
        GnmRenderTargetCreateInfo rt={};rt.colorfmt=GNM_FMT_R8G8B8A8_UNORM;
        rt.width=width;rt.height=height;rt.numslices=rt.numsamples=rt.numfragments=1;
        rt.colortilemodehint=GNM_TM_DISPLAY_LINEAR_ALIGNED;rt.mingpumode=mode;
        GnmError rc=sceGnmCreateRenderTarget(&surface.target,&rt);
        if(rc!=GNM_ERROR_OK)fail("FEEDBACK TARGET",rc);
        uint64_t rtBytes=0,texBytes=0;uint32_t rtAlign=0,texAlign=0;
        rc=sceGnmRtCalcByteSize(&rtBytes,&rtAlign,&surface.target);
        if(rc!=GNM_ERROR_OK)fail("FEEDBACK TARGET SIZE",rc);
        surface.pitch=sceGnmRtGetPitch(&surface.target);
        GnmTextureCreateInfo tex={};tex.format=GNM_FMT_R8G8B8A8_UNORM;
        tex.texturetype=GNM_TEXTURE_2D;tex.width=width;tex.height=height;tex.depth=1;
        tex.pitch=surface.pitch;tex.nummiplevels=tex.numslices=tex.numfragments=1;
        tex.tilemodehint=GNM_TM_DISPLAY_LINEAR_ALIGNED;tex.mingpumode=mode;
        rc=sceGnmCreateTexture(&surface.texture,&tex);
        if(rc!=GNM_ERROR_OK)fail("FEEDBACK TEXTURE VIEW",rc);
        rc=sceGnmTexCalcByteSize(&texBytes,&texAlign,&surface.texture);
        if(rc!=GNM_ERROR_OK)fail("FEEDBACK TEXTURE SIZE",rc);
        if(sceGnmTexGetPitch(&surface.texture)!=surface.pitch ||
           (uint64_t)surface.pitch*height*4>rtBytes)fail("FEEDBACK VIEW PITCH",-1);
        surface.bytes=(size_t)(rtBytes>texBytes?rtBytes:texBytes);
        surface.data=(unsigned char *)gpuAlloc(surface.bytes,rtAlign>texAlign?rtAlign:texAlign);
        surface.table=(unsigned char *)gpuAlloc(128,64);
        if(!surface.data || !surface.table)fail("FEEDBACK SURFACE MEMORY",-1);
        if((uintptr_t)surface.table>>32 != (uintptr_t)gpuMemory>>32)fail("FEEDBACK ADDRESS WINDOW",-1);
        memset(surface.data,0,surface.bytes);memset(surface.table,0,128);
        sceGnmRtSetBaseAddr(&surface.target,surface.data);
        sceGnmTexSetBaseAddress(&surface.texture,surface.data);
        GnmSampler sampler={};sampler.xymagfilter=sampler.xyminfilter=GNM_FILTER_BILINEAR;
        GnmBuffer buffer={};sceGnmBufSetBaseAddress(&buffer,feedbackParams);
        sceGnmBufSetFormat(&buffer,GNM_FMT_R32_FLOAT);
        sceGnmBufSetMemoryType(&buffer,GNM_MEMORY_READONLY,false,false);
        buffer.stride=0;buffer.numrecords=48;
        memcpy(surface.table,&surface.texture,sizeof(surface.texture));
        memcpy(surface.table+32,&sampler,sizeof(sampler));
        memcpy(surface.table+64,&buffer,sizeof(buffer));
        printf("MILKDROP FEEDBACK SURFACE %u DATA %p TABLE %p PITCH %u BYTES %u\n",
               i,surface.data,surface.table,surface.pitch,(unsigned)surface.bytes);
    }
}

static void feedbackStep(const GnmPsShader *shader,unsigned source,unsigned destination,
                         const float control[4],const float *presetSettings=nullptr)
{
    if(source>=2 || destination>=2 || source==destination)fail("FEEDBACK ALIAS",-1);
    // Called only after the previous EOP completed. No GPU resource is changed
    // while in flight; the writable target never aliases its sampled history.
    memcpy(feedbackParams,control,16);
    if(presetSettings)memcpy(feedbackParams+4,presetSettings,32);
    else memset(feedbackParams+4,0,32);
    *feedbackLabel=0;
    GnmCommandBuffer command=sceGnmCmdInit(feedbackCommands,feedbackCommandBytes,nullptr,nullptr);
    sceGnmDrawCmdInitDefaultHardwareState(&command);
    // Invalidate shader constants and texture caches, including non-volatile
    // descriptors. Previous EOP has already flushed finished color writes.
    if(command.endptr-command.cmdptr<7)fail("FEEDBACK CACHE COMMAND SPACE",-1);
    *command.cmdptr++=PKT3(PKT3_ACQUIRE_MEM,5,0);
    *command.cmdptr++=S_0301F0_SH_KCACHE_ACTION_ENA(1)|S_0301F0_TC_ACTION_ENA(1)|
                      S_0301F0_TCL1_ACTION_ENA(1)|S_0301F0_TC_WB_ACTION_ENA(1);
    *command.cmdptr++=0xffffffff;*command.cmdptr++=0xff;
    *command.cmdptr++=0;*command.cmdptr++=0;*command.cmdptr++=10;
    sceGnmDrawCmdWaitGraphicsWrite(&command,GNM_ACQUIRE_TARGET_CB0);
    contextRegister(&command,R_028808_CB_COLOR_CONTROL,S_028808_MODE(V_028808_CB_NORMAL)|S_028808_ROP3(0xcc));
    contextRegister(&command,R_028204_PA_SC_WINDOW_SCISSOR_TL,0x80000000);
    contextRegister(&command,R_028208_PA_SC_WINDOW_SCISSOR_BR,width|(height<<16));
    contextRegister(&command,R_028240_PA_SC_GENERIC_SCISSOR_TL,0x80000000);
    contextRegister(&command,R_028244_PA_SC_GENERIC_SCISSOR_BR,width|(height<<16));
    contextRegister(&command,R_028C38_PA_SC_AA_MASK_X0Y0_X1Y0,0xffffffff);
    contextRegister(&command,R_028C3C_PA_SC_AA_MASK_X0Y1_X1Y1,0xffffffff);
    GnmDbRenderControl db={};sceGnmDrawCmdSetDbRenderControl(&command,&db);
    GnmDepthStencilControl depth={};sceGnmDrawCmdSetDepthStencilControl(&command,&depth);
    GnmPrimitiveSetup primitive={};primitive.cullmode=GNM_CULL_NONE;
    primitive.frontface=GNM_FACE_CCW;primitive.frontmode=primitive.backmode=GNM_FILL_SOLID;
    primitive.provokemode=GNM_PROVOKINGVTX_FIRST;
    sceGnmDrawCmdSetPrimitiveSetup(&command,&primitive);
    sceGnmDrawCmdSetRenderTarget(&command,0,&feedback[destination].target);
    sceGnmDrawCmdSetRenderTargetMask(&command,0xf);
    GnmBlendControl overwrite={};sceGnmDrawCmdSetBlendControl(&command,0,&overwrite);
    GnmSetViewportInfo viewport={};viewport.dmin=0;viewport.dmax=1;
    viewport.scale[0]=width/2.0f;viewport.scale[1]=-(float)height/2.0f;viewport.scale[2]=0.5f;
    viewport.offset[0]=width/2.0f;viewport.offset[1]=height/2.0f;viewport.offset[2]=0.5f;
    sceGnmDrawCmdSetViewport(&command,0,&viewport);
    GnmViewportTransformControl transform={};
    transform.scalex=transform.offsetx=transform.scaley=transform.offsety=transform.scalez=transform.offsetz=transform.invertw=true;
    sceGnmDrawCmdSetViewportTransformControl(&command,&transform);
    sceGnmDrawCmdSetScreenScissor(&command,0,0,width,height);
    sceGnmDrawCmdSetHwScreenOffset(&command,0,0);
    sceGnmDrawCmdSetGuardBands(&command,1,1,1,1);
    sceGnmDrawCmdSetEmbeddedVsShader(&command,GNM_EMBEDDED_VSH_FULLSCREEN,0);
    sceGnmDrawCmdSetPsShader(&command,&shader->registers);
    bindResourceTable(&command,shader,feedback[source].table,false);
    sceGnmDrawCmdSetNumInstances(&command,1);
    sceGnmDrawCmdSetPrimitiveType(&command,GNM_PT_RECTLIST);
    sceGnmDrawCmdDrawIndexAuto(&command,3);
    const uint64_t expected=0x4859504e46423039ULL;
    sceGnmDrawCmdEventWriteEop(&command,GNM_CACHE_FLUSH_AND_INV_TS_EVENT,
        (uint64_t)(uintptr_t)feedbackLabel,GNM_DATA_SEL_SEND_DATA64,expected);
    const uint32_t dwords=(uint32_t)(command.cmdptr-command.beginptr);
    if(dwords>feedbackCommandBytes/4)fail("FEEDBACK COMMAND OVERFLOW",-1);
    void *buffers[]={command.beginptr};uint32_t sizes[]={dwords*4};
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    int rc=sceGnmSubmitCommandBuffers(1,buffers,sizes,nullptr,nullptr);
    if(rc<0)fail("FEEDBACK SUBMIT",rc);
    rc=sceGnmSubmitDone();if(rc<0)fail("FEEDBACK SUBMIT DONE",rc);
    const uint64_t started=sceKernelGetProcessTime();
    while(*feedbackLabel!=expected && sceKernelGetProcessTime()-started<3000000)sceKernelUsleep(1000);
    if(*feedbackLabel!=expected)fail("FEEDBACK GPU TIMEOUT",-1);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    pixels=feedback[destination].data;pixelPitch=feedback[destination].pitch;
}

static void checkProjectedDotCull(){
 auto waves=new CustomWaves();PresetEquations::Output base={};float pcm[512]={};
 if(!waves->load("/app0/assets/misc/projected-dots-control-0.48.milk",base) || !waves->build(pcm,pcm,base)){note("PROJECTED DOT CONTROL %s",waves->error.c_str());hold(false);}
 auto &s=waves->slots[0];bool ok=s.mesh.count==2 && *projectm_eval_context_register_variable(s.point.ctx,"scratch")==4;
 memset(feedback[0].data,0,feedback[0].bytes);drawStandardWave(&feedback[0].target,s.mesh,true,false,false,2);
 auto p=feedback[0].data+((height/2)*feedback[0].pitch+width/2)*4;auto edge=feedback[0].data+(64*feedback[0].pitch+64)*4;
 ok&=p[0]>250 && p[1]>250 && p[2]>250 && edge[0]==0;
 printf("MILKDROP PROJECTED DOT CULL VERTICES %u CENTER %u %u %u %s\n",s.mesh.count,p[0],p[1],p[2],ok?"PASS":"FAIL");
 delete waves;if(!ok){note("GPU PROJECTED DOT CONTROL FAILED - SEND KLOG");hold(false);}
}

static void checkPresetSharedMemory(){
 const char *path="/app0/assets/misc/preset-memory-control-0.49.milk";
 auto eq=new PresetEquations();auto warp=new SpatialWarp();auto shape=new CustomShapes();auto points=new CustomWaves();
 bool ok=eq->load(path,true,true) && warp->load(path,true,true,eq->sharedMemory) && shape->load(path,eq->initialOutput(),eq->sharedMemory) && points->load(path,eq->initialOutput(),eq->sharedMemory);
 float pcm[512]={};
 if(ok)ok=eq->evaluate(0,60,1,1,1) && warp->build(eq->output,0,1.f/60) && shape->build(eq->output) && points->build(pcm,pcm,eq->output);
 if(ok)ok=fabs(eq->output.q[0]-.21)<1e-9 && eq->output.q[1]==.75 && eq->output.q[2]==0 && fabs(shape->slots[0].copies[0][0]-.21)<1e-9 && fabs(points->slots[0].mesh.vertices[0].position[1]+.0042/.5625)<1e-6;
 auto candidate=new PresetEquations();
 if(ok)ok=eq->evaluate(1.f/60,60,1,1,1) && candidate->load(path,true,true) && candidate->evaluate(0,60,1,1,1) && fabs(candidate->output.q[0]-.21)<1e-9 && fabs(eq->sharedMemory->buffer[0][17]-.22)<1e-9;
 if(ok){eq->restart();ok=eq->sharedMemory->buffer[0][18]==0 && eq->evaluate(0,60,1,1,1) && fabs(eq->output.q[0]-.21)<1e-9;}
 printf("MILKDROP PRESET MEMORY CONTROL 8M SLOTS CROSS LAYERS ISOLATION RESTART %s\n",ok?"PASS":"FAIL");
 delete eq;delete warp;delete shape;delete points;delete candidate;
 if(!ok){note("PRESET MEMORY CONTROL FAILED - SEND KLOG");hold(false);}
}

static void checkCpuZoomCompatibility(){
 SpatialWarp warp;if(!warp.load("/app0/assets/misc/signed-zoom-control-0.50.milk",false,true)){note("CPU ZOOM CONTROL LOAD %s",warp.error.c_str());hold(false);}
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;p[0]=x<width/2?32:128;p[1]=y<height/2?64:192;p[2]=96;p[3]=255;}
 bool ok=true;
 for(double zoom:{-1.,1e13}){PresetEquations::Output v={};v.zoom=zoom;v.decay=1;
  if(!warp.build(v,0,1.f/60)){note("CPU ZOOM CONTROL MESH %s",warp.error.c_str());hold(false);}
  spatialStep(0,1,warp.mesh);
  for(unsigned row=0;row<2;++row)for(unsigned col=0;col<2;++col){auto p=feedback[1].data+(((row*2+1)*height/4)*feedback[1].pitch+(col*2+1)*width/4)*4;
   const int red=zoom<0?(col?32:128):80,green=zoom<0?(row?64:192):128;
   bool pass=abs((int)p[0]-red)<=2 && abs((int)p[1]-green)<=2 && abs((int)p[2]-96)<=2;ok&=pass;
   printf("MILKDROP CPU ZOOM CONTROL VALUE %.0f ROW %u COL %u RGB %u %u %u EXPECT %d %d 96 %s\n",zoom,row,col,p[0],p[1],p[2],red,green,pass?"PASS":"FAIL");
  }
 }
 if(!ok){note("GPU CPU-ZOOM COMPATIBILITY CONTROL FAILED - SEND KLOG");hold(false);}
}

static void runFeedback(const GnmPsShader *shader,GnmGpuMode mode)
{
    createFeedback(mode);
    createImageTextures(mode);
    createCommonRenderer(mode);
    createShapeTextureRenderer(mode);
    createLegacyFinalRenderer();
    checkLegacyFinal();
    checkShapeTextureRenderer();
    checkShapeViewport();
    checkNoiseResources();
    checkShaderCompatibility();
    checkLoopNoiseCompatibility();
    checkProjectedDotCull();
    checkConditionalLoops();
    checkPresetSharedMemory();
    checkCpuZoomCompatibility();
    geometryControl(&feedback[0].target,feedback[0].data,feedback[0].pitch,feedback[0].bytes);
    dotControl(&feedback[0].target,feedback[0].data,feedback[0].pitch,feedback[0].bytes);
    vectorLineControl(&feedback[0].target,feedback[0].data,feedback[0].pitch,feedback[0].bytes);
    geometryBatchControl(&feedback[0].target,feedback[0].data,feedback[0].pitch,feedback[0].bytes);
    checkCommonBlur();
    checkSamplerVariants();
    checkCommonReference();
    // Seed once on CPU; every following decay image comes from the previous
    // GPU render, with asymmetric vertical colors to catch flipped UVs.
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;
        p[0]=32*(1+x/128);p[1]=y<height/2?64:128;p[2]=32*(4-x/128);p[3]=255;
    }
    checkSpatial();
    checkComposite();
    checkImageTextures();
    checkFullPipeline();
    imageChoice=nextTextureRandom()&1;
    rowCount=0;
    note("MILKDROP TEST 0.50 - COMMUNITY PRESET SELECTOR - HYPN90001");
    note("REAL PROJECTM PARSER AND EVALUATOR - LIMITED NATIVE ADAPTER");
    std::string equationError;
    if(!runEquationChecks(equationError)) {
        note("EQUATION ERROR: %s",equationError.c_str());fail("PROJECTM EQUATION CHECK",-1);
    }
    note("PROJECTM EQUATION SELF-CHECK PASS");
    note("GPU VERTEX DATA CHECK PASS");
    auto bank=new CommunityBank();bank->load();
    auto preset=bank->equations[bank->active];auto community=bank->waves[bank->active];
    note("EIGHT BUILT-IN PRESETS - FAILURE FIXTURES REMOVED FROM MENU");
    note("ACTIVE 2/14 - GEISS - BASS ZOOM");
    note("SELECT 2/14 - GEISS - BASS ZOOM");
    note("UP DOWN BROWSE - X APPLY - TRIANGLE RESCAN - L1 R1 PAGE");
    note("STANDARD WAVE MODE 2 - 959 SMOOTHED POINTS");
    note("GPU LINE AND UV MESH CHECKS PASS");
    unsigned source=0;
    const float decayControl[4]={0,0,0,0.5f};
    for(unsigned pass=1;pass<=3;++pass) {
        const unsigned destination=source^1;
        feedbackStep(shader,source,destination,decayControl);
        bool success=true;
        for(unsigned yi=0;yi<2;++yi)for(unsigned xi=0;xi<4;++xi) {
            const unsigned y=yi?224:64,x=64+xi*128;
            const auto p=pixels+(y*pixelPitch+x)*4;
            const unsigned expected[3]={(32*(1+xi))>>pass,(yi?128u:64u)>>pass,(32*(4-xi))>>pass};
            printf("MILKDROP FEEDBACK CHECK %u X %u Y %u RGB %u %u %u EXPECT %u %u %u\n",
                pass,x,y,p[0],p[1],p[2],expected[0],expected[1],expected[2]);
            for(unsigned c=0;c<3;++c)if(abs((int)p[c]-(int)expected[c])>2)success=false;
        }
        note("GPU HISTORY DECAY %u %s",pass,success?"PASS":"FAIL - SEND SCREENSHOT AND KLOG");
        if(!success){notify("FEEDBACK CHECK FAILED");hold(true);}
        source=destination;
    }
    note("GPU FEEDBACK PASS - THREE SUCCESSIVE HISTORY READS");
    note("GPU HLSL, FILE AND NOISE TEXTURE CHECKS PASS");
    const GnmTexture presentationViews[2]={feedback[0].texture,feedback[1].texture};
    createPresentation(mode,presentationViews);
    notify("PROJECTM EQUATIONS READY - LIVE TRAILS STARTING");
    memset(feedback[0].data,0,feedback[0].bytes);
    memset(feedback[1].data,0,feedback[1].bytes);
    uint64_t previous=sceKernelGetProcessTime(),lastReport=previous;
    bank->openPad();
    bank->scanFolders();
    startTestAudio();
    TestAudioFrame audioFrame;AudioBandSmoother audioLevels;
    auto layers=new PresetLayers();PerformanceProbe probe;CuratedTour tour;
    float time=0;unsigned steps=0;
    for(;;) {
        const uint64_t now=sceKernelGetProcessTime();
        float dt=(now-previous)/1000000.0f;previous=now;
        if(dt<0.001f)dt=0.001f;if(dt>0.1f)dt=0.1f;
        const uint32_t pressed=bank->buttons();
        if(pressed&ORBIS_PAD_BUTTON_L2){
            if(tour.running){tour.stop();printf("MILKDROP TOUR CANCELLED\n");}
            else{
                unsigned items[192],count=0;char line[128];FILE *file=fopen("/app0/assets/misc/retry-failures-0.49.txt","rb");
                if(file){while(count<192 && fgets(line,sizeof(line),file)){line[strcspn(line,"\r\n")]=0;
                    for(unsigned i=CommunityBank::builtinCount;i<bank->count;++i){const char *name=strrchr(bank->paths[i].c_str(),'/');name=name?name+1:bank->paths[i].c_str();if(!strcmp(name,line)){items[count++]=i;break;}}
                }fclose(file);}
                if(tour.startList(items,count)){probe.normal(now);printf("MILKDROP TOUR START FIRST %u END %u DURATION 12 WARMUP 2 MODE RETRY TOTAL %u\n",tour.next,tour.end,count);}
                else printf("MILKDROP RETRY LIST EMPTY OR INVALID\n");
            }
        }
        if(pressed&ORBIS_PAD_BUTTON_R2){
            tour.start(bank->selected,CommunityBank::builtinCount,bank->count);probe.normal(now);
            printf("MILKDROP TOUR START FIRST %u END %u DURATION 12 WARMUP 2\n",tour.next,tour.end);
        }
        bool tourApply=false;
        if(tour.running && tour.measuring && (bank->stopped || tour.due(now))){
            const double seconds=now>tour.measureAt?(now-tour.measureAt)/1000000.:0;
            printf("MILKDROP TOUR RESULT INDEX %u STATUS %s FRAMES %u FPS %.2f AUDIO_FRAMES %u AUDIO_SEQ %llu %llu PEAK %.5f LIT %.4f %.4f MEAN %.3f %.3f PATH %s\n",
                bank->active,bank->stopped?"FRAME_REJECT":"RENDERED",tour.frames,seconds>0?tour.frames/seconds:0,tour.audioFrames,
                (unsigned long long)tour.firstSequence,(unsigned long long)tour.lastSequence,tour.peak,
                tour.samples?tour.litMin:0,tour.litMax,tour.samples?tour.meanMin:0,tour.meanMax,bank->paths[bank->active].c_str());
            if(bank->stopped)printf("MILKDROP TOUR FRAME_REASON INDEX %u REASON %s\n",bank->active,bank->reasons[bank->active].c_str());
            tour.finish(bank->stopped);
            if(!tour.running)printf("MILKDROP TOUR DONE RENDERED %u REJECTED %u\n",tour.completed,tour.rejected);
        }
        if(tour.running && tour.pending){
            bank->selected=tour.next;
            printf("MILKDROP TOUR BEGIN INDEX %u PATH %s\n",tour.next,bank->paths[tour.next].c_str());
            if(bank->prepare(bank->selected))tourApply=true;
            else{
                printf("MILKDROP TOUR RESULT INDEX %u STATUS LOAD_REJECT REASON %s PATH %s\n",tour.next,bank->reasons[tour.next].c_str(),bank->paths[tour.next].c_str());
                bank->reject(tour.next,"LOAD",bank->reasons[tour.next]);tour.finish(true);
                if(!tour.running)printf("MILKDROP TOUR DONE RENDERED %u REJECTED %u\n",tour.completed,tour.rejected);
                presentNative(bank->lastDisplay,time,true,bank->lastCustom);continue;
            }
        }
        if((pressed&ORBIS_PAD_BUTTON_SQUARE) && bank->active==7){imageChoice^=1;printf("MILKDROP IMAGE SELECT %u\n",imageChoice);}
        if((pressed&ORBIS_PAD_BUTTON_SQUARE) && bank->active!=7){compositeStage=(compositeStage+1)%3;printf("MILKDROP COMPOSITE SELECT %s\n",compositeNames[compositeStage]);}
        if((pressed&ORBIS_PAD_BUTTON_TRIANGLE) && !tour.running)bank->scanFolders();
        if((pressed&ORBIS_PAD_BUTTON_L3) && !tour.running){probe.manual(now);printf("MILKDROP PROFILE MODE %u %s\n",probe.mode,PerformanceProbe::name(probe.mode));}
        if((pressed&ORBIS_PAD_BUTTON_R3) && !tour.running){probe.start(now);printf("MILKDROP PROFILE SWEEP START PRESET %s\n",bank->paths[bank->active].c_str());}
        if(probe.tick(now))printf("MILKDROP PROFILE MODE %u %s%s\n",probe.mode,PerformanceProbe::name(probe.mode),probe.automatic?"":" - SWEEP DONE");

        if(pressed&ORBIS_PAD_BUTTON_L1)bank->selected=(bank->selected+bank->count-std::min(10u,bank->count))%bank->count;
        if(pressed&ORBIS_PAD_BUTTON_R1)bank->selected=(bank->selected+10)%bank->count;
        if(pressed&ORBIS_PAD_BUTTON_OPTIONS)bank->hideText=!bank->hideText;
        if(pressed&ORBIS_PAD_BUTTON_UP)bank->selected=(bank->selected+bank->count-1)%bank->count;
        if(pressed&ORBIS_PAD_BUTTON_DOWN)bank->selected=(bank->selected+1)%bank->count;
        if(pressed&ORBIS_PAD_BUTTON_CIRCLE)bank->errorVisible=false;
        if((tourApply || ((pressed&ORBIS_PAD_BUTTON_CROSS) && (bank->selected!=bank->active || bank->stopped))) && bank->accept()) {
            if(bank->active==7)imageChoice=nextTextureRandom()&1;preset=bank->equations[bank->active];community=bank->waves[bank->active];
            probe.normal(now);layers->restart();preset->restart();community->restart();bank->warps[bank->active]->restart();bank->shapes[bank->active]->restart(preset->initialOutput());bank->extras[bank->active]->restart(preset->initialOutput());time=0;
            const int slot=bank->shaderSlots[bank->active];
            if(slot>=0){
                // Match projectM MilkdropPreset::DrawInitialImage: bootstrap
                // from the previous visible image, rather than deleting it.
                // Both surfaces are fenced and have the same pitch/size.
                if(bank->lastDisplay!=source)memcpy(feedback[source].data,feedback[bank->lastDisplay].data,feedback[source].bytes);
                activateCommon();setCommonInputs(preset->initialOutput());updateCommonBlur(source,commonPrograms[slot].blurLevels);
                printf("MILKDROP COMMON INITIAL IMAGE PRESERVED FROM %u TO %u\n",bank->lastDisplay,source);
            }else {
                // Keep the established reset behavior of legacy test entries.
                memset(feedback[0].data,0,feedback[0].bytes);memset(feedback[1].data,0,feedback[1].bytes);
            }
            activateLegacyFinal();
            printf("MILKDROP SELECT APPLY %u %s\n",bank->active,bank->names[bank->active].c_str());
            if(tourApply)tour.loaded(sceKernelGetProcessTime());else tour.stop();
        }
        snprintf(rows[4],100,"%s",bank->folderStatus);
        const int commonSlot=bank->shaderSlots[bank->active];
        presentationGain=community->gamma;
        snprintf(rows[5],100,"ACTIVE %u/%u - %s",bank->active+1,bank->count,bank->names[bank->active].c_str());
        snprintf(rows[6],100,"SELECT %u/%u - %s",bank->selected+1,bank->count,bank->names[bank->selected].c_str());
        snprintf(rows[8],100,"SHAPES %u - WAVE %d - %s",bank->shapes[bank->active]->enabled,community->mode,(bank->active==6 || bank->active==7)?"FULL SHADER PIPELINE":bank->warps[bank->active]->enabled?"PER PIXEL WARP":"UNIFORM FEEDBACK");
        if(commonSlot>=0)snprintf(rows[13],100,community->fixedFinal?"IMPORTED WARP + LEGACY ECHO/FILTERS - %u BLUR LEVELS":"IMPORTED WARP + COMPOSITE - %u BLUR LEVELS",commonPrograms[commonSlot].blurLevels);
        else if(bank->active==7)snprintf(rows[13],100,"SQUARE IMAGE %s - FULL TEXTURED PRESET",imageChoice?"COPPER":"AURORA");
        else if(bank->active==6)snprintf(rows[13],100,"FULL PRESET - OWN WARP AND COMPOSITE SHADERS");
        else if(community->fixedFinal && compositeStage==0)snprintf(rows[13],100,"LEGACY ECHO / GAMMA / FINAL FILTERS");
        else snprintf(rows[13],100,"SQUARE SHADER %s - SHADER STAGE ONLY",compositeNames[compositeStage]);
        snprintf(rows[9],100,"%s",bank->ready[bank->selected]?"SELECTED PRESET READY":bank->examined[bank->selected]?"SELECTED PRESET UNSUPPORTED - X SHOWS REASON":"IMPORTED FILE - X CHECK AND APPLY");
        rows[10][0]=rows[11][0]=0;
        snprintf(rows[12],100,"%.98s",bank->selected>=CommunityBank::builtinCount?bank->paths[bank->selected].c_str():bank->folderStatus);
        bank->showError();
        if(bank->stopped){presentNative(bank->lastDisplay,time,!bank->hideText || bank->errorVisible,bank->lastCustom);continue;}
        time+=dt;
        readTestAudio(audioFrame);audioLevels.update(audioFrame,dt);
        const auto levels=(bank->active>=CommunityBank::builtinCount || bank->active==6 || bank->active==7 || commonSlot>=0)?audioLevels.relative:audioLevels.bands;
        const uint64_t equationStarted=sceKernelGetProcessTime();
        if(!preset->evaluate(time,1.0/dt,levels[0],levels[1],levels[2],audioLevels.attenuated[0],audioLevels.attenuated[1],audioLevels.attenuated[2])) {
            bank->frameError("EQUATION FRAME",preset->error);continue;
        }
        const uint64_t renderStarted=sceKernelGetProcessTime();
        const auto &v=preset->output;
        // This table is used by the stock warp. Explicit HLSL sampler aliases
        // have independent immutable addressing/filter descriptors.
        for(auto &surface:feedback){GnmSampler sampler={};sampler.xymagfilter=sampler.xyminfilter=GNM_FILTER_BILINEAR;sampler.clampx=sampler.clampy=sampler.clampz=v.textureWrap>.0001?GNM_TEX_CLAMP_WRAP:GNM_TEX_CLAMP_CLAMP_LAST_TEXEL;memcpy(surface.table+32,&sampler,sizeof(sampler));}

        const float control[4]={3,dt,time,(float)pow(presetSurfaceDecay(v.decay),dt*60.0)};
        if(!community->build(audioFrame.fullLeft,audioFrame.fullRight,v,time)) {
            bank->frameError("WAVE FRAME",community->error.empty()?"Geometry capacity exceeded":community->error);continue;
        }
        auto shapes=bank->shapes[bank->active];
        if(!shapes->build(v)){bank->frameError("CUSTOM SHAPE FRAME",shapes->error);continue;}
        const uint64_t pointStarted=sceKernelGetProcessTime();
        auto custom=bank->extras[bank->active];if(probe.evaluate() && !custom->build(audioFrame.fullLeft,audioFrame.fullRight,v,audioFrame.spectrumLeft,audioFrame.spectrumRight)){bank->frameError("CUSTOM POINT FRAME",custom->error);continue;}
        const uint64_t pointFinished=sceKernelGetProcessTime();uint64_t warpUs=0,blurUs=0;

        auto spatial=bank->warps[bank->active];
        if(!layers->buildVectors(v) || !layers->buildBorder(v)){bank->frameError("LAYER FRAME",layers->error.empty()?"Border geometry capacity":layers->error);continue;}
        // Previous-frame vector field is drawn into source before the warp.
        for(unsigned batch=0;batch<layers->vectorBatches;++batch)drawPresetGeometry(&feedback[source].target,layers->vectors[batch],true,GNM_PT_LINELIST,false,nullptr,nullptr,false,width,height,100);

        if(spatial->enabled){
            if(!spatial->build(v,time,dt)){bank->frameError("WARP FRAME",spatial->error);continue;}
            if(commonSlot>=0){const uint64_t w=sceKernelGetProcessTime();setCommonInputs(v);drawCommon(source,source^1,spatial->mesh,commonSlot,false);const uint64_t b=sceKernelGetProcessTime();warpUs=b-w;updateCommonBlur(source,commonPrograms[commonSlot].blurLevels);blurUs=sceKernelGetProcessTime()-b;}
            else if(bank->active==7){setTextureUniforms(audioLevels.relative);texturedStep(source,source^1,spatial->mesh,brainWarpPS);}
            else if(bank->active==6)pipelineWarpStep(source,source^1,spatial->mesh,audioLevels.relative);
            else spatialStep(source,source^1,spatial->mesh);
        }else{
            const float inverse=(float)pow(v.zoom,-dt*60.0);
            if(!std::isfinite(inverse)){bank->frameError("WARP FRAME","Non-finite legacy feedback inverse zoom");continue;}
            const float settings[8]={inverse,(float)(v.rot*dt*60.0),(float)(v.dx*dt*60.0),(float)(v.dy*dt*60.0),(float)v.red,(float)v.green,(float)v.blue,1};
            feedbackStep(shader,source,source^1,control,settings);
        }
        beginGeometryBatch(&feedback[source^1].target);
        for(auto &shape:shapes->slots)if(shape.enabled)for(const auto &copy:shape.copies){
            std::string error;
            if(!CustomShapes::geometry(shape,copy,error))fail("VALIDATED SHAPE GEOMETRY",-1);
            if(shape.fill.count){
                if(geometryBatchCount==geometryBatchCapacity){endGeometryBatch();beginGeometryBatch(&feedback[source^1].target);}
                if(copy[19]!=0)drawTexturedShape(source,source^1,shape);
                else drawPresetGeometry(&feedback[source^1].target,shape.fill,true,GNM_PT_TRILIST,false,nullptr,nullptr,shape.additive);
            }
            if(shape.border.count){
                if(geometryBatchCount==geometryBatchCapacity){endGeometryBatch();beginGeometryBatch(&feedback[source^1].target);}
                drawPresetGeometry(&feedback[source^1].target,shape.border,true,GNM_PT_LINESTRIP,shape.thick,nullptr,nullptr,shape.additive);
            }
        }
        endGeometryBatch();
        const uint64_t pointDrawStarted=sceKernelGetProcessTime();
        if(probe.draw()){
            beginGeometryBatch(&feedback[source^1].target);
            for(auto &slot:custom->slots)if(slot.enabled && slot.mesh.count)drawStandardWave(&feedback[source^1].target,slot.mesh,slot.dots,slot.thick,slot.additive,slot.thick?2:1);
            endGeometryBatch();
        }
        const uint64_t pointDrawUs=sceKernelGetProcessTime()-pointDrawStarted;

        drawStandardWave(&feedback[source^1].target,community->mesh,community->dots,community->thick,community->additive);
        drawStandardWave(&feedback[source^1].target,community->secondMesh,community->dots,community->thick,community->additive);
        if(v.darkenCenter>0)drawDarkenCentre(source^1);
        if(layers->border.count)drawPresetGeometry(&feedback[source^1].target,layers->border,true);
        layers->capture(spatial->mesh);
        const uint64_t warpFinished=sceKernelGetProcessTime();
        source^=1;++steps;
        // Presentation-only shader: the source history remains untouched.
        // The other surface is overwritten by the next frame's feedback draw.
        const bool legacyFinal=bank->active>=CommunityBank::builtinCount && community->fixedFinal && compositeStage==0;
        if(legacyFinal)drawLegacyFinal(source,source^1,v,community->finalFilters);
        else if(commonSlot>=0)drawCommon(source,source^1,pipelineQuad,commonSlot,true);
        else if(bank->active==7)texturedStep(source,source^1,compositeQuad,brainCompositePS);
        else if(bank->active==6)pipelineCompositeStep(source,source^1);
        else if(compositeStage)compositeStep(source,source^1,compositeStage);
        const bool customComposite=legacyFinal || (bank->active==6 || bank->active==7 || commonSlot>=0) || compositeStage!=0;
        bank->lastDisplay=customComposite?source^1:source;bank->lastCustom=customComposite;
        const uint64_t compositeFinished=sceKernelGetProcessTime();
        if(!bank->errorVisible){const double divisor=probe.samples?probe.samples*1000.:1000.;
            snprintf(rows[10],100,"MS POINT CPU %.1f DRAW %.1f WARP %.1f BLUR %.1f COMP %.1f",probe.total[1]/divisor,probe.total[2]/divisor,probe.total[3]/divisor,probe.total[4]/divisor,probe.total[5]/divisor);
            snprintf(rows[11],100,"%s - R2 FULL TOUR - L2 RETRY/STOP - OPTIONS HUD",tour.running?"PRESET TOUR RUNNING":"CURATED TEST READY");
        }
        presentNative(bank->lastDisplay,time,!bank->hideText || bank->errorVisible,customComposite);
        const uint64_t tourNow=sceKernelGetProcessTime();
        if(tour.running && tour.measuring && tourNow>=tour.measureAt){
            ++tour.frames;if(audioFrame.available){++tour.audioFrames;if(!tour.firstSequence)tour.firstSequence=audioFrame.sequence;tour.lastSequence=audioFrame.sequence;tour.peak=std::max(tour.peak,(double)audioFrame.peak);}
            if(tourNow>=tour.nextSample){
                const auto &surface=feedback[bank->lastDisplay];unsigned total=0,lit=0,n=0;
                for(unsigned y=0;y<height;y+=16)for(unsigned x=0;x<width;x+=16){const auto pixel=surface.data+(y*surface.pitch+x)*4;total+=pixel[0]+pixel[1]+pixel[2];lit+=std::max(pixel[0],std::max(pixel[1],pixel[2]))>8;++n;}
                double fraction=lit/(double)n,mean=total/(3.*n);tour.litMin=std::min(tour.litMin,fraction);tour.litMax=std::max(tour.litMax,fraction);tour.meanMin=std::min(tour.meanMin,mean);tour.meanMax=std::max(tour.meanMax,mean);++tour.samples;tour.nextSample=tourNow+1000000;
            }
        }
        const uint64_t values[]={renderStarted-equationStarted,pointFinished-pointStarted,pointDrawUs,warpUs,blurUs,compositeFinished-warpFinished,presentationGpuUs,presentationHudUs,presentationWaitUs,sceKernelGetProcessTime()-renderStarted};probe.add(values);
        if(now-probe.reportAt>=1000000 && probe.samples){
            printf("MILKDROP PROFILE PRESET %s MODE %u %s FPS %.2f HUD %u SAMPLES %u\n",bank->paths[bank->active].c_str(),probe.mode,PerformanceProbe::name(probe.mode),presentationFps,!bank->hideText,probe.samples);
            printf("MILKDROP PROFILE US EQUATIONS %llu POINT_CPU %llu POINT_DRAW %llu WARP %llu BLUR %llu COMPOSITE %llu DISPLAY %llu HUD %llu VSYNC %llu FRAME %llu\n",(unsigned long long)(probe.total[0]/probe.samples),(unsigned long long)(probe.total[1]/probe.samples),(unsigned long long)(probe.total[2]/probe.samples),(unsigned long long)(probe.total[3]/probe.samples),(unsigned long long)(probe.total[4]/probe.samples),(unsigned long long)(probe.total[5]/probe.samples),(unsigned long long)(probe.total[6]/probe.samples),(unsigned long long)(probe.total[7]/probe.samples),(unsigned long long)(probe.total[8]/probe.samples),(unsigned long long)(probe.total[9]/probe.samples));
            probe.resetStats(now);
        }

        if(now-lastReport>(commonSlot>=0?30000000u:5000000u)) {
            if(commonSlot>=0){
                unsigned lit=0,maximum=0,samples=0;uint64_t total=0;
                const auto &surface=feedback[source];
                for(unsigned y=0;y<height;y+=16)for(unsigned x=0;x<width;x+=16){
                    const auto p=surface.data+(y*surface.pitch+x)*4;
                    const unsigned m=std::max((unsigned)p[0],std::max((unsigned)p[1],(unsigned)p[2]));
                    lit+=m>8;maximum=std::max(maximum,m);total+=p[0]+p[1]+p[2];++samples;
                }
                printf("MILKDROP COMMON FRAME US FEEDBACK %llu COMPOSITE %llu PRESENT %llu\n",(unsigned long long)(warpFinished-renderStarted),(unsigned long long)(compositeFinished-warpFinished),(unsigned long long)(sceKernelGetProcessTime()-compositeFinished));
                printf("MILKDROP COMMON HISTORY SAMPLE LIT %u MAX %u MEAN %.3f BLUR LEVELS %u\n",lit,maximum,total/(samples*3.0),commonPrograms[commonSlot].blurLevels);
            }
            printf("MILKDROP FEEDBACK ALIVE STEPS %u SIMULATION %.2f\n",steps,time);
            printf("MILKDROP PRESET FRAME ZOOM %.6f ROT %.6f DECAY %.6f COLOR %.3f %.3f %.3f\n",
                   v.zoom,v.rot,v.decay,v.red,v.green,v.blue);
            printf("MILKDROP AUDIO REACT ACTIVE %u PEAK %.4f BANDS %.3f %.3f %.3f SEQ %llu\n",audioFrame.available,audioFrame.peak,audioLevels.bands[0],audioLevels.bands[1],audioLevels.bands[2],(unsigned long long)audioFrame.sequence);
            printf("MILKDROP LOUDNESS RELATIVE %.5f %.5f %.5f ATT %.5f %.5f %.5f\n",audioLevels.relative[0],audioLevels.relative[1],audioLevels.relative[2],audioLevels.attenuated[0],audioLevels.attenuated[1],audioLevels.attenuated[2]);
            for(unsigned i=0;i<4;++i){const auto &slot=custom->slots[i];if(!slot.enabled)continue;unsigned inside=0;float maxAlpha=0;
                for(unsigned j=0;j<slot.mesh.count;++j){const auto &p=slot.mesh.vertices[j];inside+=fabsf(p.position[0])<=1 && fabsf(p.position[1])<=1;maxAlpha=std::max(maxAlpha,p.color[3]);}
                printf("MILKDROP CUSTOM WAVE %u VERTICES %u INSIDE %u ALPHA %.4f SPECTRUM %u\n",i,slot.mesh.count,inside,maxAlpha,slot.spectrum);
            }
            if(bank->active==6)printf("MILKDROP FULL CAULDRON RELATIVE %.3f %.3f %.3f CENTER %.4f %.4f\n",audioLevels.relative[0],audioLevels.relative[1],audioLevels.relative[2],v.cx,v.cy);
            lastReport=now;
        }
    }
}
