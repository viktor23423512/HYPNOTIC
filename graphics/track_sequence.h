#pragma once
#include "track_library.h"
struct TrackSequence {
 int order[TrackLibrary::capacity]={},count=0,position=0,folder=-2;bool shuffled=false;unsigned random=0x4859504e;
 unsigned nextRandom(){random^=random<<13;random^=random>>17;random^=random<<5;return random;}
 void mix(){for(int i=count-1;i>1;--i){int j=1+nextRandom()%i;int t=order[i];order[i]=order[j];order[j]=t;}}
 void prepare(const TrackLibrary &tracks,int current,bool shuffle){
  if(current<0 || current>=tracks.count){count=0;return;}
  const int parent=tracks.folder[current];
  if(folder!=parent || shuffled!=shuffle || !count){folder=parent;shuffled=shuffle;count=0;
   for(int i=0;i<tracks.count;++i)if(tracks.folder[i]==parent)order[count++]=i;
   if(shuffle){for(int i=0;i<count;++i)if(order[i]==current){int t=order[0];order[0]=current;order[i]=t;break;}mix();}
  }
  for(int i=0;i<count;++i)if(order[i]==current){position=i;return;}
 }
 // repeat:0off,1all,2one. Manual stepping always moves/wraps.
 int advance(const TrackLibrary &tracks,int current,int delta,bool shuffle,int repeat,bool automatic){
  prepare(tracks,current,shuffle);if(!count)return -1;if(automatic && repeat==2)return current;
  int next=position+delta;if(automatic && next>=count && repeat==0)return -1;
  if(automatic && next>=count && shuffle && repeat==1 && count>1){int last=order[position];order[position]=order[0];order[0]=last;mix();position=0;return order[++position];}
  next%=count;if(next<0)next+=count;position=next;return order[position];
 }
};
