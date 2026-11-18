#pragma once
#include "hlsl_adapter.hpp"
static GnmPsShader *compositePrograms[2]={};
static PresetMesh compositeQuad,pipelineQuad;
static GnmPsShader *pipelineWarpPS=nullptr,*pipelineCompositePS=nullptr;
static unsigned compositeStage=0;
static const char *compositeNames[]={"OFF","GOLD","ROYAL"};
static void createCompositeShaders(PsbcTarget target){
    const char *paths[]={"/app0/assets/misc/shader-stage-gold.milk","/app0/assets/misc/shader-stage-royal.milk"};
    const PsbcResourceType binding[]={PSBC_RESOURCE_IMAGE_SAMPLER};
    PsbcCompileOptions options={target,PSBC_STAGE_FRAGMENT,"main",true};options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);
    options.resource_count=1;options.resources=binding;
    for(unsigned i=0;i<2;++i){
        libprojectM::MilkdropPreset::PresetFileParser file;
        if(!file.Read(std::string(paths[i])))fail("HLSL PRESET FILE",-1);
        std::string translated,error;
        if(!translateComposite(file.GetCode("comp_"),translated,error)){note("HLSL ERROR: %s",error.c_str());fail("COMMUNITY HLSL TRANSLATION",-1);}
        printf("MILKDROP HLSL TRANSLATED %s TO %u GLSL BYTES\n",compositeNames[i+1],(unsigned)translated.size());
        auto output=compileText(target,PSBC_STAGE_FRAGMENT,translated.c_str(),&options);
        compositePrograms[i]=(GnmPsShader *)prepareShader(output,false);compositePrograms[i]->registers.spibaryccntl=0;psbc_free_output(&output);
    }
    const float uv00[]={0,0,0,1},uv10[]={1,0,0,1},uv01[]={0,1,0,1},uv11[]={1,1,0,1};
    compositeQuad.add(-1,1,uv00);compositeQuad.add(1,1,uv10);compositeQuad.add(-1,-1,uv01);
    compositeQuad.add(1,1,uv10);compositeQuad.add(-1,-1,uv01);compositeQuad.add(1,-1,uv11);
    libprojectM::MilkdropPreset::PresetFileParser full;
    if(!full.Read(std::string("/app0/assets/misc/community-cauldron.milk")))fail("FULL PRESET FILE",-1);
    const PsbcResourceType pipelineBindings[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER};
    for(unsigned i=0;i<2;++i){
        std::string translated,error;
        const bool ok=i==0?translateWarp(full.GetCode("warp_"),translated,error):translateComposite(full.GetCode("comp_"),translated,error);
        if(!ok){note("FULL PRESET HLSL ERROR: %s",error.c_str());fail("CAULDRON TRANSLATION",-1);}
        options.resource_count=i==0?2:1;options.resources=pipelineBindings;
        auto output=compileText(target,PSBC_STAGE_FRAGMENT,translated.c_str(),&options);
        auto program=(GnmPsShader *)prepareShader(output,false);program->registers.spibaryccntl=0;psbc_free_output(&output);
        if(i==0)pipelineWarpPS=program;else pipelineCompositePS=program;
        printf("MILKDROP FULL CAULDRON %s SHADER READY\n",i==0?"WARP":"COMPOSITE");
    }
    const float xy00[]={0,0,0,0},xy10[]={1,0,1,0},xy01[]={0,1,0,1},xy11[]={1,1,1,1};
    pipelineQuad.add(-1,1,xy00);pipelineQuad.add(1,1,xy10);pipelineQuad.add(-1,-1,xy01);
    pipelineQuad.add(1,1,xy10);pipelineQuad.add(-1,-1,xy01);pipelineQuad.add(1,-1,xy11);

}
