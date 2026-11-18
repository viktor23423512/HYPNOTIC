#pragma once
#include <algorithm>
#include <cmath>
// projectM BlurTexture range progression, with a finite minimum-width repair.
// The pinned source assigns max=avg-.05 as well as min=avg-.05 in collapsed
// cases, contradicting its minimum-gap comment and causing division by zero.
// Here max=avg+.05. This correction is explicit and tested.
struct BlurConfig {
 float min[3]={0,0,0},max[3]={1,1,1},scale[3]={1,1,1},bias[3]={0,0,0},edge=0;
 bool set(const double *lo,const double *hi,double darken){
  if(!std::isfinite(darken) || darken<0 || darken>1)return false;edge=(float)darken;
  for(unsigned i=0;i<3;++i){
   if(!std::isfinite(lo[i]) || !std::isfinite(hi[i]) || std::fabs(lo[i])>100 || std::fabs(hi[i])>100)return false;
   min[i]=(float)lo[i];max[i]=(float)hi[i];
   if(i){min[i]=std::max(min[i-1],min[i]);max[i]=std::min(max[i-1],max[i]);}
   if(max[i]-min[i]<.1f){const float average=(min[i]+max[i])*.5f;min[i]=average-.05f;max[i]=average+.05f;}
   const float width=max[i]-min[i];
   scale[i]=i?(max[i-1]-min[i-1])/width:1/width;
   bias[i]=i?-(min[i]-min[i-1])/width:-min[i]/width;
   if(!std::isfinite(scale[i]) || !std::isfinite(bias[i]))return false;
  }
  return true;
 }
};
