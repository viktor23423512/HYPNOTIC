#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <stdexcept>
#include <cassert>
using OrbisPthread=std::thread*;using OrbisPthreadAttr=int;
using PsbcTarget=int;using GnmPsShader=int;
enum{PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER,PSBC_STAGE_FRAGMENT};using PsbcResourceType=int;
struct PsbcCompileOptions{int target,stage;const char *entry;bool optimize;unsigned address32_hi=0,resource_count=0;const PsbcResourceType *resources=nullptr;};
struct PsbcShaderOutput{void *data=nullptr;size_t size=0;};
static std::thread::id mainThread=std::this_thread::get_id();static int allocations=0,installs=0;static bool attrFailure=false,initFailure=false;
uint64_t sceKernelGetProcessTime(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int scePthreadAttrInit(int*){return attrFailure?-1:0;}int scePthreadAttrSetstacksize(int*,size_t){return 0;}void scePthreadAttrDestroy(int*){}
int scePthreadCreate(std::thread **out,int*,void*(*fn)(void*),void*arg,const char*){*out=new std::thread([=]{fn(arg);});return 0;}
void scePthreadJoin(std::thread *t,void*){t->join();delete t;}
void psbc_free_output(PsbcShaderOutput *o){if(o->data){free(o->data);o->data=nullptr;o->size=0;--allocations;}}
struct CommonProgram{uint64_t hash=0;GnmPsShader *warp=nullptr,*comp=nullptr;unsigned blurLevels=0;bool pointNoise[2]={};};
static CommonProgram commonPrograms[160];static unsigned commonProgramCount=0;static size_t gpuUsed=0;static unsigned char gpuMemory[4];static int commonTarget=0;
struct Bank{unsigned active=8,count=10;bool ready[10]={},examined[10]={};std::string paths[10],names[10],reasons[10];void discard(unsigned i){assert(std::this_thread::get_id()==mainThread);ready[i]=false;}bool prepare(unsigned i){assert(std::this_thread::get_id()==mainThread);ready[i]=true;return true;}};
static Bank bank;static Bank *playerBank=&bank;static char playerError[256];static bool playerSafeUniforms=true,playerSafeShapes=true,playerSafeWaves=true,playerUseStagedUniforms=true,playerShapePacking=true,playerWavePacking=true;
void playerInitialize(){if(initFailure)throw std::runtime_error("init failure");}
namespace libprojectM{namespace MilkdropPreset{struct PresetFileParser{std::string path;bool Read(const std::string&p){path=p;return p!="bad";}std::string GetCode(const char*){return path=="plain"?"":"shader_body { ret=GetBlur2(uv); }";}};}}
void completeShaderStages(std::string&,std::string&){}
bool translateCommonShader(const std::string&s,bool,std::string&o,std::string&,bool*p){assert(std::this_thread::get_id()!=mainThread);o=s;*p=false;return true;}
bool tryCompileFragment(const char*,const PsbcCompileOptions&o,PsbcShaderOutput&v,std::string&){assert(std::this_thread::get_id()!=mainThread&&o.resource_count==16);std::this_thread::sleep_for(std::chrono::milliseconds(15));v.data=malloc(32);v.size=32;++allocations;return true;}
GnmPsShader *tryPreparePixel(const PsbcShaderOutput&o,std::string&){assert(std::this_thread::get_id()==mainThread&&o.data&&o.size);++installs;return (int*)malloc(4);}
#include "player_prepare.hpp"
static void wait(){while(milkdropPlayerPrepareBusy())std::this_thread::sleep_for(std::chrono::milliseconds(1));}
int main(){assert(milkdropPlayerBeginPrepare("one"));int ticks=0;while(milkdropPlayerPrepareBusy()){++ticks;assert(!milkdropPlayerBeginPrepare("other"));std::this_thread::sleep_for(std::chrono::milliseconds(1));}assert(ticks>0&&bank.active==8&&installs==0);assert(milkdropPlayerPrepareState("one")==2);assert(milkdropPlayerFinishPrepare("one"));assert(bank.active==8&&bank.ready[9]&&installs==2&&allocations==0);assert(milkdropPlayerPrepareState("one")==3);assert(milkdropPlayerBeginPrepare("two"));wait();assert(milkdropPlayerFinishPrepare("two"));assert(installs==2); // identical shader cache reused
 assert(milkdropPlayerBeginPrepare("plain"));wait();assert(milkdropPlayerFinishPrepare("plain")&&bank.active==8);
 assert(milkdropPlayerBeginPrepare("bad"));wait();assert(milkdropPlayerPrepareState("bad")==-1&&!milkdropPlayerFinishPrepare("bad"));assert(bank.active==8);
 attrFailure=true;assert(!milkdropPlayerBeginPrepare("attr"));assert(milkdropPlayerPrepareState("attr")==-1&&!milkdropPlayerFinishPrepare("attr"));attrFailure=false;initFailure=true;assert(!milkdropPlayerBeginPrepare("init"));assert(milkdropPlayerPrepareState("init")==-1);initFailure=false;
 assert(milkdropPlayerBeginPrepare("recover"));wait();assert(milkdropPlayerFinishPrepare("recover"));playerPrepareJob->release();assert(allocations==0);milkdropPlayerSetBoosted(false);assert(!playerWavePacking&&!playerShapePacking&&!playerUseStagedUniforms);milkdropPlayerSetBoosted(true);assert(playerWavePacking&&playerShapePacking&&playerUseStagedUniforms);playerSafeShapes=false;milkdropPlayerSetBoosted(true);assert(!playerShapePacking);puts("PASS real worker handoff, render-thread progress, CPU/GPU ownership, prepared without activation, cache reuse, plain shaders, rejected/missing/thread/init failures, recovery and byte-check fallbacks");}
