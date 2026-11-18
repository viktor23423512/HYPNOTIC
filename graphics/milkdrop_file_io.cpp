#include "milkdrop_file_io.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <exception>
#ifndef MILKDROP_FILE_IO_TEST
#include <orbis/libkernel.h>
#include "milkdrop_asset_inventory.h"
#endif
// Read once before the USB thread changes root. Presets remain original bytes.
// No dependency on libc file wrappers, openat support or later app0 resolution.
static std::vector<char> milkdropPackageCache;
static bool milkdropCacheReady=false;
static bool readMilkdropNative(const char *path,char *out,size_t bytes,std::string &error){
 const int fd=sceKernelOpen(path,0,0);
 if(fd<0){char text[180];snprintf(text,sizeof(text),"File open %08X: %.120s",(unsigned)fd,path);error=text;return false;}
 struct Close{int fd;~Close(){sceKernelClose(fd);}} close{fd};const auto length=sceKernelLseek(fd,0,2);
 if(length<0 || (uint64_t)length!=bytes || sceKernelLseek(fd,0,0)<0){error="File size mismatch or seek failure";return false;}
 size_t offset=0;while(offset<bytes){const auto n=sceKernelRead(fd,out+offset,bytes-offset);if((int64_t)n<=0 || (uint64_t)n>bytes-offset){error="Incomplete preset/texture read";return false;}offset+=(size_t)n;}return true;
}
bool milkdropFilesInit(std::string &error){
 if(milkdropCacheReady)return true;
 try{if(milkdropBundledBytes>8*1024*1024){error="Package cache exceeds limit";return false;}milkdropPackageCache.resize(milkdropBundledBytes);
  for(const auto &asset:milkdropBundledAssets){if(asset.offset>milkdropPackageCache.size() || asset.bytes>milkdropPackageCache.size()-asset.offset){error="Invalid package cache inventory";return false;}
   if(!readMilkdropNative(asset.path,milkdropPackageCache.data()+asset.offset,asset.bytes,error)){printf("HYPNOTIC MILKDROP CACHE ERROR %s\n",error.c_str());return false;}
  }
  milkdropCacheReady=true;printf("HYPNOTIC MILKDROP PACKAGE CACHE READY %u FILES %u BYTES\n",(unsigned)(sizeof(milkdropBundledAssets)/sizeof(milkdropBundledAssets[0])),(unsigned)milkdropPackageCache.size());return true;
 }catch(const std::exception&){milkdropPackageCache.clear();error="Package cache allocation failed";return false;}
}
bool readMilkdropFile(const char *path,std::vector<char> &bytes,size_t maxBytes,std::string &error){
 bytes.clear();if(!path || !*path){error="Missing preset/texture path";return false;}
 if(!strncmp(path,"/app0/",6)){
  if(!milkdropCacheReady){error="Package asset cache unavailable";return false;}
  for(const auto &asset:milkdropBundledAssets)if(!strcmp(path,asset.path)){
   if(asset.bytes>maxBytes){error="Oversized preset/texture";return false;}
   bytes.assign(milkdropPackageCache.data()+asset.offset,milkdropPackageCache.data()+asset.offset+asset.bytes);return true;
  }
  error="Bundled asset is absent from package cache";return false;
 }
 const int fd=sceKernelOpen(path,0,0);if(fd<0){char text[160];snprintf(text,sizeof(text),"External file open %08X: %.100s",(unsigned)fd,path);error=text;printf("HYPNOTIC MILKDROP FILE ERROR %s\n",text);return false;}
 struct Close{int fd;~Close(){sceKernelClose(fd);}} close{fd};const auto length=sceKernelLseek(fd,0,2);
 if(length<0 || (uint64_t)length>maxBytes || sceKernelLseek(fd,0,0)<0){error="Unreadable or oversized external file";return false;}bytes.resize((size_t)length);size_t offset=0;
 while(offset<bytes.size()){const auto n=sceKernelRead(fd,bytes.data()+offset,bytes.size()-offset);if((int64_t)n<=0 || (uint64_t)n>bytes.size()-offset){bytes.clear();error="Incomplete external file read";return false;}offset+=(size_t)n;}return true;
}
