#pragma once
#include <stdint.h>
#include <math.h>

constexpr unsigned AUDIO_SPECTRUM_BANDS=128;
struct AudioSpectrum { float bands[AUDIO_SPECTRUM_BANDS]={}; float peak=0; bool available=false; };

// A packed stereo FFT: real/imaginary inputs hold L/R, so antiphase stereo
// retains its energy rather than cancelling in a mono sum. No heap allocation.
class SpectrumAnalyzer
{
    static constexpr unsigned size=2048;
    float left[size]={},right[size]={},real[size],imag[size],window[size],cs[size/2],sn[size/2];
    unsigned cursor=0,filled=0;
    bool initialized=false;
public:
    void reset() { cursor=filled=0; }
    bool process(const int16_t *pcm,unsigned frames,AudioSpectrum &out,unsigned sampleRate=48000)
    {
        if(sampleRate<2)return false;
        if(!initialized)
        {
            for(unsigned i=0;i<size;++i) window[i]=0.5f-0.5f*sinf(6.283185307f*i/(size-1)+1.570796327f);
            for(unsigned i=0;i<size/2;++i) { cs[i]=sinf(6.283185307f*i/size+1.570796327f); sn[i]=-sinf(6.283185307f*i/size); }
            initialized=true;
        }
        float peak=0;
        for(unsigned i=0;i<frames;++i)
        {
            left[cursor]=pcm[i*2]/32768.0f; right[cursor]=pcm[i*2+1]/32768.0f;
            const float l=fabsf(left[cursor]),r=fabsf(right[cursor]);
            if(l>peak)peak=l; if(r>peak)peak=r;
            cursor=(cursor+1)&(size-1); if(filled<size)++filled;
        }
        if(filled<size) return false;
        for(unsigned i=0,j=0;i<size;++i)
        {
            real[j]=left[(cursor+i)&(size-1)]*window[i];
            imag[j]=right[(cursor+i)&(size-1)]*window[i];
            unsigned bit=size>>1; for(;j&bit;bit>>=1)j^=bit; j^=bit;
        }
        for(unsigned length=2;length<=size;length<<=1)
            for(unsigned start=0;start<size;start+=length)
                for(unsigned j=0;j<length/2;++j)
                {
                    const unsigned k=j*size/length,a=start+j,b=a+length/2;
                    const float r=real[b]*cs[k]-imag[b]*sn[k],im=real[b]*sn[k]+imag[b]*cs[k];
                    real[b]=real[a]-r; imag[b]=imag[a]-im; real[a]+=r; imag[a]+=im;
                }
        float edge=20;
        for(unsigned band=0;band<AUDIO_SPECTRUM_BANDS;++band)
        {
            const float next=edge*1.053611229f;
            if(edge>=sampleRate*.5f){out.bands[band]=0;edge=next;continue;}
            unsigned first=(unsigned)(edge*size/sampleRate),last=(unsigned)(next*size/sampleRate);
            if(first<1)first=1; if(last<=first)last=first+1; if(last>size/2)last=size/2;
            float energy=0;
            for(unsigned k=first;k<last;++k)
            {
                const unsigned mirror=size-k;
                const float lr=(real[k]+real[mirror])*0.5f,li=(imag[k]-imag[mirror])*0.5f;
                const float rr=(imag[k]+imag[mirror])*0.5f,ri=(real[mirror]-real[k])*0.5f;
                const float e=(lr*lr+li*li+rr*rr+ri*ri)*0.5f;
                if(e>energy)energy=e;
            }
            float value=sqrtf(sqrtf(energy)*(4.0f/size)*1.8f);
            out.bands[band]=value>1?1:value; edge=next;
        }
        out.peak=peak; out.available=true;
        return true;
    }
};
