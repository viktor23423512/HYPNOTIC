#pragma once
#include <stdint.h>
#include <stdio.h>
struct AudioOutputSettings {
 int volume=100;bool muted=false,eqEnabled=true;int bands[5]={};
 static int bound(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
 void adjustVolume(int direction){volume=bound(volume+direction*5,0,100);if(volume>0)muted=false;}
 void flat(){eqEnabled=true;for(int &g:bands)g=0;}
 bool isFlat()const{for(int g:bands)if(g)return false;return true;}
 uint64_t packed()const{uint64_t p=(uint64_t)bound(volume,0,100)|(uint64_t)muted<<7|(uint64_t)eqEnabled<<8;for(int i=0;i<5;++i)p|=(uint64_t)(bound(bands[i],-12,12)+12)<<(9+i*5);return p;}
 static AudioOutputSettings unpack(uint64_t p){AudioOutputSettings s;s.volume=bound(p&127,0,100);s.muted=(p>>7)&1;s.eqEnabled=(p>>8)&1;for(int i=0;i<5;++i)s.bands[i]=bound((int)((p>>(9+i*5))&31)-12,-12,12);return s;}
 bool decode(const char *value){int v,m,e,g[5];char extra;if(sscanf(value,"%d,%d,%d,%d,%d,%d,%d,%d%c",&v,&m,&e,&g[0],&g[1],&g[2],&g[3],&g[4],&extra)!=8||v<0||v>100||(m!=0&&m!=1)||(e!=0&&e!=1))return false;for(int n:g)if(n< -12||n>12)return false;volume=v;muted=m!=0;eqEnabled=e!=0;for(int i=0;i<5;++i)bands[i]=g[i];return true;}
};
