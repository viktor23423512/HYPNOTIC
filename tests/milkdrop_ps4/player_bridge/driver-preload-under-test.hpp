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
