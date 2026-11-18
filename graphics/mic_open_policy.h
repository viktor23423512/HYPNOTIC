#pragma once
#include <stdint.h>

struct MicOpenResult {int port=-1;unsigned frames=0,attempts=0;int firstCode=0;};
// 0.47's 512-frame request was rejected on the console. Probe a small bounded
// set of power-of-two sizes, changing no other parameter. Only INVALID_SIZE
// permits another attempt; routing, permissions and device errors stop here.
template<class Open>
MicOpenResult openMicrophoneBlock(Open open)
{
    const unsigned candidates[]={256,1024,2048,128};
    MicOpenResult result;
    for(unsigned frames:candidates)
    {
        result.frames=frames;++result.attempts;result.port=open(frames);
        if(result.attempts==1)result.firstCode=result.port;
        if(result.port>=0 || (uint32_t)result.port!=0x80260102u)break;
    }
    return result;
}
