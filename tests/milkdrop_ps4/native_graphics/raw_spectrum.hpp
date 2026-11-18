#pragma once
#include <array>
#include <algorithm>
#include <complex>
#include <cmath>
#include <cstdint>
// Fixed-storage equivalent of projectM Audio::PCM::UpdateSpectrum and
// MilkdropFFT(480,512,true,1): 128-scaled PCM, adjacent-sample damping,
// 480-sample Hann envelope, zero-padded 1024 FFT, logarithmic equalization.
class RawStereoSpectrum {
 std::array<unsigned,1024> reverse{};
 std::array<float,480> window{};
 std::array<float,512> equalize{};
 std::array<std::complex<float>,10> roots{};
 std::array<std::complex<float>,1024> work{};
public:
 RawStereoSpectrum(){
  constexpr float pi=3.14159265358979323846f;
  for(unsigned i=0;i<1024;++i){unsigned n=i,k=0;for(unsigned b=0;b<10;++b){k=(k<<1)|(n&1);n>>=1;}reverse[i]=k;}
  for(unsigned i=0;i<480;++i)window[i]=.5f+.5f*std::sin(i*(2*pi/480)-pi*.5f);
  for(unsigned i=0;i<512;++i)equalize[i]=-.02f*std::log((512-i)/512.f);
  for(unsigned i=0;i<10;++i)roots[i]=std::polar(1.f,-2*pi/(2u<<i));
 }
 void channel(const int16_t *pcm,unsigned frames,unsigned channel,float *out){
  const unsigned start=frames>576?frames-576:0;
  for(unsigned i=0;i<1024;++i){unsigned n=reverse[i];float v=0;
   if(n<480){unsigned a=std::min(start+n,frames-1),b=std::min(start+(n?n-1:0),frames-1);
    v=(pcm[2*a+channel]+(float)pcm[2*b+channel])*(64.f/32768)*window[n];}
   work[i]={v,0};
  }
  for(unsigned size=2,stage=0;size<=1024;size<<=1,++stage){
   std::complex<float> w(1,0);const unsigned half=size/2;
   for(unsigned m=0;m<half;++m){for(unsigned i=m;i<1024;i+=size){auto t=work[i+half]*w;work[i+half]=work[i]-t;work[i]+=t;}w*=roots[stage];}
  }
  for(unsigned i=0;i<512;++i)out[i]=equalize[i]*std::abs(work[i]);
 }
 void process(const int16_t *pcm,unsigned frames,float *left,float *right){channel(pcm,frames,0,left);channel(pcm,frames,1,right);}
};
