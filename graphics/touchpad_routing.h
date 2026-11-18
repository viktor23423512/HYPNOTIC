#pragma once
#include <stdint.h>
struct TouchpadRouting {
 unsigned width=1920;uint64_t last=0;bool seen=false,left=false;
 void reset(){seen=false;last=0;}
 void setWidth(unsigned value){if(value>=100&&value<=8192)width=value;}
 void observe(unsigned fingers,unsigned x,uint64_t now){if(fingers>=1&&fingers<=2&&x<width){left=x<width/2;last=now;seen=true;}}
 // Remote Play may supply a click without coordinates: default to Music.
 bool searchSide(uint64_t now)const{return seen&&now>=last&&now-last<=250000&&left;}
};
