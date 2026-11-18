#pragma once
#include "texture_image.hpp"
#include "milkdrop_noise.hpp"
struct ImageGpu {GnmTexture texture={};unsigned char *data=nullptr;unsigned width=0,height=0,pitch=0;};
static ImageGpu imageTextures[2],noiseTexture,extraNoise[5];
static unsigned char *texturedTables[2][2]={};
static float *texturedUniforms=nullptr;
static GnmPsShader *brainWarpPS=nullptr,*brainCompositePS=nullptr,*textureCheckPS=nullptr;
static unsigned imageChoice=0;static uint32_t textureRandom=0x4859504e;
static uint32_t nextTextureRandom(){textureRandom^=textureRandom<<13;textureRandom^=textureRandom>>17;textureRandom^=textureRandom<<5;return textureRandom;}
static void uploadImage(GnmGpuMode mode,ImageGpu &image,unsigned w,unsigned h,const unsigned char *rgba){
    GnmTextureCreateInfo info={};info.format=GNM_FMT_R8G8B8A8_UNORM;info.texturetype=GNM_TEXTURE_2D;
    info.width=w;info.height=h;info.depth=1;info.nummiplevels=info.numslices=info.numfragments=1;
    info.tilemodehint=GNM_TM_DISPLAY_LINEAR_ALIGNED;info.mingpumode=mode;
    if(sceGnmCreateTexture(&image.texture,&info)!=GNM_ERROR_OK)fail("IMAGE GPU VIEW",-1);
    uint64_t bytes=0;uint32_t alignment=0;
    if(sceGnmTexCalcByteSize(&bytes,&alignment,&image.texture)!=GNM_ERROR_OK)fail("IMAGE GPU SIZE",-1);
    image.pitch=sceGnmTexGetPitch(&image.texture);image.width=w;image.height=h;
    if(image.pitch<w || (uint64_t)image.pitch*h*4>bytes)fail("IMAGE GPU PITCH",-1);
    image.data=(unsigned char *)gpuAlloc((size_t)bytes,alignment);
    if(!image.data)fail("IMAGE GPU MEMORY",-1);memset(image.data,0,(size_t)bytes);
    for(unsigned y=0;y<h;++y)memcpy(image.data+y*image.pitch*4,rgba+y*w*4,w*4);
    sceGnmTexSetBaseAddress(&image.texture,image.data);
}
static void uploadNoiseVolume(GnmGpuMode mode,ImageGpu &image,const std::vector<uint32_t> &rgba){
    GnmTextureCreateInfo info={};info.format=GNM_FMT_R8G8B8A8_UNORM;info.texturetype=GNM_TEXTURE_3D;
    info.width=info.height=info.depth=32;info.nummiplevels=info.numslices=info.numfragments=1;info.tilemodehint=GNM_TM_DISPLAY_LINEAR_ALIGNED;info.mingpumode=mode;
    if(sceGnmCreateTexture(&image.texture,&info)!=GNM_ERROR_OK)fail("NOISE VOLUME VIEW",-1);
    uint64_t bytes=0;uint32_t alignment=0;if(sceGnmTexCalcByteSize(&bytes,&alignment,&image.texture)!=GNM_ERROR_OK)fail("NOISE VOLUME SIZE",-1);
    image.pitch=sceGnmTexGetPitch(&image.texture);image.width=image.height=32;
    if((uint64_t)image.pitch*32*32*4>bytes)fail("NOISE VOLUME PITCH",-1);
    image.data=(unsigned char *)gpuAlloc(bytes,alignment);if(!image.data)fail("NOISE VOLUME MEMORY",-1);memset(image.data,0,bytes);
    for(unsigned z=0;z<32;++z)for(unsigned y=0;y<32;++y)memcpy(image.data+(z*32+y)*image.pitch*4,rgba.data()+(z*32+y)*32,32*4);
    sceGnmTexSetBaseAddress(&image.texture,image.data);
}
static void createImageTextures(GnmGpuMode mode){
    textureRandom^=(uint32_t)sceKernelGetProcessTime();if(!textureRandom)textureRandom=0x4859504e;
    const char *paths[]={"/app0/assets/images/texture-aurora.png","/app0/assets/images/texture-copper.png"};
    for(unsigned i=0;i<2;++i){TextureImage decoded;if(!loadTextureImage(paths[i],decoded)){rowCount=0;note("TEXTURE ERROR %s",decoded.error.c_str());fail(paths[i],-1);}
        uploadImage(mode,imageTextures[i],decoded.width,decoded.height,decoded.rgba);freeTextureImage(decoded);printf("MILKDROP IMAGE FILE LOADED %s %u x %u\n",paths[i],imageTextures[i].width,imageTextures[i].height);}
    // Same low-quality 256x256 RGBA packed-value distribution/row shuffle as
    // projectM MilkdropNoise::generate2D(size=256, zoomFactor=1), with local PRNG.
    auto noise=new unsigned char[256*256*4];
    for(unsigned y=0;y<256;++y){for(unsigned x=0;x<256;++x){uint32_t value=0;for(unsigned c=0;c<4;++c)value|=((nextTextureRandom()%256)+128)<<(c*8);memcpy(noise+(y*256+x)*4,&value,4);}
        for(unsigned x=0;x<256;++x){const unsigned a=nextTextureRandom()%256,b=nextTextureRandom()%256;for(unsigned c=0;c<4;++c)std::swap(noise[(y*256+a)*4+c],noise[(y*256+b)*4+c]);}}
    uploadImage(mode,noiseTexture,256,256,noise);delete[] noise;
    for(unsigned i=0;i<5;++i){
        if(i==2 || i==3){auto data=NativeMilkdropNoise::generate3D(32,i==2?1:4,nextTextureRandom());uploadNoiseVolume(mode,extraNoise[i],data);}
        else{unsigned size=i==4?32:256;auto data=NativeMilkdropNoise::generate2D(size,i==0?4:i==1?8:1,nextTextureRandom());uploadImage(mode,extraNoise[i],size,size,(const unsigned char *)data.data());}
    }
    texturedUniforms=(float *)gpuAlloc(64,16);if(!texturedUniforms)fail("IMAGE UNIFORMS",-1);memset(texturedUniforms,0,64);
    for(unsigned source=0;source<2;++source)for(unsigned pick=0;pick<2;++pick){
        auto table=(unsigned char *)gpuAlloc(256,64);if(!table)fail("IMAGE RESOURCE TABLE",-1);texturedTables[source][pick]=table;memset(table,0,256);
        GnmSampler sampler={};sampler.xymagfilter=sampler.xyminfilter=GNM_FILTER_BILINEAR;sampler.clampx=sampler.clampy=sampler.clampz=GNM_TEX_CLAMP_WRAP;
        const GnmTexture *textures[]={&feedback[source].texture,&noiseTexture.texture,&imageTextures[pick].texture};const unsigned offsets[]={0,128,192};
        for(unsigned i=0;i<3;++i){memcpy(table+offsets[i],textures[i],sizeof(GnmTexture));memcpy(table+offsets[i]+32,&sampler,sizeof(sampler));}
        GnmBuffer buffer={};sceGnmBufSetBaseAddress(&buffer,texturedUniforms);sceGnmBufSetFormat(&buffer,GNM_FMT_R32_FLOAT);sceGnmBufSetMemoryType(&buffer,GNM_MEMORY_READONLY,false,false);buffer.numrecords=64;buffer.stride=0;memcpy(table+64,&buffer,sizeof(buffer));
    }
}
static void createTexturedShaders(PsbcTarget target){
    libprojectM::MilkdropPreset::PresetFileParser file;if(!file.Read(std::string("/app0/assets/misc/community-brain-zoom.milk")))fail("BRAIN PRESET FILE",-1);
    const PsbcResourceType resources[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER};
    PsbcCompileOptions options={target,PSBC_STAGE_FRAGMENT,"main",true};options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);options.resource_count=4;options.resources=resources;
    for(unsigned i=0;i<3;++i){std::string glsl,error;
        const std::string body=i<2?file.GetCode(i==0?"warp_":"comp_"):"shader_body{ret=float3(tex2D(sampler_rand00,uv).r,tex2D(sampler_noise_lq,uv).g,rand_frame.x);}";
        if(!translateTextured(body,glsl,error,i==0)){note("TEXTURED HLSL ERROR: %s",error.c_str());fail("TEXTURED SHADER TRANSLATION",-1);}
        auto compiled=compileText(target,PSBC_STAGE_FRAGMENT,glsl.c_str(),&options);auto ps=(GnmPsShader *)prepareShader(compiled,false);ps->registers.spibaryccntl=0;psbc_free_output(&compiled);
        if(i==0)brainWarpPS=ps;else if(i==1)brainCompositePS=ps;else textureCheckPS=ps;
    }
}
static void setTextureUniforms(const float bands[3],bool fixed=false){
    texturedUniforms[0]=512;texturedUniforms[1]=288;texturedUniforms[2]=1.0f/512;texturedUniforms[3]=1.0f/288;
    for(unsigned i=0;i<3;++i)texturedUniforms[4+i]=bands[i];texturedUniforms[7]=(bands[0]+bands[1]+bands[2])/3;
    for(unsigned i=0;i<4;++i)texturedUniforms[8+i]=fixed?.25f:(nextTextureRandom()>>8)*(1.0f/16777216);
    texturedUniforms[12]=texturedUniforms[13]=256;texturedUniforms[14]=texturedUniforms[15]=1.0f/256;
}
static void texturedStep(unsigned source,unsigned destination,const PresetMesh &mesh,GnmPsShader *ps){
    if(source>1 || destination>1 || source==destination || imageChoice>1)fail("TEXTURED HISTORY ALIAS",-1);
    drawPresetGeometry(&feedback[destination].target,mesh,false,GNM_PT_TRILIST,false,ps,texturedTables[source][imageChoice]);pixels=feedback[destination].data;pixelPitch=feedback[destination].pitch;
}
static float sampleImage(const ImageGpu &im,float u,float v,unsigned c){
    float x=u*im.width-.5f,y=v*im.height-.5f;const int ix=(int)floorf(x),iy=(int)floorf(y);x-=ix;y-=iy;
    float values[4];for(unsigned j=0;j<2;++j)for(unsigned i=0;i<2;++i){const unsigned xx=(ix+(int)i+(int)im.width*16)%im.width,yy=(iy+(int)j+(int)im.height*16)%im.height;values[j*2+i]=im.data[(yy*im.pitch+xx)*4+c];}
    return (values[0]*(1-x)+values[1]*x)*(1-y)+(values[2]*(1-x)+values[3]*x)*y;
}
static void checkImageTextures(){
    const float bands[]={1,1,1};setTextureUniforms(bands,true);
    for(imageChoice=0;imageChoice<2;++imageChoice){texturedStep(0,1,compositeQuad,textureCheckPS);bool ok=true;
        for(unsigned yi=0;yi<2;++yi)for(unsigned xi=0;xi<4;++xi){const unsigned x=64+xi*128,y=yi?224:64;const auto p=pixels+(y*pixelPitch+x)*4;const float u=(x+.5f)/512,v=(y+.5f)/288;
            const float expected[]={sampleImage(imageTextures[imageChoice],u,v,0),sampleImage(noiseTexture,u,v,1),63.75f};
            for(unsigned c=0;c<3;++c){if(fabsf(p[c]-expected[c])>3)ok=false;printf("MILKDROP TEXTURE CHECK PICK %u X %u Y %u C %u GOT %u EXPECT %.2f\n",imageChoice,x,y,c,p[c],expected[c]);}}
        if(!ok){rowCount=0;note("FILE/NOISE/UNIFORM TEXTURE CHECK FAIL");hold(true);}
        texturedStep(0,1,compositeQuad,brainCompositePS);
        for(unsigned xi=0;xi<4;++xi){const unsigned x=64+xi*128,y=64;const auto p=pixels+(y*pixelPitch+x)*4;const float u=.4f+.6f*(32*(1+xi)/255.0f),v=.4f+.6f*(64/255.0f),gain=32*(4-xi)/255.0f*3;
            for(unsigned c=0;c<3;++c){const float expected=std::min(255.0f,sampleImage(imageTextures[imageChoice],u,v,c)*gain);if(fabsf(p[c]-expected)>4){rowCount=0;note("ORIGINAL TEXTURED COMPOSITE CHECK FAIL");hold(true);}}}
    }
    imageChoice=0;printf("MILKDROP PNG FILES + NOISE + UBO + ORIGINAL BRAIN COMPOSITE CHECKS PASS\n");
}
