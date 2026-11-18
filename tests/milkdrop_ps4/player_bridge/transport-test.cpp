#include "track_sequence.h"
#include "trigger_scrub.h"
#include "playback_position.h"
#include <cassert>
#include <cstdio>
#include <set>
#include <limits>
#include <cmath>
static TrackLibrary tracks;
static int scrub(unsigned pressure,unsigned hz){TriggerScrub s;int total=0;for(unsigned i=0;i<hz*2;++i)total+=s.updateMs(0,pressure,1000000/hz);return total;}
int main(){for(int i=0;i<9;++i){char path[100];snprintf(path,sizeof(path),"/mnt/usb0/%d/%d.wav",i/3,i);assert(tracks.add(path,1000,i/3));}tracks.sort();TrackSequence q;
 assert(q.advance(tracks,0,1,false,0,true)==1);assert(q.advance(tracks,1,1,false,0,true)==2);assert(q.advance(tracks,2,1,false,0,true)==-1);assert(q.advance(tracks,2,1,false,1,true)==0);assert(q.advance(tracks,1,1,false,2,true)==1);assert(q.advance(tracks,0,-1,false,2,false)==2);assert(q.advance(tracks,2,1,false,2,false)==0);assert(q.advance(tracks,-1,1,false,1,true)==-1);
 for(int round=0;round<100;++round){TrackSequence shuffled;shuffled.random=round+1;std::set<int> visited;int current=0;visited.insert(current);
  for(int i=0;i<2;++i){current=shuffled.advance(tracks,current,1,true,0,true);assert(current>=0&&current<3&&visited.insert(current).second);}assert(shuffled.advance(tracks,current,1,true,0,true)==-1);
  int last=current;current=shuffled.advance(tracks,current,1,true,1,true);assert(current>=0&&current<3&&current!=last);
  for(int i=0;i<20;++i){current=shuffled.advance(tracks,current,1,true,1,true);assert(current>=0&&current<3);}assert(shuffled.advance(tracks,3,1,true,1,true)>=3);
 }
 for(unsigned p:{25u,80u,180u,255u}){int a=scrub(p,30),b=scrub(p,60),c=scrub(p,120);assert(a>0&&abs(a-b)<3500&&abs(b-c)<3500);printf("2sec pressure%u 30/60/120FPS scrub %d/%d/%d ms\n",p,a,b,c);}
 TriggerScrub input;assert(input.updateMs(0,24,100000)==0);assert(input.updateMs(255,255,100000)==0);int light=0;for(int i=0;i<60;++i)light+=input.updateMs(0,25,16667);assert(light>0&&light<1000);assert(input.updateMs(0,0,100000)==0);assert(input.updateMs(255,0,100000)<0);
 assert(seekTargetFrameMs(48000,480000,25)==49200);assert(seekTargetFrameMs(100,1000,-100)==0);assert(seekTargetFrameMs(900,1000,100)==1000);assert(seekTargetFrameMs(100,1000,std::numeric_limits<int64_t>::min())==0);assert(seekTargetFrameMs(100,1000,std::numeric_limits<int64_t>::max())==1000);assert(seekTargetFrame(48000,480000,2)==144000);
 puts("PASS folder queue,manual wrap,off/all/one,shuffle without repeats and folder crossing,subsecondpressure/release/reverse,seek bounds including signed extremes");}
