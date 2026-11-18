#pragma once
#include "../../../graphics/audio_spectrum.h"
#include <string.h>
#include "raw_spectrum.hpp"

struct TestAudioFrame {
    float left[64]={},right[64]={};
    float fullLeft[480]={},fullRight[480]={};
    float spectrumLeft[512]={},spectrumRight[512]={};
    float bands[3]={},peak=0;
    uint64_t sequence=0;
    bool available=false;
};
inline void makeAudioFrame(const int16_t *pcm,unsigned frames,const AudioSpectrum &spectrum,TestAudioFrame &out)
{
    if(!pcm || !frames || frames>1024){out=TestAudioFrame{};return;}
    out.available=true;++out.sequence;out.peak=0;
    static RawStereoSpectrum rawSpectrum;rawSpectrum.process(pcm,frames,out.spectrumLeft,out.spectrumRight);
    for(unsigned i=0;i<frames*2;++i) {
        const float value=fabsf(pcm[i]/32768.0f);if(value>out.peak)out.peak=value;
    }
    for(unsigned i=0;i<64;++i) {
        const unsigned index=i*(frames-1)/63;
        out.left[i]=pcm[index*2]/32768.0f;out.right[i]=pcm[index*2+1]/32768.0f;
    }
    const unsigned start=frames>576?frames-576:0;
    for(unsigned i=0;i<480;++i) {
        const unsigned index=start+i<frames?start+i:frames-1;
        out.fullLeft[i]=pcm[index*2]/32768.0f;out.fullRight[i]=pcm[index*2+1]/32768.0f;
    }
    unsigned counts[3]={};out.bands[0]=out.bands[1]=out.bands[2]=0;
    if(spectrum.available) {
        float frequency=20;
        for(unsigned i=0;i<AUDIO_SPECTRUM_BANDS;++i) {
            const unsigned band=frequency<250?0:frequency<4000?1:2;
            out.bands[band]+=spectrum.bands[i];++counts[band];frequency*=1.053611229f;
        }
        for(unsigned i=0;i<3;++i)if(counts[i])out.bands[i]=2*out.bands[i]/counts[i];
    }
}
class AudioBandSmoother {
public:
    float bands[3]={};
    float average[3]={},longAverage[3]={},attenuated[3]={},relative[3]={};
    unsigned frames=0;
    void update(const TestAudioFrame &frame,float dt) {
        if(!(dt>0))return;if(dt>0.1f)dt=0.1f;
        ++frames;
        for(unsigned i=0;i<3;++i) {
            float target=frame.available?frame.bands[i]:0;
            if(!(target>=0))target=0;if(target>2)target=2;
            const float tau=target>bands[i]?0.06f:0.25f;
            bands[i]+=(target-bands[i])*(1-expf(-dt/tau));
            // Match projectM 4.1.8 PCM::UpdateFrameAudioData / Loudness:
            // left raw spectrum, three equal sixths, no relative-level cap.
            float sum=0;
            if(frame.available)for(unsigned bin=512*i/6;bin<512*(i+1)/6;++bin)sum+=frame.spectrumLeft[bin];
            const float rate=std::pow(std::pow(sum>average[i]?0.2f:0.5f,30.f),dt);
            average[i]=average[i]*rate+sum*(1-rate);
            const float slow=std::pow(std::pow(frames-1<50?0.9f:0.992f,30.f),dt);
            longAverage[i]=longAverage[i]*slow+sum*(1-slow);
            relative[i]=!frame.available?0:std::fabs(longAverage[i])<0.001f?1:sum/longAverage[i];
            attenuated[i]=!frame.available?0:std::fabs(longAverage[i])<0.001f?1:average[i]/longAverage[i];
        }
    }
};
