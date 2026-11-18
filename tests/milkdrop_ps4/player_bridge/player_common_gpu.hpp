#pragma once
#include "../native_graphics/common_shader.hpp"
#include "../native_graphics/blur_config.hpp"
#include "../native_graphics/hue_field.hpp"
#include "../native_graphics/transform_limits.hpp"
#include "../native_graphics/engine_inputs.hpp"
static BlurConfig commonBlurConfig;
#include "../native_graphics/recoverable_shader.hpp"
#include "../native_graphics/common_blur_shaders.hpp"
struct CommonSurface {FeedbackSurface image={};unsigned w=0,h=0;};
static CommonSurface commonBlur[6];
static unsigned char *commonTables[2][2]={},*blurTables[6][2]={};
static float *commonInputs=nullptr,*blurInputs=nullptr;
static GnmPsShader *commonBlurPS[2]={};
static PsbcTarget commonTarget=PSBC_TARGET_PS4_BASE;
struct CommonProgram {uint64_t hash=0;GnmPsShader *warp=nullptr,*comp=nullptr;unsigned blurLevels=0;bool pointNoise[2]={};};
static CommonProgram commonPrograms[160];static unsigned commonProgramCount=0;
static float commonPresetRandom[4]={},commonHueOffsets[4]={};
static void writeTexture(unsigned char *table,unsigned binding,const GnmTexture &texture,bool clamp,bool point=false){
    GnmSampler sampler={};sampler.xymagfilter=sampler.xyminfilter=point?GNM_FILTER_POINT:GNM_FILTER_BILINEAR;sampler.zfilter=point?GNM_ZFILTER_POINT:GNM_ZFILTER_LINEAR;
    sampler.clampx=sampler.clampy=sampler.clampz=clamp?GNM_TEX_CLAMP_CLAMP_LAST_TEXEL:GNM_TEX_CLAMP_WRAP;
    memcpy(table+binding*64,&texture,sizeof(texture));memcpy(table+binding*64+32,&sampler,sizeof(sampler));
}
static void writeUniforms(unsigned char *table,unsigned binding,float *data,unsigned size){
    GnmBuffer buffer={};sceGnmBufSetBaseAddress(&buffer,data);sceGnmBufSetFormat(&buffer,GNM_FMT_R32_FLOAT);sceGnmBufSetMemoryType(&buffer,GNM_MEMORY_READONLY,false,false);buffer.numrecords=size;buffer.stride=0;memcpy(table+binding*64,&buffer,sizeof(buffer));
}
static void createCommonSurface(GnmGpuMode mode,CommonSurface &surface,unsigned w,unsigned h){
    surface.w=w;surface.h=h;auto &s=surface.image;
    GnmRenderTargetCreateInfo rt={};rt.colorfmt=GNM_FMT_R8G8B8A8_UNORM;rt.width=w;rt.height=h;rt.numslices=rt.numsamples=rt.numfragments=1;rt.colortilemodehint=GNM_TM_DISPLAY_LINEAR_ALIGNED;rt.mingpumode=mode;
    if(sceGnmCreateRenderTarget(&s.target,&rt)!=GNM_ERROR_OK)fail("BLUR TARGET",-1);
    uint64_t rb=0,tb=0;uint32_t ra=0,ta=0;if(sceGnmRtCalcByteSize(&rb,&ra,&s.target)!=GNM_ERROR_OK)fail("BLUR TARGET SIZE",-1);
    s.pitch=sceGnmRtGetPitch(&s.target);GnmTextureCreateInfo info={};info.format=GNM_FMT_R8G8B8A8_UNORM;info.texturetype=GNM_TEXTURE_2D;info.width=w;info.height=h;info.depth=1;info.pitch=s.pitch;info.nummiplevels=info.numslices=info.numfragments=1;info.tilemodehint=GNM_TM_DISPLAY_LINEAR_ALIGNED;info.mingpumode=mode;
    if(sceGnmCreateTexture(&s.texture,&info)!=GNM_ERROR_OK || sceGnmTexCalcByteSize(&tb,&ta,&s.texture)!=GNM_ERROR_OK || sceGnmTexGetPitch(&s.texture)!=s.pitch)fail("BLUR TEXTURE VIEW",-1);
    s.bytes=std::max(rb,tb);s.data=(unsigned char *)gpuAlloc(s.bytes,std::max(ra,ta));if(!s.data || (uint64_t)s.pitch*h*4>s.bytes)fail("BLUR MEMORY",-1);
    memset(s.data,0,s.bytes);sceGnmRtSetBaseAddr(&s.target,s.data);sceGnmTexSetBaseAddress(&s.texture,s.data);
}
static void createCommonRenderer(GnmGpuMode mode){
    commonTarget=mode==GNM_GPU_NEO?PSBC_TARGET_PS4_NEO:PSBC_TARGET_PS4_BASE;
    commonInputs=(float *)gpuAlloc(27*16,16);blurInputs=(float *)gpuAlloc(64,16);if(!commonInputs || !blurInputs)fail("COMMON UNIFORM MEMORY",-1);
    unsigned w=width,h=height;
    for(unsigned i=0;i<6;++i){if(!(i&1) || i<2){w=std::max(16u,w/2);h=std::max(16u,h/2);}createCommonSurface(mode,commonBlur[i],((w+3)/16)*16,((h+3)/4)*4);}
    for(unsigned source=0;source<2;++source)for(unsigned pick=0;pick<2;++pick){
        auto &table=commonTables[source][pick];table=(unsigned char *)gpuAlloc(16*64,64);if(!table)fail("COMMON TABLE MEMORY",-1);memset(table,0,16*64);
        writeTexture(table,0,feedback[source].texture,false);writeUniforms(table,1,commonInputs,27*16);
        for(unsigned level=0;level<3;++level)writeTexture(table,2+level,commonBlur[level*2+1].image.texture,true);
        writeTexture(table,5,noiseTexture.texture,false);writeTexture(table,6,imageTextures[pick].texture,false);
        for(unsigned alias=0;alias<4;++alias)writeTexture(table,7+alias,feedback[source].texture,(alias%2)==0,alias>=2);
        for(unsigned noise=0;noise<5;++noise)writeTexture(table,11+noise,extraNoise[noise].texture,false);
    }
    for(unsigned i=0;i<6;++i)for(unsigned source=0;source<2;++source){
        auto &table=blurTables[i][source];table=(unsigned char *)gpuAlloc(128,64);if(!table)fail("BLUR TABLE MEMORY",-1);memset(table,0,128);
        writeTexture(table,0,i?commonBlur[i-1].image.texture:feedback[source].texture,true);writeUniforms(table,1,blurInputs,64);
    }
    const PsbcResourceType bindings[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER};PsbcCompileOptions options={commonTarget,PSBC_STAGE_FRAGMENT,"main",true};options.resource_count=2;options.resources=bindings;options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);
    for(unsigned i=0;i<2;++i){auto compiled=compileText(commonTarget,PSBC_STAGE_FRAGMENT,i?common_blur_1:common_blur_0,&options);commonBlurPS[i]=(GnmPsShader *)prepareShader(compiled,false);commonBlurPS[i]->registers.spibaryccntl=0;psbc_free_output(&compiled);}
    printf("MILKDROP COMMON BLUR PIPELINE READY GPU ARENA %u/%u\n",(unsigned)gpuUsed,(unsigned)gpuCapacity);
}
static void activateCommon(){
    commonBlurConfig=BlurConfig{};
    for(auto &surface:commonBlur)memset(surface.image.data,0,surface.image.bytes);
    for(auto &v:commonPresetRandom)v=(nextTextureRandom()>>8)*(1.f/16777216);
    const unsigned modulus[]={64841,53751,42661,31571};for(unsigned i=0;i<4;++i)commonHueOffsets[i]=(nextTextureRandom()%modulus[i])*.01f;
}
static void setCommonInputs(const PresetEquations::Output &v){
    for(unsigned source=0;source<2;++source)for(unsigned pick=0;pick<2;++pick)writeTexture(commonTables[source][pick],0,feedback[source].texture,v.textureWrap<=.0001);
    if(!commonBlurConfig.set(v.blurMin,v.blurMax,v.blurEdge))fail("INVALID BLUR CONFIG",-1);
    memset(commonInputs,0,27*16);commonInputs[0]=width;commonInputs[1]=height;commonInputs[2]=1.f/width;commonInputs[3]=1.f/height;
    for(unsigned i=0;i<3;++i){commonInputs[4+i]=v.audio[i];commonInputs[8+i]=v.audio[i+3];}
    commonInputs[7]=(v.audio[0]+v.audio[1]+v.audio[2])/3;commonInputs[11]=(v.audio[3]+v.audio[4]+v.audio[5])/3;
    commonInputs[12]=v.seconds;commonInputs[13]=v.fps;commonInputs[14]=v.frame;commonInputs[15]=0;
    commonInputs[16]=1;commonInputs[17]=.5625f;commonInputs[18]=1;commonInputs[19]=1.f/.5625f;
    for(unsigned i=0;i<4;++i){commonInputs[20+i]=(nextTextureRandom()>>8)*(1.f/16777216);commonInputs[24+i]=commonPresetRandom[i];}
    for(unsigned i=0;i<32;++i)commonInputs[28+i]=v.q[i];
    shaderRoamingInputs((float)v.seconds,commonInputs+60);
    presetHueCorners(v.seconds,commonHueOffsets,commonInputs+92);
    commonInputs[76]=commonInputs[77]=256;commonInputs[78]=commonInputs[79]=1.f/256;
    commonInputs[84]=commonBlurConfig.max[0]-commonBlurConfig.min[0];commonInputs[85]=commonBlurConfig.min[0];commonInputs[86]=commonBlurConfig.max[1]-commonBlurConfig.min[1];commonInputs[87]=commonBlurConfig.min[1];commonInputs[88]=commonBlurConfig.max[2]-commonBlurConfig.min[2];commonInputs[89]=commonBlurConfig.min[2];commonInputs[90]=presetSurfaceDecay(v.decay);
    const auto &image=imageTextures[imageChoice];commonInputs[80]=image.width;commonInputs[81]=image.height;commonInputs[82]=1.f/image.width;commonInputs[83]=1.f/image.height;
}
static void updateCommonBlur(unsigned source,unsigned levels=3){
    if(source>1 || levels>3)fail("BLUR UPDATE RANGE",-1);
    const float weights[]={4,3.8f,3.5f,2.9f,1.9f,1.2f,.7f,.3f};
    for(unsigned i=0;i<levels*2;++i){
        auto &dst=commonBlur[i];const float w=i?commonBlur[i-1].w:width,h=i?commonBlur[i-1].h:height;
        memset(blurInputs,0,64);blurInputs[0]=w;blurInputs[1]=h;blurInputs[2]=1/w;blurInputs[3]=1/h;
        if(!(i&1)){
            float sum=0;for(unsigned j=0;j<4;++j){const float a=weights[j*2]+weights[j*2+1];blurInputs[4+j]=a;blurInputs[8+j]=j*2+2*weights[j*2+1]/a;sum+=a;}
            blurInputs[12]=commonBlurConfig.scale[i/2];blurInputs[13]=commonBlurConfig.bias[i/2];blurInputs[14]=.5f/sum;
        }else {
            const float a=weights[0]+weights[1]+weights[2]+weights[3],b=weights[4]+weights[5]+weights[6]+weights[7];
            blurInputs[4]=a;blurInputs[5]=b;blurInputs[6]=2*(weights[2]+weights[3])/a;blurInputs[7]=2+2*(weights[6]+weights[7])/b;
            blurInputs[8]=1/(2*(a+b));blurInputs[9]=i==1?1-commonBlurConfig.edge:1;blurInputs[10]=i==1?commonBlurConfig.edge:0;blurInputs[11]=5;
        }
        drawPresetGeometry(&dst.image.target,compositeQuad,false,GNM_PT_TRILIST,false,commonBlurPS[i&1],blurTables[i][source],false,dst.w,dst.h,100);
    }
}
static void drawCommon(unsigned source,unsigned destination,const PresetMesh &mesh,int program,bool composite){
    if(source>1 || destination>1 || source==destination || program<0 || (unsigned)program>=commonProgramCount)fail("COMMON DRAW RANGE",-1);
    auto table=commonTables[source][imageChoice];
    if(commonPrograms[program].pointNoise[composite?1:0])writeTexture(table,6,noiseTexture.texture,false,true);
    else writeTexture(table,6,imageTextures[imageChoice].texture,false);
    drawPresetGeometry(&feedback[destination].target,mesh,false,GNM_PT_TRILIST,false,composite?commonPrograms[program].comp:commonPrograms[program].warp,commonTables[source][imageChoice],false,width,height,100);pixels=feedback[destination].data;pixelPitch=feedback[destination].pitch;
}
static int prepareImportedShaders(const char *path,std::string &error){
    libprojectM::MilkdropPreset::PresetFileParser file;if(!file.Read(std::string(path))){error="Cannot read shader preset";return -1;}
    auto warp=file.GetCode("warp_"),comp=file.GetCode("comp_");completeShaderStages(warp,comp);uint64_t hash=14695981039346656037ULL;
    for(char c:warp+"\n---\n"+comp){hash^=(unsigned char)c;hash*=1099511628211ULL;}
    for(unsigned i=0;i<commonProgramCount;++i)if(commonPrograms[i].hash==hash)return (int)i;
    if(commonProgramCount>=160){error="Shader cache full (160 presets) - relaunch to clear";return -1;}
    const auto checkpoint=gpuUsed;CommonProgram result;result.hash=hash;
    // Inspect original bodies, not the translator's always-present helpers.
    // Comments can conservatively request an extra blur; they never skip one.
    const auto bodies=warp+"\n"+comp;
    for(unsigned level=1;level<=3;++level)
        if(bodies.find("GetBlur"+std::to_string(level))!=std::string::npos || bodies.find("sampler_blur"+std::to_string(level))!=std::string::npos)result.blurLevels=level;
    const PsbcResourceType bindings[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER};
    PsbcCompileOptions options={commonTarget,PSBC_STAGE_FRAGMENT,"main",true};options.resource_count=16;options.resources=bindings;options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);
    for(unsigned i=0;i<2;++i){std::string glsl;PsbcShaderOutput output={};
        const bool translated=translateCommonShader(i?comp:warp,i==0,glsl,error,&result.pointNoise[i]);
        printf("MILKDROP COMMON COMPILE %s STAGE %u TRANSLATED %u\n",path,i,translated);
        if(!translated || !tryCompileFragment(glsl.c_str(),options,output,error)){
            free(result.warp);free(result.comp);gpuUsed=checkpoint;printf("MILKDROP COMMON COMPILE REJECT %s\n",error.c_str());return -1;
        }
        auto ps=tryPreparePixel(output,error);psbc_free_output(&output);
        if(!ps){free(result.warp);free(result.comp);gpuUsed=checkpoint;return -1;}
        if(i)result.comp=ps;else result.warp=ps;
    }
    const unsigned index=commonProgramCount++;commonPrograms[index]=result;
    printf("MILKDROP COMMON PRESET COMPILED SLOT %u BLUR LEVELS %u GPU USED %u\n",index,result.blurLevels,(unsigned)gpuUsed);return (int)index;
}
static void checkCommonBlur(){
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;p[0]=48;p[1]=96;p[2]=144;p[3]=255;}
    updateCommonBlur(0);bool ok=true;
    for(unsigned i=0;i<6;++i){const auto &s=commonBlur[i];const auto p=s.image.data+((s.h/2)*s.image.pitch+s.w/2)*4;
        const bool pass=abs((int)p[0]-48)<=3 && abs((int)p[1]-96)<=3 && abs((int)p[2]-144)<=3;
        printf("MILKDROP BLUR CONTROL PASS %u SIZE %u %u RGB %u %u %u %s\n",i,s.w,s.h,p[0],p[1],p[2],pass?"PASS":"FAIL");ok&=pass;
    }
    if(!ok){rowCount=0;note("GPU BLUR CONTROL FAILED - SEND KLOG");hold(false);}
    // Non-default ranges exercise all three progressive normalization stages.
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;p[0]=p[1]=p[2]=128;p[3]=255;}
    const double lo[3]={.1,.2,.3},hi[3]={.9,.8,.7};commonBlurConfig.set(lo,hi,.5);updateCommonBlur(0);
    bool ranges=true;for(unsigned i=0;i<6;++i){const auto &b=commonBlur[i];const auto p=b.image.data+((b.h/2)*b.image.pitch+b.w/2)*4;if(abs((int)p[0]-128)>5)ranges=false;}
    const auto &first=commonBlur[1];const unsigned edge=first.image.data[0];
    // The first vertical pass applies edge darkening; the centre stays intact.
    const bool darkened=edge<120;
    printf("MILKDROP BLUR RANGES %s EDGE DARKEN %s EDGE %u\n",ranges?"PASS":"FAIL",darkened?"PASS":"FAIL",edge);
    if(!ranges || !darkened){rowCount=0;note("GPU BLUR RANGE CONTROL FAILED - SEND KLOG");hold(false);}
    activateCommon();printf("MILKDROP SIX PASS BLUR CONTROL PASS\n");
}
// Development-only differential check of the actual unchanged target. This
// does not implement or replace any preset shader; it diagnoses native output.
static bool readReferenceImage(FILE *file,CommonSurface &surface,std::vector<unsigned char> &bytes){
    uint32_t size[2];if(fread(size,4,2,file)!=2 || !size[0] || !size[1] || size[0]>512 || size[1]>288)return false;
    surface.w=size[0];surface.h=size[1];bytes.resize((size_t)size[0]*size[1]*4);
    return fread(bytes.data(),1,bytes.size(),file)==bytes.size();
}
static bool compareReference(const char *stage,const CommonSurface &reference,const std::vector<unsigned char> &bytes,const FeedbackSurface &actual){
    unsigned maximum=0,errors=0,atX=0,atY=0,atC=0;uint64_t total=0;
    for(unsigned y=0;y<reference.h;++y)for(unsigned x=0;x<reference.w;++x)for(unsigned c=0;c<3;++c){
        const unsigned want=bytes[(y*reference.w+x)*4+c],got=actual.data[(y*actual.pitch+x)*4+c];
        const unsigned d=abs((int)want-(int)got);total+=d;errors+=d>8;
        if(d>maximum){maximum=d;atX=x;atY=y;atC=c;}
    }
    const double mean=total/(reference.w*reference.h*3.0);const bool pass=maximum<=8 && mean<=1;
    printf("MILKDROP NUMERIC %s %s MAX %u MEAN %.5f ERRORS %u AT %u %u CHANNEL %u EXPECT %u GOT %u\n",stage,pass?"PASS":"FAIL",maximum,mean,errors,atX,atY,atC,bytes[(atY*reference.w+atX)*4+atC],actual.data[(atY*actual.pitch+atX)*4+atC]);return pass;
}
static void checkCommonReference(){
    FILE *file=fopen("/app0/assets/misc/spatialist-reference-0.28.bin","rb");
    if(!file){printf("MILKDROP NUMERIC REFERENCE FILE MISSING\n");return;}
    uint32_t header[2];if(fread(header,4,2,file)!=2 || header[0]!=0x48595246 || header[1]!=6){fclose(file);printf("MILKDROP NUMERIC REFERENCE HEADER INVALID\n");return;}
    std::string error;const int slot=prepareImportedShaders("/app0/assets/presets/20-cream-hypnotic-flexi-spatialist.milk",error);
    if(slot<0){fclose(file);printf("MILKDROP NUMERIC COMPILE REJECT %s\n",error.c_str());return;}
    CommonSurface ref;std::vector<unsigned char> data;bool ok=readReferenceImage(file,ref,data);
    if(ok && ref.w==width && ref.h==height){
        for(unsigned y=0;y<height;++y)memcpy(feedback[0].data+y*feedback[0].pitch*4,data.data()+y*width*4,width*4);
        activateCommon();updateCommonBlur(0,2);
        for(unsigned level=0;level<2 && ok;++level){
            ok=readReferenceImage(file,ref,data);if(ok){const auto &blur=commonBlur[level*2+1];
                if(ref.w!=blur.w || ref.h!=blur.h){ok=false;break;}
                const bool pass=compareReference(level?"BLUR2":"BLUR1",ref,data,blur.image);if(!pass)printf("MILKDROP NUMERIC BLUR DIFFERENCE LEVEL %u\n",level+1);
            }
        }
        PresetEquations::Output values={};setCommonInputs(values);unsigned source=0;
        for(unsigned step=0;step<3 && ok;++step){
            drawCommon(source,source^1,pipelineQuad,slot,false);updateCommonBlur(source,2);source^=1;
            ok=readReferenceImage(file,ref,data);if(ok && ref.w==width && ref.h==height){char label[32];snprintf(label,sizeof(label),"ORIGINAL WARP STEP %u",step+1);compareReference(label,ref,data,feedback[source]);}
            else ok=false;
        }
    }else ok=false;
    fclose(file);activateCommon();printf("MILKDROP NUMERIC REFERENCE READ %s\n",ok?"PASS":"FAIL");
}

static void checkSamplerVariants(){
 std::string error;int program=prepareImportedShaders("/app0/assets/misc/sampler-control-0.33.milk",error);
 if(program<0){rowCount=0;note("SAMPLER CONTROL COMPILE %s",error.c_str());hold(false);return;}
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;p[0]=x<width/2?255:0;p[1]=x<width/2?0:255;p[2]=0;p[3]=255;}
 PresetEquations::Output values={};setCommonInputs(values);drawCommon(0,1,pipelineQuad,program,false);bool ok=true;
 for(unsigned row=0;row<2;++row)for(unsigned col=0;col<4;++col){const auto p=feedback[1].data+(((row*2+1)*height/4)*feedback[1].pitch+(col*2+1)*width/8)*4;
  int red=0,green=255;if(col==1 && row==0){red=255;green=0;}if(row==1 && col<2)red=green=128;
  bool pass=abs((int)p[0]-red)<=3 && abs((int)p[1]-green)<=3 && p[2]<=3;ok&=pass;
  printf("MILKDROP SAMPLER CONTROL ROW %u ALIAS %u RGB %u %u %u %s\n",row,col,p[0],p[1],p[2],pass?"PASS":"FAIL");
 }
 for(unsigned wrap=0;wrap<2;++wrap){values.textureWrap=wrap;setCommonInputs(values);drawCommon(0,1,pipelineQuad,program,true);auto p=feedback[1].data+((height/2)*feedback[1].pitch+width/2)*4;bool pass=wrap?(p[0]>252 && p[1]<3):(p[1]>252 && p[0]<3);ok&=pass;printf("MILKDROP MAIN SAMPLER WRAP %u %s\n",wrap,pass?"PASS":"FAIL");}
 if(!ok){rowCount=0;note("GPU SAMPLER VARIANT CHECK FAILED - SEND KLOG");hold(false);}activateCommon();
}

static void checkNoiseResources(){
 std::string error;int slot=prepareImportedShaders("/app0/assets/misc/noise-control-0.40.milk",error);
 if(slot<0){note("NOISE RESOURCE COMPILE ERROR %s",error.c_str());hold(false);}
 bool ok=true;
 for(unsigned z:{0u,17u}){
  PresetEquations::Output values={};values.q[0]=(z+.5)/32;setCommonInputs(values);drawCommon(0,1,pipelineQuad,slot,false);
  for(unsigned i=0;i<5;++i){const auto &image=extraNoise[i];unsigned x=(unsigned)((i+.5)*width/5);const auto p=feedback[1].data+((height/2)*feedback[1].pitch+x)*4;
   const auto expected=image.data+(((i==2 || i==3)?z*32:0)+5)*image.pitch*4+3*4;bool pass=true;
   for(unsigned c=0;c<3;++c)pass&=abs((int)p[c]-(int)expected[c])<=2;ok&=pass;
   printf("MILKDROP NOISE RESOURCE %u SLICE %u RGB %u %u %u EXPECT %u %u %u %s\n",i,z,p[0],p[1],p[2],expected[0],expected[1],expected[2],pass?"PASS":"FAIL");
  }
 }
 if(!ok){note("NOISE RESOURCE GPU CHECK FAIL - SEND KLOG");hold(true);}
}

static void checkShaderCompatibility(){
 std::string error;int slot=prepareImportedShaders("/app0/assets/misc/shader-compat-control-0.43.milk",error);
 if(slot<0){note("SHADER COMPATIBILITY CONTROL COMPILE %s",error.c_str());hold(false);return;}
 bool ok=true;activateCommon();
 for(unsigned step=0;step<2;++step){PresetEquations::Output values={};values.seconds=step*9;setCommonInputs(values);
  drawCommon(0,1,pipelineQuad,slot,false);drawCommon(1,0,pipelineQuad,slot,true);
  for(unsigned y:{height/4,3*height/4})for(unsigned x:{width/4,3*width/4}){
   float u=(x+.5f)/width,v=1-(y+.5f)/height;auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;bool pass=true;
   // Warp quantization is applied before the presentation shader.
   const unsigned base[]={51,102,153};unsigned expected[3];
   for(unsigned c=0;c<3;++c){float hue=commonInputs[92+c]*u*v+commonInputs[96+c]*(1-u)*v+commonInputs[100+c]*u*(1-v)+commonInputs[104+c]*(1-u)*(1-v);expected[c]=(unsigned)(base[c]*hue+.5f);pass&=abs((int)p[c]-(int)expected[c])<=2;}
   ok&=pass;printf("MILKDROP SHADER COMPAT TIME %u XY %u %u RGB %u %u %u EXPECT %u %u %u %s\n",step,x,y,p[0],p[1],p[2],expected[0],expected[1],expected[2],pass?"PASS":"FAIL");
  }
 }
 if(!ok){note("GPU HUE AND MACRO CONTROL FAILED - SEND KLOG");hold(false);}activateCommon();
}

static void checkLoopNoiseCompatibility(){
 std::string error;int slot=prepareImportedShaders("/app0/assets/misc/loop-noise-control-0.47.milk",error);
 if(slot<0){note("LOOP NOISE CONTROL COMPILE %s",error.c_str());hold(false);return;}
 PresetEquations::Output values={};setCommonInputs(values);
 drawCommon(0,1,pipelineQuad,slot,false);drawCommon(1,0,pipelineQuad,slot,true);
 auto p=feedback[0].data+((height/2)*feedback[0].pitch+width/2)*4;
 auto noise=noiseTexture.data+(5*noiseTexture.pitch+3)*4;
 unsigned expected[]={41u,(unsigned)std::min(255,(int)noise[1]+10),74u};bool ok=true;
 for(unsigned c=0;c<3;++c)ok&=abs((int)p[c]-(int)expected[c])<=2;
 printf("MILKDROP BOUNDED LOOP POINT NOISE RGB %u %u %u EXPECT %u %u %u %s\n",p[0],p[1],p[2],expected[0],expected[1],expected[2],ok?"PASS":"FAIL");
 // Slot 6 must return to the file image after a point-noise stage.
 int imageSlot=prepareImportedShaders("/app0/assets/misc/file-slot-control-0.47.milk",error);
 if(imageSlot<0){note("FILE SLOT CONTROL COMPILE %s",error.c_str());hold(false);return;}
 drawCommon(0,1,pipelineQuad,imageSlot,false);
 auto imagePixel=feedback[1].data+((height/2)*feedback[1].pitch+width/2)*4;bool restored=true;
 for(unsigned c=0;c<3;++c)restored&=fabsf(imagePixel[c]-sampleImage(imageTextures[imageChoice],.37f,.63f,c))<=3;
 printf("MILKDROP NOISE FILE SLOT RESTORE %s\n",restored?"PASS":"FAIL");ok&=restored;
 if(!ok){note("GPU LOOP / POINT NOISE CONTROL FAILED - SEND KLOG");hold(false);}activateCommon();
}

static void checkConditionalLoops(){
 std::string error;int slot=prepareImportedShaders("/app0/assets/misc/conditional-loop-control-0.49.milk",error);
 if(slot<0){note("CONDITIONAL LOOP CONTROL %s",error.c_str());hold(false);return;}
 PresetEquations::Output values={};setCommonInputs(values);drawCommon(0,1,pipelineQuad,slot,false);drawCommon(1,0,pipelineQuad,slot,true);
 auto p=feedback[0].data+((height/2)*feedback[0].pitch+width/2)*4;
 bool ok=abs((int)p[0]-13)<=2 && abs((int)p[1]-18)<=2 && abs((int)p[2]-64)<=2;
 printf("MILKDROP CONDITIONAL LOOP CONTROL RGB %u %u %u EXPECT 13 18 64 %s\n",p[0],p[1],p[2],ok?"PASS":"FAIL");
 if(!ok){note("GPU CONDITIONAL LOOP CONTROL FAILED - SEND KLOG");hold(false);}activateCommon();
}
