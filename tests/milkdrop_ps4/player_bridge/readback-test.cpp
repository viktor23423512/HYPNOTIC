#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <cassert>
#include <malloc.h>
#include <sys/types.h>
static constexpr unsigned height=288;
static constexpr int ORBIS_KERNEL_WB_ONION=0,ORBIS_KERNEL_PROT_CPU_READ=1,ORBIS_KERNEL_PROT_CPU_RW=2,ORBIS_KERNEL_PROT_GPU_READ=4,ORBIS_KERNEL_PROT_GPU_WRITE=8,GNM_ACQUIRE_TARGET_CB0=1;
struct GnmCommandBuffer{};
static unsigned char commands[4096];static void *geometryCommands=commands;static constexpr size_t geometryCommandBytes=sizeof(commands);
struct Surface{unsigned char *data;unsigned pitch;};static Surface feedback[2];
static bool allocFail=false,mapFail=false,corrupt=false;static unsigned copies=0,finishes=0,releases=0;
static void fail(const char *s,int){throw std::runtime_error(s);}
static size_t sceKernelGetDirectMemorySize(){return 1024*1024;}
static int sceKernelAllocateDirectMemory(size_t,size_t,size_t,size_t,int type,off_t *offset){assert(type==0);*offset=0;return allocFail?-1:0;}
static int sceKernelMapDirectMemory(void **p,size_t n,int,int,off_t,size_t){if(mapFail)return -1;*p=_aligned_malloc(n,16384);assert(*p);return 0;}
static void sceKernelReleaseDirectMemory(off_t,size_t){++releases;}
static GnmCommandBuffer sceGnmCmdInit(void*,size_t,void*,void*){return {};}
static void sceGnmDrawCmdWaitGraphicsWrite(GnmCommandBuffer*,int){}
static bool sceGnmDrawCmdCopyMemory(GnmCommandBuffer*,uint64_t dst,uint64_t src,uint32_t n){++copies;memcpy((void*)(uintptr_t)dst,(void*)(uintptr_t)src,n);if(corrupt)((unsigned char*)(uintptr_t)dst)[0]^=1;return true;}
static void finishGeometryCommands(GnmCommandBuffer&,unsigned poll){assert(poll==1000);++finishes;}
#include "player_readback.hpp"
int main(){
 const size_t n=520*height*4;for(unsigned i=0;i<2;++i){feedback[i]={static_cast<unsigned char*>(malloc(n)),520};memset(feedback[i].data,i?0xA7:0x39,n);}
 allocFail=true;initializePlayerReadback();assert(!playerReadbackReady);allocFail=false;mapFail=true;initializePlayerReadback();assert(!playerReadbackReady&&releases==1);mapFail=false;
 initializePlayerReadback();assert(playerReadbackReady&&copies==4&&finishes==4);assert(!memcmp(playerReadback,feedback[1].data,n));
 milkdropPlayerSetReadback(true);assert(playerReadbackRequested&&milkdropPlayerReadbackReady());milkdropPlayerSetReadback(false);assert(!playerReadbackRequested);
 for(size_t bad:{size_t(0),size_t(3),playerReadbackCapacity+4}){bool thrown=false;try{copyPlayerReadback(feedback[0].data,bad);}catch(const std::runtime_error&){thrown=true;}assert(thrown);}
 auto old=playerReadback;playerReadbackReady=false;corrupt=true;initializePlayerReadback();assert(!playerReadbackReady);_aligned_free(old);_aligned_free(playerReadback);for(auto &s:feedback)free(s.data);
 puts("PASS exact alternating padded-surface copies, completion waits, bounded size checks, optional allocation/map failure and corrupt-copy fallback, mode control");
}
