#pragma once
struct PlayerNativeTiming {
 unsigned frames=0;bool mode=false;uint64_t window=0,total[11]={};
 void reset(){frames=0;window=sceKernelGetProcessTime();for(auto &v:total)v=0;}
 void add(const uint64_t *t,bool cached){
  if(mode!=cached){mode=cached;reset();}
  for(unsigned i=0;i<11;++i)total[i]+=t[i+1]-t[i];++frames;
  const auto now=sceKernelGetProcessTime();if(now-window<4000000)return;
  printf("HYPNOTIC MILKDROP NATIVE PROFILE MODE %u FRAMES %u AUDIO_EQ %llu BUILD %llu VECTORS %llu WARP_CPU %llu SETTINGS %llu WARP_GPU %llu BLUR %llu SHAPES %llu WAVES_LAYERS %llu COMPOSITE %llu READBACK %llu US\n",mode,frames,
   (unsigned long long)(total[0]/frames),(unsigned long long)(total[1]/frames),(unsigned long long)(total[2]/frames),(unsigned long long)(total[3]/frames),(unsigned long long)(total[4]/frames),(unsigned long long)(total[5]/frames),(unsigned long long)(total[6]/frames),(unsigned long long)(total[7]/frames),(unsigned long long)(total[8]/frames),(unsigned long long)(total[9]/frames),(unsigned long long)(total[10]/frames));reset();
 }
};
static PlayerNativeTiming playerNativeTiming;
