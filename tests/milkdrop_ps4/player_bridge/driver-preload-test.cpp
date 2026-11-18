#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <stdexcept>
#include <string>
using OrbisKernelModule=int;struct OrbisKernelModuleInfo{size_t size;char name[256];};
static char playerError[256]={};static unsigned scenario=0,loads=0;static bool loaded=false;
static int sceKernelGetModuleList(OrbisKernelModule *out,size_t capacity,size_t *count){assert(capacity==128);*count=(scenario==0 || loaded)?1:0;if(*count)out[0]=scenario==0?5:42;return 0;}
static int sceKernelGetModuleInfo(int,OrbisKernelModuleInfo *info){strcpy(info->name,"UnexpectedDriverModuleName");return 0;}
static int sceKernelDlsym(int handle,const char *name,void **out){assert(!strcmp(name,"sceGnmSubmitCommandBuffers"));*out=((scenario==0&&handle==5)||(loaded&&handle==42))?(void*)uintptr_t(123):nullptr;return *out?0:-3;}
static const char *sceKernelGetFsSandboxRandomWord(){return "sandbox";}
static uint32_t sceKernelLoadStartModule(const char *path,size_t,const void*,uint32_t,void*,int *start){++loads;*start=0;
 if(scenario==1 || (scenario==3 && !strcmp(path,"/system/common/lib/libSceGnmDriver.sprx"))){loaded=true;return 42;}return (uint32_t)-2;}
#include "driver-preload-under-test.hpp"
int main(){for(scenario=0;scenario<4;++scenario){playerDriverReady=loaded=false;loads=0;playerError[0]=0;bool ok=milkdropPlayerPreloadDriver();if(scenario==2){assert(!ok&&loads==3);assert(strstr(playerError,"LOAD FFFFFFFE START 00000000"));}else{assert(ok);assert(loads==(scenario==0?0:scenario==1?1:2));assert(milkdropPlayerPreloadDriver());}}
 puts("PASS existing export with unexpected module name, early sandbox load, system-path fallback, actual negative load errors and cached readiness");}
