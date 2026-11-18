#pragma once
#include <stdint.h>

struct AudioStereoScope
{
    static constexpr unsigned capacity=1024;
    float left[capacity]={},right[capacity]={};
    unsigned count=0;
    uint64_t sequence=0,epoch=0;
    bool available=false;
};
inline void scopeFromStereo(const int16_t *pcm,unsigned frames,AudioStereoScope &out)
{
    if(frames>out.capacity)frames=out.capacity;
    out.count=frames;out.available=frames!=0;
    for(unsigned i=0;i<frames;++i){out.left[i]=pcm[i*2]/32768.0f;out.right[i]=pcm[i*2+1]/32768.0f;}
}
