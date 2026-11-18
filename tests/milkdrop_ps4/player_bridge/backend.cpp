#include <stdexcept>
// HYPNOTIC separate native GPU experiment. Renderer/command setup follows
// OpenGNM/freegnm-examples (MIT). Compiler: OpenGNM PSBC / Mesa NIR+ACO.
// Display is CPU Scene2D; generated shaders execute into a separate GPU target.
#include "../native_graphics/graphics.h"
#include "../native_graphics/status_text.h"
#include "../native_graphics/log.h"
#include "../native_graphics/spirv_fixtures.h"
#include "../native_graphics/solid_fragment.h"
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

extern std::stringstream debugLogStream;
static char rows[17][100];
static unsigned rowCount=0;
static FILE *file=nullptr;
static Scene2D *screen=nullptr;
static unsigned char *pixels=nullptr;
static unsigned pixelPitch=0;
static int frame=0;
static unsigned width=512,height=288;

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
static void hold(bool){throw std::runtime_error("MilkDrop startup self-check failed; see Klog");}

static void fail(const char *step,int result){char text[256];snprintf(text,sizeof(text),"%s (%08X)",step,(unsigned)result);throw std::runtime_error(text);}

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
#include "../native_graphics/glsl_text_compiler.h"
#include "../native_graphics/shader_text.h"
#include "../native_graphics/resource_shader.h"
#include "../native_graphics/preset_equations.hpp"
#include "../native_graphics/geometry_preset.hpp"
#include "../native_graphics/legacy_shake.hpp"
#include "../native_graphics/geometry_renderer.hpp"
#include "../native_graphics/native_audio.hpp"
#include "player_spatial_warp.hpp"
using SpatialWarp=PlayerSpatialWarp;
#include "../native_graphics/community_bank.hpp"
#include "../native_graphics/native_composite.hpp"
#include "../native_graphics/native_present.hpp"
#include "player_feedback_shader.h"
#include "player_backend_impl.hpp"
