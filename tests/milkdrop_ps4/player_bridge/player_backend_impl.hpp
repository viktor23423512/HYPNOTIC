#include "../../../graphics/milkdrop_player.h"
#include "../../../graphics/milkdrop_file_io.h"
static CommunityBank *playerBank=nullptr;
static PresetLayers *playerLayers=nullptr;
static GnmPsShader *playerFeedbackPS=nullptr;
static AudioBandSmoother playerLevels;
static TestAudioFrame playerAudio;
static uint64_t playerSequence=~uint64_t(0),playerEpoch=~uint64_t(0);
static unsigned playerSource=0,playerSteps=0;static float playerTime=0;
static bool playerInitialized=false,playerInitAttempted=false;
static char playerError[256]={};
static bool playerDriverReady=false;
static bool playerDriverExports(int handle){
 void *address=nullptr;const int rc=sceKernelDlsym(handle,"sceGnmSubmitCommandBuffers",&address);
 printf("HYPNOTIC MILKDROP DRIVER DLSYM HANDLE %08X RC %08X FOUND %d\n",(unsigned)handle,(unsigned)rc,address!=nullptr);
 return rc==0 && address;
}
static bool findPlayerDriver(){
 OrbisKernelModule modules[128];size_t count=0;const int rc=sceKernelGetModuleList(modules,128,&count);
 printf("HYPNOTIC MILKDROP DRIVER MODULE LIST RC %08X COUNT %u\n",(unsigned)rc,(unsigned)count);
 if(rc!=0)return false;if(count>128)count=128;
 for(size_t i=0;i<count;++i){OrbisKernelModuleInfo info={};info.size=sizeof(info);if(sceKernelGetModuleInfo(modules[i],&info)==0 && strstr(info.name,"Gnm"))printf("HYPNOTIC MILKDROP DRIVER MODULE %08X %s\n",(unsigned)modules[i],info.name);}
 // Resolve the actual export; do not depend on a particular module filename.
 for(size_t i=0;i<count;++i){void *address=nullptr;if(sceKernelDlsym(modules[i],"sceGnmSubmitCommandBuffers",&address)==0 && address){printf("HYPNOTIC MILKDROP DRIVER FOUND HANDLE %08X\n",(unsigned)modules[i]);return true;}}
 return false;
}
bool milkdropPlayerPreloadDriver(){
 std::string ioError;if(!milkdropFilesInit(ioError)){snprintf(playerError,sizeof(playerError),"%s",ioError.c_str());return false;}
 if(playerDriverReady)return true;
 try{
  // Load before the USB thread can change the process filesystem root.
  // Prefer an already-loaded export, otherwise retain the actual load result.
  if(findPlayerDriver()){playerDriverReady=true;return true;}
  char sandboxPath[256]={};const char *sandbox=sceKernelGetFsSandboxRandomWord();if(sandbox)snprintf(sandboxPath,sizeof(sandboxPath),"/%s/common/lib/libSceGnmDriver.sprx",sandbox);
  const char *paths[]={sandboxPath,"/system/common/lib/libSceGnmDriver.sprx","/common/lib/libSceGnmDriver.sprx"};int lastHandle=-1,lastStart=0;
  for(const char *path:paths){if(!*path)continue;lastStart=0;lastHandle=(int)sceKernelLoadStartModule(path,0,nullptr,0,nullptr,&lastStart);printf("HYPNOTIC MILKDROP DRIVER LOAD %s HANDLE %08X START %08X\n",path,(unsigned)lastHandle,(unsigned)lastStart);
   if((lastHandle>=0 && playerDriverExports(lastHandle)) || findPlayerDriver()){playerDriverReady=true;playerError[0]=0;return true;}
  }
  snprintf(playerError,sizeof(playerError),"GPU driver unavailable: LOAD %08X START %08X",(unsigned)lastHandle,(unsigned)lastStart);return false;
 }catch(const std::exception &e){snprintf(playerError,sizeof(playerError),"%s",e.what());return false;}
}
#include "player_readback.hpp"
#include "player_uniforms.hpp"
#include "player_native_timing.hpp"
#include "player_shapes.hpp"
#include "player_wave_pair.hpp"
#include "player_wave_check.hpp"
static bool playerSafeUniforms=true,playerSafeShapes=true,playerSafeWaves=true;
#include "player_hd.hpp"
static void playerInitialize(){
 if(playerInitialized)return;
 if(!milkdropPlayerPreloadDriver())throw std::runtime_error(playerError);
 if(playerInitAttempted)throw std::runtime_error(playerError[0]?playerError:"MilkDrop initialization previously failed");playerInitAttempted=true;
 off_t offset=0;int rc=sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),gpuCapacity,0x200000,ORBIS_KERNEL_WC_GARLIC,&offset);if(rc)fail("MILKDROP GPU MEMORY",rc);void *mapped=nullptr;
 rc=sceKernelMapDirectMemory(&mapped,gpuCapacity,ORBIS_KERNEL_PROT_CPU_READ|ORBIS_KERNEL_PROT_CPU_RW|ORBIS_KERNEL_PROT_GPU_READ|ORBIS_KERNEL_PROT_GPU_WRITE,0,offset,0x200000);if(rc)fail("MILKDROP GPU MAP",rc);gpuMemory=(unsigned char*)mapped;
 const auto mode=sceGnmGpuMode();const auto target=mode==GNM_GPU_NEO?PSBC_TARGET_PS4_NEO:PSBC_TARGET_PS4_BASE;
 if(!glslang_initialize_process())fail("MILKDROP COMPILER",-1);
 const PsbcResourceType resources[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER};PsbcCompileOptions options={target,PSBC_STAGE_FRAGMENT,"main",true};options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);options.resource_count=2;options.resources=resources;
 auto shader=compileText(target,PSBC_STAGE_FRAGMENT,feedback_frag,&options);playerFeedbackPS=(GnmPsShader*)prepareShader(shader,false);psbc_free_output(&shader);
 createGeometryShaders(target);createWarpShader(target);createCompositeShaders(target);createTexturedShaders(target);
 createFeedback(mode);createImageTextures(mode);createCommonRenderer(mode);createShapeTextureRenderer(mode);createLegacyFinalRenderer();
 geometryControl(&feedback[0].target,feedback[0].data,feedback[0].pitch,feedback[0].bytes);checkCpuZoomCompatibility();initializePlayerReadback();playerUseStagedUniforms=true;checkPlayerUniforms();checkPlayerShapePacking();checkPlayerWavePacking();
 playerSafeUniforms=playerUseStagedUniforms;playerSafeShapes=playerShapePacking;playerSafeWaves=playerWavePacking;initializePlayerHD(mode);
 playerBank=new CommunityBank();playerLayers=new PresetLayers();memset(feedback[0].data,0,feedback[0].bytes);memset(feedback[1].data,0,feedback[1].bytes);playerInitialized=true;
 printf("HYPNOTIC MILKDROP BRIDGE INITIALIZED - RENDERER0.50 - NO FPS PATCHES\n");
}
#include "player_prepare.hpp"
const char *milkdropPlayerError(){return playerError[0]?playerError:playerBank && playerBank->stopped?playerBank->reasons[playerBank->active].c_str():"Preset could not be rendered";}
bool milkdropPlayerApply(const char *path){
 try{playerInitialize();auto bank=playerBank;const bool staged=milkdropPlayerPrepareState(path)==3;const unsigned candidate=staged?(unsigned)playerPrepareJob->slot:bank->active==8?9:8;if(!staged){bank->discard(candidate);bank->examined[candidate]=bank->ready[candidate]=false;bank->paths[candidate]=path;bank->names[candidate]=path;}bank->count=10;bank->selected=candidate;
  if(!bank->accept()){snprintf(playerError,sizeof(playerError),"%s",bank->reasons[candidate].c_str());return false;}
  auto preset=bank->equations[bank->active];auto community=bank->waves[bank->active];playerLayers->restart();preset->restart();community->restart();bank->warps[bank->active]->restart();bank->shapes[bank->active]->restart(preset->initialOutput());bank->extras[bank->active]->restart(preset->initialOutput());playerTime=0;
  const int slot=bank->shaderSlots[bank->active];if(slot>=0){if(bank->lastDisplay!=playerSource)memcpy(feedback[playerSource].data,feedback[bank->lastDisplay].data,feedback[playerSource].bytes);activateCommon();playerSetCommonInputs(preset->initialOutput());updateCommonBlur(playerSource,commonPrograms[slot].blurLevels);}else{memset(feedback[0].data,0,feedback[0].bytes);memset(feedback[1].data,0,feedback[1].bytes);}activateLegacyFinal();
  playerNativeTiming.reset();playerError[0]=0;printf("HYPNOTIC MILKDROP APPLY %s\n",path);return true;
 }catch(const std::exception &e){snprintf(playerError,sizeof(playerError),"%s",e.what());printf("HYPNOTIC MILKDROP ERROR %s\n",playerError);return false;}
}
bool milkdropPlayerRender(const float *left,const float *right,unsigned count,uint64_t sequence,uint64_t epoch,bool available,bool paused,float dt,MilkdropPixels &out){
 try{if(!playerInitialized || !playerBank->ready[playerBank->active])return false;auto bank=playerBank;
  auto view=[&](){auto &s=feedback[bank->lastDisplay];const auto data=playerReadbackRequested && playerReadbackReady?copyPlayerReadback(s.data,s.pitch*height*4):s.data;out={data,width,height,s.pitch};};if(paused || bank->stopped){view();return true;}
  uint64_t profile[12]={};profile[0]=sceKernelGetProcessTime();
  if(!(dt>0) || !std::isfinite(dt))dt=1.f/60;if(dt>.1f)dt=.1f;
  if(epoch!=playerEpoch){playerLevels=AudioBandSmoother{};playerEpoch=epoch;playerSequence=~uint64_t(0);}
  if(sequence!=playerSequence){int16_t pcm[2048]={};count=std::min(count,1024u);if(available && left && right)for(unsigned i=0;i<count;++i){pcm[i*2]=(int16_t)std::max(-32768.f,std::min(32767.f,left[i]*32768.f));pcm[i*2+1]=(int16_t)std::max(-32768.f,std::min(32767.f,right[i]*32768.f));}AudioSpectrum unused;makeAudioFrame(pcm,available?count:0,unused,playerAudio);playerAudio.sequence=sequence;playerSequence=sequence;}
  playerLevels.update(playerAudio,dt);auto &audioFrame=playerAudio;auto &audioLevels=playerLevels;auto preset=bank->equations[bank->active];auto community=bank->waves[bank->active];auto layers=playerLayers;auto &source=playerSource;auto &steps=playerSteps;auto &time=playerTime;time+=dt;
  if(!preset->evaluate(time,1./dt,audioLevels.relative[0],audioLevels.relative[1],audioLevels.relative[2],audioLevels.attenuated[0],audioLevels.attenuated[1],audioLevels.attenuated[2])){bank->frameError("EQUATION FRAME",preset->error);snprintf(playerError,sizeof(playerError),"%s",preset->error.c_str());return false;}
  profile[1]=sceKernelGetProcessTime();
  const auto &v=preset->output;const int commonSlot=bank->shaderSlots[bank->active];const auto shader=playerFeedbackPS;
        // This table is used by the stock warp. Explicit HLSL sampler aliases
        // have independent immutable addressing/filter descriptors.
        for(auto &surface:feedback){GnmSampler sampler={};sampler.xymagfilter=sampler.xyminfilter=GNM_FILTER_BILINEAR;sampler.clampx=sampler.clampy=sampler.clampz=v.textureWrap>.0001?GNM_TEX_CLAMP_WRAP:GNM_TEX_CLAMP_CLAMP_LAST_TEXEL;memcpy(surface.table+32,&sampler,sizeof(sampler));}

        const float control[4]={3,dt,time,(float)pow(presetSurfaceDecay(v.decay),dt*60.0)};
        if(!community->build(audioFrame.fullLeft,audioFrame.fullRight,v,time)) {
            bank->frameError("WAVE FRAME",community->error.empty()?"Geometry capacity exceeded":community->error);return false;
        }
        auto shapes=bank->shapes[bank->active];
        if(!shapes->build(v)){bank->frameError("CUSTOM SHAPE FRAME",shapes->error);return false;}
        
        auto custom=bank->extras[bank->active];if(!custom->build(audioFrame.fullLeft,audioFrame.fullRight,v,audioFrame.spectrumLeft,audioFrame.spectrumRight)){bank->frameError("CUSTOM POINT FRAME",custom->error);return false;}
        

        auto spatial=bank->warps[bank->active];
        if(!layers->buildVectors(v) || !layers->buildBorder(v)){bank->frameError("LAYER FRAME",layers->error.empty()?"Border geometry capacity":layers->error);return false;}
        profile[2]=sceKernelGetProcessTime();
        // Previous-frame vector field is drawn into source before the warp.
        for(unsigned batch=0;batch<layers->vectorBatches;++batch)drawPresetGeometry(&feedback[source].target,layers->vectors[batch],true,GNM_PT_LINELIST,false,nullptr,nullptr,false,width,height,100);

        profile[3]=sceKernelGetProcessTime();
        if(spatial->enabled){
            if(!spatial->build(v,time,dt)){bank->frameError("WARP FRAME",spatial->error);return false;}
            profile[4]=sceKernelGetProcessTime();
            if(commonSlot>=0){playerSetCommonInputs(v);profile[5]=sceKernelGetProcessTime();drawCommon(source,source^1,spatial->mesh,commonSlot,false);profile[6]=sceKernelGetProcessTime();updateCommonBlur(source,commonPrograms[commonSlot].blurLevels);}
            else if(bank->active==7){setTextureUniforms(audioLevels.relative);texturedStep(source,source^1,spatial->mesh,brainWarpPS);}
            else if(bank->active==6)pipelineWarpStep(source,source^1,spatial->mesh,audioLevels.relative);
            else spatialStep(source,source^1,spatial->mesh);
        }else{
            const float inverse=(float)pow(v.zoom,-dt*60.0);
            if(!std::isfinite(inverse)){bank->frameError("WARP FRAME","Non-finite legacy feedback inverse zoom");return false;}
            const float settings[8]={inverse,(float)(v.rot*dt*60.0),(float)(v.dx*dt*60.0),(float)(v.dy*dt*60.0),(float)v.red,(float)v.green,(float)v.blue,1};
            feedbackStep(shader,source,source^1,control,settings);
        }
        profile[7]=sceKernelGetProcessTime();
        if(!profile[4])profile[4]=profile[3];if(!profile[5])profile[5]=profile[4];if(!profile[6])profile[6]=profile[7];
        playerDrawShapes(shapes,source);
        profile[8]=sceKernelGetProcessTime();
        {
            beginGeometryBatch(&feedback[source^1].target);
            for(auto &slot:custom->slots)if(slot.enabled && slot.mesh.count)drawStandardWave(&feedback[source^1].target,slot.mesh,slot.dots,slot.thick,slot.additive,slot.thick?2:1);
            endGeometryBatch();
        }
        

        playerDrawWavePair(&feedback[source^1].target,community->mesh,community->secondMesh,community->dots,community->thick,community->additive);
        if(v.darkenCenter>0)drawDarkenCentre(source^1);
        if(layers->border.count)drawPresetGeometry(&feedback[source^1].target,layers->border,true);
        layers->capture(spatial->mesh);
        
        profile[9]=sceKernelGetProcessTime();
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
        

  profile[10]=sceKernelGetProcessTime();view();profile[11]=sceKernelGetProcessTime();playerNativeTiming.add(profile,playerReadbackRequested);return true;
 }catch(const std::exception &e){snprintf(playerError,sizeof(playerError),"%s",e.what());return false;}
}
