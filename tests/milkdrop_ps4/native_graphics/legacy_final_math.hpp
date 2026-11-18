#pragma once
#include <algorithm>
#include <array>
#include <cmath>
// Reference math for projectM 4.1.8 VideoEcho/Filters normalized targets.
namespace LegacyFinalMath {
using Color=std::array<float,3>;
inline float q8(float x){return std::floor(std::max(0.f,std::min(1.f,x))*255.f+.5f)/255.f;}
inline Color addGamma(Color dst,Color source,float gain){for(unsigned i=0;i<8;++i){float w=std::max(0.f,std::min(1.f,gain-i));for(unsigned c=0;c<3;++c)dst[c]=q8(dst[c]+std::max(0.f,std::min(1.f,source[c]*w)));}return dst;}
inline Color filters(Color x,const bool enabled[4]){
 for(unsigned c=0;c<3;++c){float v=x[c];if(enabled[0]){v=q8(1-v);v=q8(v*v);v=q8(1-v);}if(enabled[1])v=q8(v*v);if(enabled[2]){v=q8(v*(1-v));v=q8(v+v);}if(enabled[3])v=q8(1-v);x[c]=v;}return x;
}
inline Color apply(Color image,Color echo,Color shade,float gamma,float alpha,const bool enabled[4]){
 Color a{},b{},out{};for(unsigned c=0;c<3;++c){a[c]=image[c]*shade[c];b[c]=echo[c]*shade[c];}
 if(alpha>.001f){for(unsigned c=0;c<3;++c){a[c]*=1-alpha;b[c]*=alpha;}out=addGamma(out,a,std::max(1.f,gamma));out=addGamma(out,b,std::max(1.f,gamma));}
 else out=addGamma(out,a,gamma);
 return filters(out,enabled);
}
inline void uv(float inU,float inV,float zoom,int orientation,float &u,float &v,float &eu,float &ev){
 u=(inU*512.f+.5f)/513.f;v=(inV*288.f+.5f)/289.f;eu=(u-.5f)/zoom+.5f;ev=(v-.5f)/zoom+.5f;
 orientation%=4;if(orientation%2==1)eu=1-eu;if(orientation>=2)ev=1-ev;
}
}
