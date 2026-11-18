#pragma once
#include <cstdint>
// Diagnostic modes preserve the rendering/equation algorithms. Only the
// explicitly selected custom-point work is bypassed. No quality reduction.
struct PerformanceProbe {
 unsigned mode=0,step=0,samples=0;bool automatic=false;uint64_t next=0,reportAt=0,total[10]={};
 static const char *name(unsigned m){const char *names[]={"NORMAL","EVAL ONLY","DRAW FROZEN","POINTS OFF"};return names[m%4];}
 bool evaluate()const{return mode<2;}bool draw()const{return mode==0 || mode==2;}
 void resetStats(uint64_t now){samples=0;for(auto &v:total)v=0;reportAt=now;}
 void manual(uint64_t now){automatic=false;mode=(mode+1)%4;resetStats(now);}
 void start(uint64_t now){automatic=true;mode=step=0;next=now+4000000;resetStats(now);}
 bool tick(uint64_t now){if(!automatic || now<next)return false;if(++step==4){automatic=false;mode=0;}else{mode=step;next=now+4000000;}resetStats(now);return true;}
 void normal(uint64_t now){mode=0;automatic=false;resetStats(now);}
 void add(const uint64_t *values){for(unsigned i=0;i<10;++i)total[i]+=values[i];++samples;}
};
