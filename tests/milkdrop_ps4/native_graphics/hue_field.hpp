#pragma once
#include <cmath>
#include <algorithm>
// projectM 4.1.8 FinalComposite::ApplyHueShaderColors (LGPL-2.1),
// caller-supplied persistent preset offsets. Four RGBA corner uniforms.
inline void presetHueCorners(double time,const float offsets[4],float out[16]){
 for(unsigned i=0;i<4;++i){float *c=out+i*4;
  c[0]=.6f+.3f*sinf(time*30.f*.0143f+3.f+i*21.f+offsets[3]);
  c[1]=.6f+.3f*sinf(time*30.f*.0107f+1.f+i*13.f+offsets[1]);
  c[2]=.6f+.3f*sinf(time*30.f*.0129f+6.f+i*9.f+offsets[2]);
  const float maximum=std::max(c[0],std::max(c[1],c[2]));for(unsigned j=0;j<3;++j)c[j]=.5f+.5f*c[j]/maximum;c[3]=1;
 }
}
