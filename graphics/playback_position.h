#pragma once
#include <stdint.h>

inline uint64_t seekTargetFrameMs(uint64_t current,uint64_t total,int64_t milliseconds){
 if(current>total)current=total;const bool back=milliseconds<0;
 const uint64_t amount=back?(uint64_t)(-(milliseconds+1))+1:(uint64_t)milliseconds;
 if(amount>UINT64_MAX/48)return back?0:total;const uint64_t delta=amount*48;
 return back?(delta>current?0:current-delta):(delta>total-current?total:current+delta);
}
inline uint64_t seekTargetFrame(uint64_t current,uint64_t total,int seconds){return seekTargetFrameMs(current,total,(int64_t)seconds*1000);}
inline uint64_t scaleAudioFrames(uint64_t frames, unsigned fromRate, unsigned toRate)
{
    return (frames / fromRate) * toRate + (frames % fromRate) * toRate / fromRate;
}
