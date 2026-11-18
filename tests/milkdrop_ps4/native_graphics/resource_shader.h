#pragma once
// Explicit set-0 ABI defined by this experiment, not a projectM shader yet.
static const char resource_frag[]=R"glsl(#version 450
layout(set=0,binding=0) uniform sampler2D testImage;
layout(set=0,binding=1,std140) uniform Parameters { vec4 tintAndOffset; } params;
layout(location=0) out vec4 outColor;
void main() {
    vec2 uv=vec2(fract(gl_FragCoord.x/512.0+params.tintAndOffset.w),0.5);
    outColor=vec4(textureLod(testImage,uv,0.0).rgb*params.tintAndOffset.rgb,1.0);
}
)glsl";

static void bindResourceTable(GnmCommandBuffer *command,const GnmPsShader *shader,void *table,bool verbose=true)
{
    // RADV passes a 32-bit low address in ONE SGPR; its compiled address32_hi
    // supplies the high bits. The generic two-SGPR GNM pointer helper is not
    // the ABI here and could overwrite an adjacent argument.
    // A constant fragment shader may have all descriptors optimized away.
    // Zero metadata slots means no user-data binding is required. Any shader
    // that does declare slots must still expose exactly one valid table below.
    if(shader->common.numinputusageslots==0)return;
    const auto slots=sceGnmPsShaderInputUsageSlotTable(shader);
    unsigned matches=0;
    for(unsigned i=0;i<shader->common.numinputusageslots;++i) {
        if(verbose)printf("MILKDROP RESOURCE SLOT %u TYPE %u SGPR %u\n",i,slots[i].usagetype,slots[i].startregister);
        if(slots[i].usagetype!=GNM_SHINPUTUSAGE_PTR_INDIRECTRESOURCETABLE)continue;
        if(slots[i].startregister>=16 || command->endptr-command->cmdptr<3)fail("RESOURCE SGPR RANGE",-1);
        *command->cmdptr++=PKT3(PKT3_SET_SH_REG,1,0);
        *command->cmdptr++=(R_00B030_SPI_SHADER_USER_DATA_PS_0-0xB000)/4+slots[i].startregister;
        *command->cmdptr++=(uint32_t)(uintptr_t)table;
        ++matches;
    }
    if(matches!=1)fail("RESOURCE TABLE SLOT COUNT",-1);
}

static void *makeResourceTable(GnmGpuMode mode,const float values[4])
{
    GnmTexture texture={};GnmTextureCreateInfo info={};
    info.format=GNM_FMT_R8G8B8A8_UNORM;info.texturetype=GNM_TEXTURE_2D;
    info.width=64;info.height=64;info.depth=1;info.nummiplevels=1;
    info.numslices=1;info.numfragments=1;
    info.tilemodehint=GNM_TM_DISPLAY_LINEAR_ALIGNED;info.mingpumode=mode;
    GnmError rc=sceGnmCreateTexture(&texture,&info);
    if(rc!=GNM_ERROR_OK)fail("TEXTURE CREATE",rc);
    uint64_t bytes=0;uint32_t alignment=0;
    rc=sceGnmTexCalcByteSize(&bytes,&alignment,&texture);
    if(rc!=GNM_ERROR_OK)fail("TEXTURE SIZE",rc);
    auto image=(unsigned char *)gpuAlloc(bytes,alignment);
    auto params=(float *)gpuAlloc(16,16);
    auto table=(unsigned char *)gpuAlloc(128,64);
    if(!image || !params || !table)fail("RESOURCE ALLOCATION",-1);
    if((uintptr_t)table>>32 != (uintptr_t)gpuMemory>>32)fail("RESOURCE ADDRESS WINDOW",-1);
    memset(image,0,bytes);memset(table,0,128);memcpy(params,values,16);
    const unsigned pitch=sceGnmTexGetPitch(&texture);
    if((uint64_t)pitch*64*4>bytes)fail("TEXTURE PITCH RANGE",-1);
    static const unsigned char colors[4][4]={{255,0,0,255},{0,255,0,255},{0,0,255,255},{255,255,255,255}};
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)
        memcpy(image+(y*pitch+x)*4,colors[x/16],4);
    sceGnmTexSetBaseAddress(&texture,image);
    GnmSampler sampler={};
    sampler.xymagfilter=sampler.xyminfilter=GNM_FILTER_POINT;
    // RADV buffer descriptors use byte addressing, not a vertex/constant
    // buffer's 16-byte record stride.
    GnmBuffer buffer={};sceGnmBufSetBaseAddress(&buffer,params);
    sceGnmBufSetFormat(&buffer,GNM_FMT_R32_FLOAT);
    sceGnmBufSetMemoryType(&buffer,GNM_MEMORY_READONLY,false,false);
    buffer.stride=0;buffer.numrecords=16;
    memcpy(table,&texture,sizeof(texture));memcpy(table+32,&sampler,sizeof(sampler));
    memcpy(table+64,&buffer,sizeof(buffer));
    printf("MILKDROP RESOURCES TABLE %p IMAGE %p PARAMS %p PITCH %u SIZE %u TINT %.2f %.2f %.2f SHIFT %.2f\n",
           table,image,params,pitch,(unsigned)bytes,values[0],values[1],values[2],values[3]);
    return table;
}

static bool verifyResourceImage(unsigned pass)
{
    static const unsigned char expected[2][4][3]={
        {{255,0,0},{0,255,0},{0,0,255},{255,255,255}},
        {{0,0,64},{128,255,64},{128,0,0},{0,255,0}}};
    bool success=true;
    for(unsigned i=0;i<4;++i) {
        const auto p=pixels+((height/2)*pixelPitch+64+i*128)*4;
        printf("MILKDROP RESOURCE PASS %u SAMPLE %u RGB %u %u %u\n",pass,i,p[0],p[1],p[2]);
        for(unsigned c=0;c<3;++c) {
            const int difference=(int)p[c]-expected[pass-3][i][c];
            if(difference < -3 || difference > 3)success=false;
        }
    }
    return success;
}
