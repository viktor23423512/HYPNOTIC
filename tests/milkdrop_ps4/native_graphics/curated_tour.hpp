#pragma once
#include <cstdint>
// Loading is outside the measurement window. The tour logs before any parser or
// GPU work, so the last BEGIN identifies an interrupted/crashed candidate.
struct CuratedTour {
 unsigned queue[192]={},queued=0,cursor=0;
 bool running=false,pending=false,measuring=false;
 unsigned next=0,end=0,completed=0,rejected=0,frames=0,audioFrames=0,samples=0;
 uint64_t began=0,measureAt=0,nextSample=0,firstSequence=0,lastSequence=0;
 double peak=0,litMin=1,litMax=0,meanMin=255,meanMax=0;
 void start(unsigned selected,unsigned first,unsigned count){
  queued=cursor=0;running=true;pending=true;measuring=false;next=selected>=first?selected:first;
  end=count;completed=rejected=0;
 }
 bool startList(const unsigned *items,unsigned count){
  if(!items || !count || count>192)return false;
  for(unsigned i=0;i<count;++i){queue[i]=items[i];for(unsigned j=0;j<i;++j)if(items[i]==items[j])return false;}
  queued=count;cursor=0;next=queue[0];end=count;completed=rejected=0;running=pending=true;measuring=false;return true;
 }
 void stop(){running=pending=measuring=false;}
 void loaded(uint64_t now){
  pending=false;measuring=true;began=now;measureAt=now+2000000;nextSample=measureAt;
  frames=audioFrames=samples=0;firstSequence=lastSequence=0;peak=0;litMin=1;litMax=0;meanMin=255;meanMax=0;
 }
 bool due(uint64_t now)const{return running && measuring && now-began>=12000000;}
 void finish(bool rejectedPreset){
  if(rejectedPreset)++rejected;else ++completed;
  measuring=false;
  if(queued){if(++cursor>=queued)stop();else{next=queue[cursor];pending=true;}}
  else{++next;if(next>=end)stop();else pending=true;}
 }
};
