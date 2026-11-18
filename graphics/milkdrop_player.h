#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
struct MilkdropPixels {const unsigned char *rgba;unsigned width,height,pitch;};
bool milkdropPlayerPreloadDriver();
void milkdropPlayerSetReadback(bool enabled);
bool milkdropPlayerReadbackReady();
bool milkdropPlayerApply(const char *path);
bool milkdropPlayerRender(const float *left,const float *right,unsigned count,uint64_t sequence,uint64_t epoch,bool available,bool paused,float dt,MilkdropPixels &out);
const char *milkdropPlayerError();

// Fixed-size bridge staging buffer. Validate layout before any CPU copy.
inline bool copyMilkdropPixels(const MilkdropPixels &image,unsigned char *out,size_t capacity){
 if(!image.rgba || !out || !((image.width==512&&image.height==288)||(image.width==1920&&image.height==1080)) || image.pitch<image.width || image.pitch>8192 || capacity<(size_t)image.width*image.height*4u)return false;
 for(unsigned y=0;y<image.height;++y)memcpy(out+y*image.width*4,image.rgba+y*image.pitch*4,image.width*4);
 return true;
}

void milkdropPlayerSetBoosted(bool enabled);
bool milkdropPlayerRequestHD(bool hd);
unsigned milkdropPlayerWidth();unsigned milkdropPlayerHeight();
// Prepare states:0absent,1CPU compiler busy,2CPU shaders ready,3fully prepared,-1rejected.
int milkdropPlayerPrepareState(const char *path);
bool milkdropPlayerBeginPrepare(const char *path);
bool milkdropPlayerFinishPrepare(const char *path);
bool milkdropPlayerPrepareBusy();
