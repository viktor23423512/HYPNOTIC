#pragma once
#include <stdint.h>
#include <math.h>
// Smoothed pressure -> timeline velocity; millisecond seeks at most20Hz.
class TriggerScrub {
 float carry=0,elapsed=0,velocity=0;int direction=0,legacyCarry=0;
 static float speed(unsigned pressure){if(pressure<=24)return 0;float p=(pressure-24)/231.f;if(p>1)p=1;return .5f+59.5f*p*p;}
public:
 void reset(){carry=elapsed=velocity=0;direction=legacyCarry=0;}
 int updateMs(unsigned left,unsigned right,uint64_t microseconds){
  const float target=speed(right)-speed(left);int sign=target>0?1:target<0?-1:0;
  if(!sign){reset();return 0;}if(sign!=direction){reset();direction=sign;}
  if(microseconds>100000)microseconds=100000;float dt=microseconds/1000000.f;
  // Exact integral of exponential velocity smoothing, independent of FPS.
  float decay=expf(-dt/.06f);carry+=(target*dt+(velocity-target)*.06f*(1-decay))*1000;velocity=target+(velocity-target)*decay;elapsed+=dt;
  if(elapsed<.05f)return 0;elapsed=0;int ms=(int)carry;carry-=ms;return ms;
 }
 int update(unsigned left,unsigned right,uint64_t us){legacyCarry+=updateMs(left,right,us);int seconds=legacyCarry/1000;legacyCarry-=seconds*1000;return seconds;}
};
