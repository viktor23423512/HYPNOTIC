#pragma once
#include <atomic>
// The worker owns only source parsing/translation and CPU shader outputs.
// It never executes equations, changes GPU allocations or submits GPU work.
// projectM's shared RNG is therefore never touched concurrently.
struct PlayerPrepareJob {
 std::atomic<int> state{0};std::string path,error;OrbisPthread thread{};bool joinable=false,shaders=false;
 uint64_t hash=0,cache[160]={};unsigned cacheCount=0,hi=0,blur=0;int cached=-1,slot=-1;PsbcTarget target{};bool pointNoise[2]={};PsbcShaderOutput outputs[2]={};
 void release(bool destroy=true){if(joinable){scePthreadJoin(thread,nullptr);joinable=false;}if(destroy)for(auto &o:outputs){psbc_free_output(&o);o={};}}
};
static PlayerPrepareJob *playerPrepareJob=nullptr;
void milkdropPlayerSetBoosted(bool enabled){playerUseStagedUniforms=enabled&&playerSafeUniforms;playerShapePacking=enabled&&playerSafeShapes;playerWavePacking=enabled&&playerSafeWaves;}
static void *playerCompileNext(void *argument){
 auto &j=*(PlayerPrepareJob*)argument;const auto start=sceKernelGetProcessTime();
 try{
  libprojectM::MilkdropPreset::PresetFileParser file;if(!file.Read(j.path)){j.error="Cannot read preset for background preparation";j.state.store(-1,std::memory_order_release);return nullptr;}
  auto warp=file.GetCode("warp_"),comp=file.GetCode("comp_");j.shaders=!warp.empty()||!comp.empty();
  if(j.shaders){completeShaderStages(warp,comp);j.hash=14695981039346656037ULL;for(char c:warp+"\n---\n"+comp){j.hash^=(unsigned char)c;j.hash*=1099511628211ULL;}
   for(unsigned i=0;i<j.cacheCount;++i)if(j.cache[i]==j.hash)j.cached=i;
   const auto bodies=warp+"\n"+comp;for(unsigned level=1;level<=3;++level)if(bodies.find("GetBlur"+std::to_string(level))!=std::string::npos||bodies.find("sampler_blur"+std::to_string(level))!=std::string::npos)j.blur=level;
   if(j.cached<0){
    const PsbcResourceType bindings[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_IMAGE_SAMPLER};
    PsbcCompileOptions options={j.target,PSBC_STAGE_FRAGMENT,"main",true};options.address32_hi=j.hi;options.resource_count=16;options.resources=bindings;
    for(unsigned i=0;i<2;++i){std::string glsl;if(!translateCommonShader(i?comp:warp,i==0,glsl,j.error,&j.pointNoise[i])||!tryCompileFragment(glsl.c_str(),options,j.outputs[i],j.error)){j.state.store(-1,std::memory_order_release);return nullptr;}}
   }
  }
  printf("HYPNOTIC PREFETCH CPU READY %s %llu US CACHE %d\n",j.path.c_str(),(unsigned long long)(sceKernelGetProcessTime()-start),j.cached);j.state.store(2,std::memory_order_release);
 }catch(const std::exception &e){j.error=e.what();j.state.store(-1,std::memory_order_release);}return nullptr;
}
bool milkdropPlayerPrepareBusy(){return playerPrepareJob&&playerPrepareJob->state.load(std::memory_order_acquire)==1;}
int milkdropPlayerPrepareState(const char *path){auto j=playerPrepareJob;if(!j||j->path!=path)return 0;return j->state.load(std::memory_order_acquire);}
bool milkdropPlayerBeginPrepare(const char *path){
 try{playerInitialize();if(!playerPrepareJob)playerPrepareJob=new PlayerPrepareJob();auto &j=*playerPrepareJob;
  const int state=j.state.load(std::memory_order_acquire);if(state==1)return false;if(j.path==path&&state)return true;
  j.release();if(j.slot>=0&&unsigned(j.slot)!=playerBank->active){playerBank->discard(j.slot);playerBank->ready[j.slot]=playerBank->examined[j.slot]=false;}
  j.path=path;j.error.clear();j.shaders=false;j.cached=-1;j.slot=-1;j.state.store(0);j.hash=0;j.blur=0;j.pointNoise[0]=j.pointNoise[1]=false;j.target=commonTarget;j.hi=(uint32_t)((uintptr_t)gpuMemory>>32);j.cacheCount=commonProgramCount;for(unsigned i=0;i<j.cacheCount;++i)j.cache[i]=commonPrograms[i].hash;
  OrbisPthreadAttr attr;int rc=scePthreadAttrInit(&attr);if(rc){j.error="Background thread attributes failed";j.state.store(-1);snprintf(playerError,sizeof(playerError),"%s",j.error.c_str());return false;}
  rc=scePthreadAttrSetstacksize(&attr,4*1024*1024);if(rc){scePthreadAttrDestroy(&attr);j.error="Background thread stack failed";j.state.store(-1);snprintf(playerError,sizeof(playerError),"%s",j.error.c_str());return false;}
  j.state.store(1,std::memory_order_release);rc=scePthreadCreate(&j.thread,&attr,playerCompileNext,&j,"hypnotic-preset");scePthreadAttrDestroy(&attr);
  if(rc){j.error="Cannot start preset compiler thread";j.state.store(-1);snprintf(playerError,sizeof(playerError),"%s",j.error.c_str());return false;}j.joinable=true;return true;
 }catch(const std::exception &e){if(!playerPrepareJob)playerPrepareJob=new PlayerPrepareJob();playerPrepareJob->path=path;playerPrepareJob->error=e.what();playerPrepareJob->state.store(-1,std::memory_order_release);snprintf(playerError,sizeof(playerError),"%s",e.what());return false;}
}
bool milkdropPlayerFinishPrepare(const char *path){
 auto jp=playerPrepareJob;if(!jp||jp->path!=path)return false;auto &j=*jp;const int state=j.state.load(std::memory_order_acquire);
 if(state==3)return true;if(state==1)return false;if(state<0){j.release();snprintf(playerError,sizeof(playerError),"%s",j.error.c_str());return false;}if(state!=2)return false;
 const auto started=sceKernelGetProcessTime();j.release(false);
 try{
  if(j.shaders&&j.cached<0){
   if(commonProgramCount>=160)throw std::runtime_error("Shader cache full (160 presets) - relaunch to clear");
   CommonProgram program;program.hash=j.hash;program.blurLevels=j.blur;program.pointNoise[0]=j.pointNoise[0];program.pointNoise[1]=j.pointNoise[1];
   auto checkpoint=gpuUsed;program.warp=tryPreparePixel(j.outputs[0],j.error);if(program.warp)program.comp=tryPreparePixel(j.outputs[1],j.error);
   if(!program.warp||!program.comp){free(program.warp);free(program.comp);gpuUsed=checkpoint;throw std::runtime_error(j.error);}
   j.cached=commonProgramCount;commonPrograms[commonProgramCount++]=program;
  }
  for(auto &o:j.outputs){psbc_free_output(&o);o={};}
  auto bank=playerBank;j.slot=bank->active==8?9:8;bank->discard(j.slot);bank->examined[j.slot]=bank->ready[j.slot]=false;bank->paths[j.slot]=j.path;bank->names[j.slot]=j.path;bank->count=10;
  if(!bank->prepare(j.slot))throw std::runtime_error(bank->reasons[j.slot]);
  j.state.store(3,std::memory_order_release);printf("HYPNOTIC PREFETCH PREPARED %s COMMIT_SETUP %llu US\n",path,(unsigned long long)(sceKernelGetProcessTime()-started));return true;
 }catch(const std::exception &e){j.error=e.what();snprintf(playerError,sizeof(playerError),"%s",e.what());j.state.store(-1,std::memory_order_release);return false;}
}
