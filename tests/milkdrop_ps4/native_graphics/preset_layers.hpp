#pragma once
#include "geometry_preset.hpp"
// Geometry adapted from projectM 4.1.8 Border.cpp and MotionVectors.cpp /
// PresetMotionVectorsVertexShader. Fixed pools; no allocations during a frame.
struct PresetLayers {
 PresetMesh border;std::array<PresetMesh,6> vectors;unsigned vectorBatches=0;
 float previousUV[187][2]={};bool haveField=false;std::string error;
 void restart(){haveField=false;vectorBatches=0;border.count=0;}
 bool buildBorder(const PresetEquations::Output &v){
  border.count=0;for(double value:v.border)if(!std::isfinite(value) || fabs(value)>100000){error="Invalid border value";return false;}if(v.border[0]<0 || v.border[5]<0 || v.border[0]+v.border[5]>1){error="Border size limits";return false;}
  for(unsigned b=0;b<2;++b){const auto p=v.border+b*5;if(p[4]<=.001 || p[0]==0)continue;
   const float outer=b?1-v.border[0]:1,inner=outer-p[0];const float c[4]={(float)p[1],(float)p[2],(float)p[3],(float)p[4]};float x[4]={inner,outer,outer,inner},y[4]={inner,outer,-outer,-inner};
   for(unsigned rotation=0;rotation<4;++rotation){const unsigned at[]={0,1,2,0,2,3};for(unsigned i:at)if(!border.add(x[i],-y[i],c))return false;for(unsigned i=0;i<4;++i){const float old=x[i];x[i]=-y[i];y[i]=old;}}
  }return true;
 }
 void capture(const PresetMesh &mesh){
  haveField=mesh.count==960;if(!haveField)return;
  for(unsigned i=0;i<mesh.count;++i){const auto &v=mesh.vertices[i];const int col=(int)lroundf((v.position[0]+1)*8),row=(int)lroundf((1-v.position[1])*5);if(col<0 || col>16 || row<0 || row>10){haveField=false;return;}previousUV[row*17+col][0]=v.color[0];previousUV[row*17+col][1]=v.color[1];}
 }
 void sample(float x,float y,float &u,float &v)const{
  const float gx=std::max(0.f,std::min(16.f,x*16)),gy=std::max(0.f,std::min(10.f,y*10));const unsigned col=std::min(15u,(unsigned)gx),row=std::min(9u,(unsigned)gy);const float tx=gx-col,ty=gy-row;
  const auto a=previousUV[row*17+col],b=previousUV[row*17+col+1],c=previousUV[(row+1)*17+col],d=previousUV[(row+1)*17+col+1];
  float output[2];for(unsigned i=0;i<2;++i)output[i]=tx+ty<=1?a[i]+(b[i]-a[i])*tx+(c[i]-a[i])*ty:d[i]+(c[i]-d[i])*(1-tx)+(b[i]-d[i])*(1-ty);u=output[0];v=output[1];
 }
 bool buildVectors(const PresetEquations::Output &v){
  vectorBatches=0;for(auto &batch:vectors)batch.count=0;const auto m=v.motion;for(double value:v.motion)if(!std::isfinite(value) || fabs(value)>100000){error="Invalid motion vector value";return false;}if(!haveField || m[8]<.0001 || m[0]<1 || m[1]<1)return true;
  int nx=(int)m[0],ny=(int)m[1];float fx=m[0]-nx,fy=m[1]-ny;if(nx>64){nx=64;fx=0;}if(ny>48){ny=48;fy=0;}
  const float minimum=hypotf(1.25f/512,1.25f/288),color[4]={(float)m[5],(float)m[6],(float)m[7],(float)m[8]};unsigned batch=0;
  for(int y=0;y<ny;++y){float py=(y+.25f)/(ny+fy+.25f-1)-(float)m[3];if(py<=.0001 || py>=.9999)continue;
   for(int x=0;x<nx;++x){float px=(x+.25f)/(nx+fx+.25f-1)+(float)m[2];if(px<=.0001 || px>=.9999)continue;
    float u,w;sample(px,py,u,w);float dx=(u-px)*m[4],dy=(w-py)*m[4],length=hypotf(dx,dy);if(length<=minimum){if(length>1e-8){dx*=minimum/length;dy*=minimum/length;}else dx=dy=minimum;}
    if(vectors[batch].count==1024){if(++batch>=vectors.size()){error="Motion vector capacity";return false;}}
    if(!vectors[batch].add(px*2-1,1-py*2,color) || !vectors[batch].add((px+dx)*2-1,1-(py+dy)*2,color)){error="Non-finite vector geometry";return false;}
   }
  }vectorBatches=vectors[batch].count?batch+1:batch;return true;
 }
};
