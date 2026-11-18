// HYPNOTIC separate native GPU experiment. Renderer/command setup follows
// OpenGNM/freegnm-examples (MIT). Compiler: OpenGNM PSBC / Mesa NIR+ACO.
// Display is CPU Scene2D; generated shaders execute into a separate GPU target.
#include "graphics.h"
#include "status_text.h"
#include "log.h"
#include "spirv_fixtures.h"
#include "solid_fragment.h"
#include "psbc_compile.h"
#include <gnm.h>
#include <pm4/pm4_ps4.h>
#include <pm4/sid.h>
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>
#include <orbis/Pad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

std::stringstream debugLogStream;
static char rows[17][100];
static unsigned rowCount=0;
static FILE *file=nullptr;
static Scene2D *screen=nullptr;
static unsigned char *pixels=nullptr;
static unsigned pixelPitch=0;
static int frame=0;
static const unsigned width=512,height=288;

static void note(const char *format,...)
{
    char text[512];va_list args;va_start(args,format);
    vsnprintf(text,sizeof(text),format,args);va_end(args);
    printf("MILKDROP TEST 0.50: %s\n",text);
    sceKernelDebugOutText(0,text);
    if(file){fprintf(file,"%s\n",text);fflush(file);}
    if(rowCount<17)snprintf(rows[rowCount++],100,"%.98s",text);
}
static void notify(const char *text){printf("MILKDROP STATUS: %s\n",text);}

static void present(bool image)
{
    screen->FrameBufferFill(Color{0,0,0});
    for(unsigned i=0;i<rowCount;++i)drawStatusText(screen,rows[i],44+i*25);
    if(image && pixels)
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)
        {
            const unsigned char *p=pixels+(y*pixelPitch+x)*4;
            screen->DrawRectangle(448+x*2,480+y*2,2,2,Color{p[0],p[1],p[2]});
        }
    screen->SubmitFlip(frame);screen->FrameWait(frame);screen->FrameBufferSwap();++frame;
}
static void hold(bool image)
{
    if(file){fclose(file);file=nullptr;}
    // Populate both display buffers once; the GPU image is a static readback.
    present(image);present(image);
    for(;;){screen->SubmitFlip(frame);screen->FrameWait(frame);screen->FrameBufferSwap();++frame;}
}
static void fail(const char *step,int result)
{
    note("FAILED %s - %08X",step,(unsigned)result);notify(step);
    if(screen)hold(false);
    for(;;)sceKernelUsleep(1000000);
}

static bool hasExport(const char *library,const char *symbol)
{
    OrbisKernelModule modules[128];size_t count=0;
    if(sceKernelGetModuleList(modules,sizeof(modules),&count)!=0)return false;
    if(count>128)count=128;
    for(size_t i=0;i<count;++i)
    {
        OrbisKernelModuleInfo info={};info.size=sizeof(info);
        if(sceKernelGetModuleInfo(modules[i],&info)!=0 || !strstr(info.name,library))continue;
        void *address=nullptr;
        if(sceKernelDlsym(modules[i],symbol,&address)==0 && address)return true;
    }
    return false;
}
static void loadInternal(OrbisSysModuleInternal id,const char *library,const char *symbol)
{
    const int rc=(int)sceSysmoduleLoadModuleInternal(id);
    note("LOAD %s - %08X",library,(unsigned)rc);
    // Already-loaded status varies; export availability decides success.
    if(!hasExport(library,symbol))fail(symbol,rc);
}

// A single bounded, CPU-visible GPU allocation, retained until application exit.
static unsigned char *gpuMemory=nullptr;
static size_t gpuUsed=0;
static const size_t gpuCapacity=64*1024*1024;
static void *gpuAlloc(size_t bytes,size_t alignment)
{
    if(!alignment || alignment&(alignment-1))return nullptr;
    const size_t offset=(gpuUsed+alignment-1)&~(alignment-1);
    if(offset>gpuCapacity || bytes>gpuCapacity-offset)return nullptr;
    gpuUsed=offset+bytes;return gpuMemory+offset;
}
static PsbcShaderOutput compile(PsbcTarget target,PsbcStage stage,
                                const uint32_t *spirv,size_t size)
{
    PsbcCompileOptions options={target,stage,"main",true};
    PsbcShaderOutput output={};
    const PsbcResult rc=psbc_compile_shader(spirv,size,&options,&output);
    if(rc!=PSBC_RESULT_OK)fail("SHADER COMPILATION",rc);
    note("%s COMPILED - %u BYTES",stage==PSBC_STAGE_VERTEX?"VERTEX":"FRAGMENT",(unsigned)output.size);
    return output;
}
static GnmShaderCommonData *prepareShader(const PsbcShaderOutput &output,bool vertex)
{
    const size_t commonOffset=0x24+sizeof(GnmShaderFileHeader);
    if(!output.data || output.size<commonOffset+sizeof(GnmShaderCommonData)+8)fail("SHADER HEADER SIZE",-1);
    const auto bytes=(const unsigned char *)output.data;
    uint32_t magic=0;memcpy(&magic,bytes+0x24,4);
    if(magic!=GNM_SHADER_FILE_HEADER_ID)fail("SHADER MAGIC",-1);
    const auto common=(const GnmShaderCommonData *)(bytes+commonOffset);
    const auto addresses=(const uint32_t *)(common+1);
    const size_t codeOffset=commonOffset+addresses[0];
    const size_t codeSize=sceGnmShaderCommonCodeSize(common);
    const size_t headerSize=vertex?sceGnmVsShaderCalcSize((const GnmVsShader *)common):sceGnmPsShaderCalcSize((const GnmPsShader *)common);
    if((addresses[0]&3) || addresses[1]!=0xffffffff || headerSize>output.size-commonOffset ||
       codeOffset>output.size || codeSize>output.size-codeOffset)fail("SHADER BOUNDS",-1);
    void *code=gpuAlloc(codeSize,GNM_ALIGNMENT_SHADER_BYTES);
    auto header=(GnmShaderCommonData *)malloc(headerSize);
    if(!code || !header)fail("SHADER MEMORY",-1);
    memcpy(header,common,headerSize);memcpy(code,bytes+codeOffset,codeSize);
    if(vertex)sceGnmVsRegsSetAddress(&((GnmVsShader *)header)->registers,code);
    else sceGnmPsRegsSetAddress(&((GnmPsShader *)header)->registers,code);
    // Actual compiler output in Klog permits inspection when a draw is blank.
    printf("MILKDROP SHADER %s CODE BYTES %u\n",vertex?"VS":"PS",(unsigned)codeSize);
    for(unsigned i=0;i<codeSize/4;i+=4)
    {
        printf("MILKDROP ISA %s %04X",vertex?"VS":"PS",i*4);
        for(unsigned j=i;j<codeSize/4 && j<i+4;++j)printf(" %08X",((const uint32_t *)code)[j]);
        printf("\n");
    }
    return header;
}
static void contextRegister(GnmCommandBuffer *command,uint32_t address,uint32_t value)
{
    if(address<0x28000 || address>=0x29000 || command->endptr-command->cmdptr<3)fail("CONTEXT REGISTER RANGE",-1);
    *command->cmdptr++=PKT3(PKT3_SET_CONTEXT_REG,1,0);
    *command->cmdptr++=(address-0x28000)/4;
    *command->cmdptr++=value;
}
#include "glsl_text_compiler.h"
#include "shader_text.h"
#include "resource_shader.h"
#include "preset_equations.hpp"
#include "geometry_preset.hpp"
#include "legacy_shake.hpp"
#include "geometry_renderer.hpp"
#include "native_audio.hpp"
#include "spatial_warp.hpp"
#include "community_bank.hpp"
#include "native_composite.hpp"
#include "native_present.hpp"
#include "feedback_shader.h"
int main()
{
    setvbuf(stdout,nullptr,_IONBF,0);
    file=fopen("/data/hypnotic-milkdrop-native.log","w");
    notify("STARTING NATIVE GPU TEST");
    loadInternal(ORBIS_SYSMODULE_INTERNAL_VIDEO_OUT,"libSceVideoOut","sceVideoOutOpen");
    loadInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE,"libSceSystemService","sceSystemServiceHideSplashScreen");
    loadInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE,"libSceUserService","sceUserServiceGetInitialUser");
    loadInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT,"libSceAudioOut","sceAudioOutOpen");
    loadInternal(ORBIS_SYSMODULE_INTERNAL_PAD,"libScePad","scePadInit");
    screen=new Scene2D(1920,1080,4);
    if(!screen->Init(0x4000000,2)){screen=nullptr;fail("DISPLAY INIT",-1);}
    screen->SetActiveFrameBuffer(0);
    sceSystemServiceHideSplashScreen();
    // Keep startup module diagnostics in the log; reserve screen space for draw results.
    rowCount=0;
    note("MILKDROP TEST 0.50 - COMMUNITY PRESET TEST - HYPN90001");
    note("SEPARATE TEST - NO PIGLET OR SYSTEM MODULE PATCHES");
    present(false);
    // Resolve the firmware GPU entry points before using the OpenGNM wrappers.
    const char *sandbox=sceKernelGetFsSandboxRandomWord();
    if(!sandbox)fail("SANDBOX PATH",-1);
    char modulePath[256];
    snprintf(modulePath,sizeof(modulePath),"/%s/common/lib/libSceGnmDriver.sprx",sandbox);
    int moduleResult=0;
    const int module=(int)sceKernelLoadStartModule(modulePath,0,nullptr,0,nullptr,&moduleResult);
    note("GNM DRIVER LOAD %08X START %08X",(unsigned)module,(unsigned)moduleResult);
    if(!hasExport("libSceGnmDriver","sceGnmSubmitCommandBuffers"))fail("GNM DRIVER EXPORT",module<0?module:moduleResult);
    // This detail remains in the file/Klog, avoiding an extra on-screen row.
    --rowCount;
    off_t memoryOffset=0;
    int rc=sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),gpuCapacity,0x200000,ORBIS_KERNEL_WC_GARLIC,&memoryOffset);
    if(rc)fail("GPU ALLOCATION",rc);
    void *mapped=nullptr;
    rc=sceKernelMapDirectMemory(&mapped,gpuCapacity,ORBIS_KERNEL_PROT_CPU_READ|ORBIS_KERNEL_PROT_CPU_RW|
                              ORBIS_KERNEL_PROT_GPU_READ|ORBIS_KERNEL_PROT_GPU_WRITE,0,memoryOffset,0x200000);
    if(rc)fail("GPU MAPPING",rc);
    gpuMemory=(unsigned char *)mapped;
    const GnmGpuMode mode=sceGnmGpuMode();
    note("GPU MODE %s",mode==GNM_GPU_NEO?"PRO":"BASE COMPATIBILITY");
    notify("COMPILING NATIVE TRIANGLE SHADERS");
    const PsbcTarget target=mode==GNM_GPU_NEO?PSBC_TARGET_PS4_NEO:PSBC_TARGET_PS4_BASE;
    notify("INITIALIZING GLSL TEXT COMPILER");
    if(!glslang_initialize_process())fail("GLSL INITIALIZATION",-1);
    auto vertexOutput=compileText(target,PSBC_STAGE_VERTEX,text_vert);
    auto fragmentOutput=compileText(target,PSBC_STAGE_FRAGMENT,text_frag);
    const PsbcResourceType resources[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER};
    PsbcCompileOptions resourceOptions={target,PSBC_STAGE_FRAGMENT,"main",true};
    resourceOptions.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);
    resourceOptions.resource_count=2;resourceOptions.resources=resources;
    auto resourceOutput=compileText(target,PSBC_STAGE_FRAGMENT,feedback_frag,&resourceOptions);
    auto resourcePS=(GnmPsShader *)prepareShader(resourceOutput,false);
    psbc_free_output(&resourceOutput);
    createGeometryShaders(target);
    createWarpShader(target);
    createCompositeShaders(target);
    createTexturedShaders(target);
    createPresentationShader(target);
    // Keep glslang alive: blur shaders compile later in runFeedback(), and
    // imported preset shaders compile on demand throughout the selector loop.
    // Finalizing here invalidates its built-in symbol tables and pool allocator.
    // Process teardown reclaims the compiler together with the persistent renderer.
    auto solidOutput=compile(target,PSBC_STAGE_FRAGMENT,solid_fragment,sizeof(solid_fragment));
    auto vs=(GnmVsShader *)prepareShader(vertexOutput,true);
    auto ps=(GnmPsShader *)prepareShader(fragmentOutput,false);
    auto solidPS=(GnmPsShader *)prepareShader(solidOutput,false);
    // Follow the upstream native sample's interpolation setup.
    ps->registers.spibaryccntl=0;
    psbc_free_output(&vertexOutput);psbc_free_output(&fragmentOutput);
    psbc_free_output(&solidOutput);
    printf("MILKDROP VS EXPORTS %u RSRC %08X %08X POS %08X OUT %08X\n",vs->numexportsemantics,vs->registers.spishaderpgmrsrc1vs,vs->registers.spishaderpgmrsrc2vs,vs->registers.spishaderposformat,vs->registers.spivsoutconfig);
    printf("MILKDROP PS INPUTS %u ENA %08X ADDR %08X COLOR %08X MASK %08X\n",ps->numinputsemantics,ps->registers.spipsinputena,ps->registers.spipsinputaddr,ps->registers.spishadercolformat,ps->registers.cbshadermask);
    GnmRenderTarget targetRT={};GnmRenderTargetCreateInfo info={};
    info.colorfmt=GNM_FMT_R8G8B8A8_UNORM;
    info.width=width;info.height=height;info.numslices=1;
    info.numsamples=1;info.numfragments=1;
    info.colortilemodehint=GNM_TM_DISPLAY_LINEAR_ALIGNED;info.mingpumode=mode;
    GnmError ge=sceGnmCreateRenderTarget(&targetRT,&info);
    if(ge!=GNM_ERROR_OK)fail("RENDER TARGET",ge);
    uint64_t targetBytes=0;uint32_t targetAlign=0;
    ge=sceGnmRtCalcByteSize(&targetBytes,&targetAlign,&targetRT);
    if(ge!=GNM_ERROR_OK)fail("TARGET SIZE",ge);
    pixels=(unsigned char *)gpuAlloc(targetBytes,targetAlign);
    if(!pixels)fail("TARGET ALLOCATION",-1);
    memset(pixels,0,targetBytes);pixelPitch=sceGnmRtGetPitch(&targetRT);
    sceGnmRtSetBaseAddr(&targetRT,pixels);
    note("OFFSCREEN TARGET %u BY %u - PITCH %u",width,height,pixelPitch);
    present(false);
    const size_t commandBytes=0x10000;
    void *commandMemory=gpuAlloc(commandBytes,256);
    auto label=(volatile uint64_t *)gpuAlloc(8,8);
    if(!commandMemory || !label)fail("COMMAND MEMORY",-1);
    for(unsigned pass=0;pass<3;++pass)
    {
    *label=0;memset(pixels,0,targetBytes);
    GnmCommandBuffer command=sceGnmCmdInit(commandMemory,commandBytes,nullptr,nullptr);
    sceGnmDrawCmdInitDefaultHardwareState(&command);
    // Make color output, both scissor levels, sample masks and depth state
    // explicit instead of inheriting whatever the firmware context contained.
    contextRegister(&command,R_028808_CB_COLOR_CONTROL,S_028808_MODE(V_028808_CB_NORMAL)|S_028808_ROP3(0xcc));
    contextRegister(&command,R_028204_PA_SC_WINDOW_SCISSOR_TL,0x80000000);
    contextRegister(&command,R_028208_PA_SC_WINDOW_SCISSOR_BR,width|(height<<16));
    contextRegister(&command,R_028240_PA_SC_GENERIC_SCISSOR_TL,0x80000000);
    contextRegister(&command,R_028244_PA_SC_GENERIC_SCISSOR_BR,width|(height<<16));
    contextRegister(&command,R_028C38_PA_SC_AA_MASK_X0Y0_X1Y0,0xffffffff);
    contextRegister(&command,R_028C3C_PA_SC_AA_MASK_X0Y1_X1Y1,0xffffffff);
    GnmDbRenderControl db={};sceGnmDrawCmdSetDbRenderControl(&command,&db);
    GnmPrimitiveSetup primitive={};primitive.cullmode=GNM_CULL_NONE;
    primitive.frontface=GNM_FACE_CCW;primitive.frontmode=primitive.backmode=GNM_FILL_SOLID;
    primitive.provokemode=GNM_PROVOKINGVTX_FIRST;
    sceGnmDrawCmdSetPrimitiveSetup(&command,&primitive);
    GnmDepthStencilControl depth={};sceGnmDrawCmdSetDepthStencilControl(&command,&depth);
    sceGnmDrawCmdSetRenderTarget(&command,0,&targetRT);
    sceGnmDrawCmdSetRenderTargetMask(&command,0xf);
    GnmSetViewportInfo viewport={};viewport.dmin=0;viewport.dmax=1;
    viewport.scale[0]=width/2.0f;viewport.scale[1]=-(float)height/2.0f;viewport.scale[2]=0.5f;
    viewport.offset[0]=width/2.0f;viewport.offset[1]=height/2.0f;viewport.offset[2]=0.5f;
    sceGnmDrawCmdSetViewport(&command,0,&viewport);
    GnmViewportTransformControl transform={};transform.scalex=transform.offsetx=transform.scaley=transform.offsety=transform.scalez=transform.offsetz=transform.invertw=true;
    sceGnmDrawCmdSetViewportTransformControl(&command,&transform);
    sceGnmDrawCmdSetScreenScissor(&command,0,0,width,height);
    sceGnmDrawCmdSetHwScreenOffset(&command,0,0);
    sceGnmDrawCmdSetGuardBands(&command,1,1,1,1);
    if(pass==0)sceGnmDrawCmdSetEmbeddedVsShader(&command,GNM_EMBEDDED_VSH_FULLSCREEN,0);
    else sceGnmDrawCmdSetVsShader(&command,&vs->registers,0);
    sceGnmDrawCmdSetPsShader(&command,pass==2?&ps->registers:&solidPS->registers);
    if(pass==2)sceGnmDrawCmdSetPsInputUsage(&command,sceGnmVsShaderExportSemanticTable(vs),vs->numexportsemantics,
                                sceGnmPsShaderInputSemanticTable(ps),ps->numinputsemantics);
    sceGnmDrawCmdSetNumInstances(&command,1);
    sceGnmDrawCmdSetPrimitiveType(&command,pass==0?GNM_PT_RECTLIST:GNM_PT_TRILIST);
    sceGnmDrawCmdDrawIndexAuto(&command,3);
    const uint64_t expected=0x4859504e54524933ULL;
    sceGnmDrawCmdEventWriteEop(&command,GNM_CACHE_FLUSH_AND_INV_TS_EVENT,(uint64_t)(uintptr_t)label,GNM_DATA_SEL_SEND_DATA64,expected);
    const uint32_t dwords=(uint32_t)(command.cmdptr-command.beginptr);
    if(dwords>commandBytes/4)fail("COMMAND OVERFLOW",-1);
    void *buffers[]={command.beginptr};uint32_t sizes[]={dwords*4};
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    char passName[96];snprintf(passName,sizeof(passName),"GPU PASS %u - %s",pass,pass==0?"EMBEDDED VS SOLID PS":pass==1?"COMPILED VS SOLID PS":"COMPILED VS GRADIENT PS");
    notify(passName);
    rc=sceGnmSubmitCommandBuffers(1,buffers,sizes,nullptr,nullptr);
    if(rc<0)fail("GPU SUBMIT",rc);
    rc=sceGnmSubmitDone();if(rc<0)fail("SUBMIT DONE",rc);
    const uint64_t started=sceKernelGetProcessTime();
    while(*label!=expected && sceKernelGetProcessTime()-started<3000000)sceKernelUsleep(1000);
    if(*label!=expected)fail("GPU COMPLETION TIMEOUT",-1);
    unsigned changed=0;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)
    {
        const auto p=pixels+(y*pixelPitch+x)*4;
        if(p[0]>8 || p[1]>8 || p[2]>8)++changed;
    }
    const auto center=pixels+((height/2)*pixelPitch+width/2)*4;
    const bool verified=changed>1000 && changed<width*height/2 &&
        center[0]>80 && center[0]<190 && center[1]>20 && center[1]<125 &&
        center[2]>20 && center[2]<125 && pixels[0]<8 && pixels[1]<8 && pixels[2]<8;
    note("PASS %u - PIXELS %u - CENTER %u %u %u",pass,changed,center[0],center[1],center[2]);
    if(pass==2)note("RGB TRIANGLE %s",verified?"PASS":"FAIL - CHECK IMAGE AND KLOG");
    }
    runFeedback(resourcePS,mode);
}
