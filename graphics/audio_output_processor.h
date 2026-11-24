#pragma once
#include "audio_output_settings.h"
#include <math.h>
#include <stdint.h>
// Independent stereo peaking biquads, RBJ formulae:
// https://www.w3.org/TR/audio-eq-cookbook/# peakingEQ.
// Runs only on the playback worker; no allocation or locks in process().
class AudioOutputProcessor {
 struct Filter {
  double b0=1,b1=0,b2=0,a1=0,a2=0,z1[2]={},z2[2]={};bool active=false;
  void reset(){z1[0]=z1[1]=z2[0]=z2[1]=0;}
  void set(double hz,int db){reset();active=db!=0;if(!active){b0=1;b1=b2=a1=a2=0;return;}const double w=6.283185307179586*hz/48000.,A=pow(10.,db/40.),alpha=sin(w)/2.,a0=1+alpha/A;b0=(1+alpha*A)/a0;b1=-2*cos(w)/a0;b2=(1-alpha*A)/a0;a1=b1;a2=(1-alpha/A)/a0;}
  double step(double x,int c){if(!active)return x;double y=b0*x+z1[c];z1[c]=b1*x-a1*y+z2[c];z2[c]=b2*x-a2*y;return y;}
  double response(double w)const{if(!active)return 1;double c=cos(w),s=sin(w),c2=cos(2*w),s2=sin(2*w);double nr=b0+b1*c+b2*c2,ni=b1*s+b2*s2,dr=1+a1*c+a2*c2,di=a1*s+a2*s2;return sqrt((nr*nr+ni*ni)/(dr*dr+di*di));}
 };
 struct Chain {
  Filter filters[5];double headroom=1;bool flat=true;
  void set(const AudioOutputSettings &s){static const double hz[]={60,250,1000,4000,12000};flat=!s.eqEnabled||s.isFlat();for(int i=0;i<5;++i)filters[i].set(hz[i],s.eqEnabled?s.bands[i]:0);headroom=1;if(flat)return;
   double peak=1;for(int i=0;i<101;++i){double frequency=i<96?20*pow(1000.,i/95.):hz[i-96];double amplitude=1;for(const auto &f:filters)amplitude*=f.response(6.283185307179586*frequency/48000.);if(amplitude>peak)peak=amplitude;}if(peak>1.00001)headroom=1/(peak*1.0233);
  }
  void reset(){for(auto &f:filters)f.reset();}
  double step(double x,int c){for(auto &f:filters)x=f.step(x,c);return x*headroom;}
  void silenceDenormals(){for(auto &f:filters)for(int c=0;c<2;++c){if(fabs(f.z1[c])<1e-24)f.z1[c]=0;if(fabs(f.z2[c])<1e-24)f.z2[c]=0;}}
 } current,next;
 uint64_t eqKey=0;bool initialized=false;unsigned transition=0,volumeRamp=0;double gain=1,targetGain=1,gainStep=0;
 static constexpr unsigned rampFrames=480;
public:
 void configure(const AudioOutputSettings &s){const uint64_t key=s.packed()>>8;const double desired=s.muted?0:AudioOutputSettings::bound(s.volume,0,100)/100.;
  if(!initialized){current.set(s);next=current;eqKey=key;gain=targetGain=desired;initialized=true;return;}
  if(desired!=targetGain){targetGain=desired;volumeRamp=rampFrames;gainStep=(targetGain-gain)/rampFrames;}
  // Defer a new coefficient set until the active crossfade ends.
  if(key!=eqKey&&!transition){next.set(s);eqKey=key;transition=rampFrames;}
 }
 void resetFilters(){if(transition)current=next;current.reset();next=current;transition=0;}
 void process(int16_t *pcm,unsigned frames){if(!pcm||!frames)return;if(!transition&&current.flat&&!volumeRamp&&gain==1)return;
  for(unsigned i=0;i<frames;++i){if(volumeRamp){gain+=gainStep;if(!--volumeRamp)gain=targetGain;}const double blend=transition?(rampFrames-transition+1)/(double)rampFrames:0;
   for(int c=0;c<2;++c){double x=pcm[i*2+c]/32768.,y=current.step(x,c);if(transition)y=y*(1-blend)+next.step(x,c)*blend;y*=gain;double sample=y*32768.;if(!isfinite(sample)){sample=0;current.reset();next.reset();}if(sample>32767)sample=32767;if(sample< -32768)sample=-32768;pcm[i*2+c]=(int16_t)lrint(sample);}
   if(transition&&!--transition)current=next;
  }current.silenceDenormals();next.silenceDenormals();
 }
};
