#pragma once
// Direct GPU draws into Scene2D's registered VideoOut buffers. Only diagnostic
// text stays CPU-rendered; animation images are never copied to the CPU here.
static GnmPsShader *presentationPS=nullptr;
static GnmRenderTarget presentationTargets[2]={};
static unsigned char *presentationTables[2]={};
static void *presentationCommands=nullptr;
static volatile uint64_t *presentationLabel=nullptr;
static double presentationFps=0;
static float *presentationParams=nullptr;
static float presentationGain=1;
static uint64_t presentationGpuUs=0,presentationHudUs=0,presentationWaitUs=0;
static const char presentation_frag[]=R"glsl(#version 450
layout(set=0,binding=0) uniform sampler2D image;
layout(set=0,binding=1,std140) uniform Display { vec4 clock; } display;
vec3 shade(float i) {
    float t=display.clock.x;
    vec3 color=0.6+0.3*sin(vec3(t*0.429+3.0+i*21.0,t*0.321+1.0+i*13.0,t*0.387+6.0+i*9.0));
    color/=max(color.r,max(color.g,color.b));return 0.5+0.5*color;
}
layout(location=0) out vec4 outColor;
void main() {
    vec2 uv=(gl_FragCoord.xy-vec2(448.0,480.0))/vec2(1024.0,576.0);
    vec3 color=vec3(0.0);
    if(uv.x>=0.0 && uv.y>=0.0 && uv.x<=1.0 && uv.y<=1.0)
    {
        vec3 tint=uv.x+uv.y<=1.0?shade(0.0)+(shade(1.0)-shade(0.0))*uv.x+(shade(2.0)-shade(0.0))*uv.y:
            shade(3.0)+(shade(2.0)-shade(3.0))*(1.0-uv.x)+(shade(1.0)-shade(3.0))*(1.0-uv.y);
        vec3 sampleColor=textureLod(image,uv,0.0).rgb;
        color=clamp(display.clock.z>0.5?sampleColor:sampleColor*tint*display.clock.y,vec3(0.0),vec3(1.0));
    }
    if(gl_FragCoord.y>=1064.0 && gl_FragCoord.x<192.0)
        color=gl_FragCoord.x<64.0?vec3(1,0,0):gl_FragCoord.x<128.0?vec3(0,1,0):vec3(0,0,1);
    outColor=vec4(color,1.0);
}
)glsl";
static void createPresentationShader(PsbcTarget target)
{
    const PsbcResourceType binding[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER};
    PsbcCompileOptions options={target,PSBC_STAGE_FRAGMENT,"main",true};
    options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);
    options.resource_count=2;options.resources=binding;
    auto output=compileText(target,PSBC_STAGE_FRAGMENT,presentation_frag,&options);
    presentationPS=(GnmPsShader *)prepareShader(output,false);psbc_free_output(&output);
}
static void createPresentation(GnmGpuMode mode,const GnmTexture textures[2])
{
    presentationParams=(float *)gpuAlloc(16,16);
    if(!presentationParams)fail("DISPLAY PARAMETER MEMORY",-1);
    memset(presentationParams,0,16);
    for(unsigned i=0;i<2;++i) {
        GnmRenderTargetCreateInfo info={};info.colorfmt=GNM_FMT_B8G8R8A8_UNORM;
        info.width=1920;info.height=1080;info.pitch=1920;
        info.numslices=info.numsamples=info.numfragments=1;
        info.colortilemodehint=GNM_TM_DISPLAY_LINEAR_ALIGNED;info.mingpumode=mode;
        GnmError rc=sceGnmCreateRenderTarget(&presentationTargets[i],&info);
        if(rc!=GNM_ERROR_OK)fail("DISPLAY GPU TARGET",rc);
        uint64_t bytes=0;uint32_t alignment=0;
        rc=sceGnmRtCalcByteSize(&bytes,&alignment,&presentationTargets[i]);
        printf("MILKDROP DISPLAY LAYOUT %u REQUIRED %u CAPACITY %u ALIGN %u PITCH %u\n",i,(unsigned)bytes,(unsigned)screen->GetFrameBufferCapacity(),alignment,sceGnmRtGetPitch(&presentationTargets[i]));
        if(rc!=GNM_ERROR_OK || bytes>(uint64_t)screen->GetFrameBufferCapacity() ||
           sceGnmRtGetPitch(&presentationTargets[i])!=1920 || !alignment ||
           (uintptr_t)screen->GetFrameBuffer(i)%alignment)fail("DISPLAY BUFFER LAYOUT",rc!=GNM_ERROR_OK?rc:-1);
        sceGnmRtSetBaseAddr(&presentationTargets[i],screen->GetFrameBuffer(i));
        presentationTables[i]=(unsigned char *)gpuAlloc(128,64);
        if(!presentationTables[i] || (uintptr_t)presentationTables[i]>>32 != (uintptr_t)gpuMemory>>32)
            fail("DISPLAY RESOURCE TABLE",-1);
        memset(presentationTables[i],0,128);
        GnmSampler sampler={};sampler.xymagfilter=sampler.xyminfilter=GNM_FILTER_BILINEAR;
        memcpy(presentationTables[i],&textures[i],sizeof(GnmTexture));
        memcpy(presentationTables[i]+32,&sampler,sizeof(sampler));
        GnmBuffer buffer={};sceGnmBufSetBaseAddress(&buffer,presentationParams);
        sceGnmBufSetFormat(&buffer,GNM_FMT_R32_FLOAT);
        sceGnmBufSetMemoryType(&buffer,GNM_MEMORY_READONLY,false,false);
        buffer.stride=0;buffer.numrecords=16;
        memcpy(presentationTables[i]+64,&buffer,sizeof(buffer));
        printf("MILKDROP GPU DISPLAY BUFFER %u ADDRESS %p BYTES %u PITCH %u\n",
               i,screen->GetFrameBuffer(i),(unsigned)bytes,sceGnmRtGetPitch(&presentationTargets[i]));
    }
    presentationCommands=gpuAlloc(0x10000,256);
    presentationLabel=(volatile uint64_t *)gpuAlloc(8,8);
    if(!presentationCommands || !presentationLabel)fail("DISPLAY COMMAND MEMORY",-1);
    notify("NATIVE GPU DISPLAY READY - STARTING VSYNC");
}
static void presentNative(unsigned source,float elapsed=0,bool showText=true,bool customComposite=false)
{
    if(source>=2)fail("DISPLAY SOURCE INDEX",-1);
    const uint64_t presentationStarted=sceKernelGetProcessTime();
    const unsigned index=screen->GetActiveFrameBufferIndex();
    if(index>=2)fail("DISPLAY BUFFER INDEX",-1);
    presentationParams[0]=elapsed;presentationParams[1]=presentationGain;presentationParams[2]=customComposite?1.0f:0.0f;
    *presentationLabel=0;
    GnmCommandBuffer command=sceGnmCmdInit(presentationCommands,0x10000,nullptr,nullptr);
    sceGnmDrawCmdInitDefaultHardwareState(&command);
    sceGnmDrawCmdWaitUntilSafeForRendering(&command,screen->GetVideoHandle(),index);
    *command.cmdptr++=PKT3(PKT3_ACQUIRE_MEM,5,0);
    *command.cmdptr++=S_0301F0_SH_KCACHE_ACTION_ENA(1)|S_0301F0_TC_ACTION_ENA(1)|
                      S_0301F0_TCL1_ACTION_ENA(1)|S_0301F0_TC_WB_ACTION_ENA(1);
    *command.cmdptr++=0xffffffff;*command.cmdptr++=0xff;
    *command.cmdptr++=0;*command.cmdptr++=0;*command.cmdptr++=10;
    sceGnmDrawCmdWaitGraphicsWrite(&command,GNM_ACQUIRE_TARGET_CB0);
    contextRegister(&command,R_028808_CB_COLOR_CONTROL,S_028808_MODE(V_028808_CB_NORMAL)|S_028808_ROP3(0xcc));
    contextRegister(&command,R_028204_PA_SC_WINDOW_SCISSOR_TL,0x80000000);
    contextRegister(&command,R_028208_PA_SC_WINDOW_SCISSOR_BR,1920|(1080<<16));
    contextRegister(&command,R_028240_PA_SC_GENERIC_SCISSOR_TL,0x80000000);
    contextRegister(&command,R_028244_PA_SC_GENERIC_SCISSOR_BR,1920|(1080<<16));
    contextRegister(&command,R_028C38_PA_SC_AA_MASK_X0Y0_X1Y0,0xffffffff);
    contextRegister(&command,R_028C3C_PA_SC_AA_MASK_X0Y1_X1Y1,0xffffffff);
    GnmDbRenderControl db={};sceGnmDrawCmdSetDbRenderControl(&command,&db);
    GnmDepthStencilControl depth={};sceGnmDrawCmdSetDepthStencilControl(&command,&depth);
    GnmPrimitiveSetup primitive={};primitive.cullmode=GNM_CULL_NONE;
    primitive.frontface=GNM_FACE_CCW;primitive.frontmode=primitive.backmode=GNM_FILL_SOLID;
    primitive.provokemode=GNM_PROVOKINGVTX_FIRST;
    sceGnmDrawCmdSetPrimitiveSetup(&command,&primitive);
    sceGnmDrawCmdSetRenderTarget(&command,0,&presentationTargets[index]);
    sceGnmDrawCmdSetRenderTargetMask(&command,0xf);
    GnmBlendControl overwrite={};sceGnmDrawCmdSetBlendControl(&command,0,&overwrite);
    GnmSetViewportInfo viewport={};viewport.dmin=0;viewport.dmax=1;
    viewport.scale[0]=960;viewport.scale[1]=-540;viewport.scale[2]=0.5f;
    viewport.offset[0]=960;viewport.offset[1]=540;viewport.offset[2]=0.5f;
    sceGnmDrawCmdSetViewport(&command,0,&viewport);
    GnmViewportTransformControl transform={};
    transform.scalex=transform.offsetx=transform.scaley=transform.offsety=transform.scalez=transform.offsetz=transform.invertw=true;
    sceGnmDrawCmdSetViewportTransformControl(&command,&transform);
    sceGnmDrawCmdSetScreenScissor(&command,0,0,1920,1080);
    sceGnmDrawCmdSetHwScreenOffset(&command,0,0);
    sceGnmDrawCmdSetGuardBands(&command,1,1,1,1);
    sceGnmDrawCmdSetEmbeddedVsShader(&command,GNM_EMBEDDED_VSH_FULLSCREEN,0);
    sceGnmDrawCmdSetPsShader(&command,&presentationPS->registers);
    bindResourceTable(&command,presentationPS,presentationTables[source],false);
    sceGnmDrawCmdSetNumInstances(&command,1);
    sceGnmDrawCmdSetPrimitiveType(&command,GNM_PT_RECTLIST);
    sceGnmDrawCmdDrawIndexAuto(&command,3);
    const uint64_t expected=0x4859504e50524553ULL;
    sceGnmDrawCmdEventWriteEop(&command,GNM_CACHE_FLUSH_AND_INV_TS_EVENT,
        (uint64_t)(uintptr_t)presentationLabel,GNM_DATA_SEL_SEND_DATA64,expected);
    const uint32_t dwords=(uint32_t)(command.cmdptr-command.beginptr);
    if(dwords>0x10000/4)fail("DISPLAY COMMAND SPACE",-1);
    void *buffers[]={command.beginptr};uint32_t sizes[]={dwords*4};
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    int rc=sceGnmSubmitCommandBuffers(1,buffers,sizes,nullptr,nullptr);
    if(rc<0)fail("DISPLAY GPU SUBMIT",rc);
    rc=sceGnmSubmitDone();if(rc<0)fail("DISPLAY SUBMIT DONE",rc);
    const uint64_t started=sceKernelGetProcessTime();
    while(*presentationLabel!=expected && sceKernelGetProcessTime()-started<3000000)sceKernelUsleep(1000);
    if(*presentationLabel!=expected)fail("DISPLAY GPU TIMEOUT",-1);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    const uint64_t gpuFinished=sceKernelGetProcessTime();presentationGpuUs=gpuFinished-presentationStarted;
    // The GPU is finished with the display buffer. CPU writes only small
    // diagnostic glyphs, not the image or a full-frame background clear.
    if(showText)for(unsigned i=0;i<rowCount;++i)drawStatusText(screen,rows[i],44+i*25);
    char timing[100];snprintf(timing,sizeof(timing),"PRESENT FPS %.1f - GPU DISPLAY - RGB CHECK LOWER LEFT",presentationFps);
    if(showText){drawStatusText(screen,timing,420);drawStatusText(screen,testAudioStatus(),450);}
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    const uint64_t hudFinished=sceKernelGetProcessTime();presentationHudUs=hudFinished-gpuFinished;
    rc=sceVideoOutSubmitFlip(screen->GetVideoHandle(),index,ORBIS_VIDEO_OUT_FLIP_VSYNC,frame);
    if(rc<0)fail("DISPLAY VSYNC FLIP",rc);
    screen->FrameWait(frame);screen->FrameBufferSwap();++frame;
    static uint64_t windowStart=0;static unsigned displayed=0;
    const uint64_t now=sceKernelGetProcessTime();presentationWaitUs=now-hudFinished;
    if(!windowStart)windowStart=now;
    else ++displayed;
    if(now-windowStart>=1000000) {
        presentationFps=displayed*1000000.0/(now-windowStart);
        printf("MILKDROP NATIVE PRESENT FPS %.2f\n",presentationFps);
        windowStart=now;displayed=0;
    }
}
