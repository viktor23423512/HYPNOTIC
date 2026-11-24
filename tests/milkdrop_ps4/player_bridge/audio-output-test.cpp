#include <cassert>
#include <cstdio>
#include <vector>
#include <cmath>
#include "audio_output_processor.h"
double rms(const std::vector<int16_t> &v){double sum=0;int n=0;for(unsigned i=8192;i<v.size()/2;++i){sum+=(double)v[2*i]*v[2*i];++n;}return sqrt(sum/n);}
std::vector<int16_t> tone(double hz){std::vector<int16_t> pcm(96000);for(int i=0;i<48000;++i)pcm[2*i]=pcm[2*i+1]=(int16_t)lrint(3000*sin(6.283185307179586*hz*i/48000));return pcm;}
int main(){AudioOutputSettings s;std::vector<int16_t> original(65536);unsigned seed=3;for(auto &v:original){seed=seed*1664525+1013904223;v=(int16_t)(seed>>16);}auto data=original;AudioOutputProcessor flat;flat.configure(s);flat.process(data.data(),data.size()/2);assert(data==original);
 s.volume=50;AudioOutputProcessor volume;volume.configure(s);data=original;volume.process(data.data(),data.size()/2);for(unsigned i=0;i<data.size();++i)assert(abs((int)data[i]-(int)lrint(original[i]*.5))<=1);
 s.muted=true;volume.configure(s);data.assign(4096,10000);volume.process(data.data(),2048);for(unsigned i=1;i<480;++i)assert(abs((int)data[i*2]-data[(i-1)*2])<=22);for(unsigned i=480;i<2048;++i)assert(data[i*2]==0&&data[i*2+1]==0);
 const double hz[]={60,250,1000,4000,12000};for(int band=0;band<5;++band){s=AudioOutputSettings{};s.bands[band]=-6;AudioOutputProcessor dsp;dsp.configure(s);data=tone(hz[band]);double before=rms(data);dsp.process(data.data(),data.size()/2);assert(fabs(rms(data)/before-pow(10.,-6/20.))<.004);s.eqEnabled=false;AudioOutputProcessor off;off.configure(s);auto bypass=original;off.process(bypass.data(),bypass.size()/2);assert(bypass==original);}
 s=AudioOutputSettings{};s.bands[2]=6;AudioOutputProcessor boost;boost.configure(s);data=tone(1000);double before=rms(data);boost.process(data.data(),data.size()/2);assert(fabs(rms(data)/before-1/1.0233)<.006);
 s=AudioOutputSettings{};s.bands[0]=12;AudioOutputProcessor stereo;stereo.configure(s);data=tone(60);for(unsigned i=0;i<data.size()/2;++i)data[i*2+1]=0;stereo.process(data.data(),data.size()/2);for(unsigned i=0;i<data.size()/2;++i)assert(data[i*2+1]==0);
 for(int v=0;v<=100;v+=5)for(int g=-12;g<=12;++g){s.volume=v;s.muted=v%10!=0;s.eqEnabled=g%2!=0;for(int &n:s.bands)n=g;assert(AudioOutputSettings::unpack(s.packed()).packed()==s.packed());}
 std::vector<int16_t> guarded(2052,12345);for(int k=0;k<100;++k){s.volume=(k*17)%101;s.muted=k%7==0;for(int i=0;i<5;++i)s.bands[i]=(k+i)%25-12;stereo.configure(s);if(k%9==0)stereo.resetFilters();stereo.process(guarded.data()+2,1024);assert(guarded[0]==12345&&guarded[1]==12345&&guarded[2050]==12345&&guarded[2051]==12345);}stereo.process(nullptr,100);stereo.process(guarded.data(),0);
 puts("PASS flat/off bit-exact PCM, volume/mute ramps, five measured -6dB responses, boost headroom, stereo isolation, settings encoding, rapid changes/seek resets and buffer bounds");}
