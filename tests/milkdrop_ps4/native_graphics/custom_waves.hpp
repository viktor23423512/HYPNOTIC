#pragma once
#include "geometry_preset.hpp"
// projectM 4.1.8 CustomWaveform / WaveformPerFrameContext /
// WaveformPerPointContext conventions. Time-domain PCM or raw stereo spectrum. Fixed storage.
struct CustomWaves {
 std::shared_ptr<PresetMemory> sharedMemory;
 struct Context{
  projectm_eval_context *ctx=nullptr;projectm_eval_code *code=nullptr,*init=nullptr;PRJM_EVAL_F *v[60]={};
  ~Context(){if(code)projectm_eval_code_destroy(code);if(init)projectm_eval_code_destroy(init);if(ctx)projectm_eval_context_destroy(ctx);}
  bool create(const std::shared_ptr<PresetMemory>&memory){ctx=projectm_eval_context_create(memory->buffer,&memory->registers);if(!ctx)return false;const char *names[]={"time","fps","frame","bass","mid","treb","bass_att","mid_att","treb_att","r","g","b","a","samples","sample","value1","value2","x","y","progress"};for(unsigned i=0;i<20;++i)v[i]=projectm_eval_context_register_variable(ctx,names[i]);for(unsigned i=0;i<32;++i)v[20+i]=projectm_eval_context_register_variable(ctx,("q"+std::to_string(i+1)).c_str());for(unsigned i=0;i<8;++i)v[52+i]=projectm_eval_context_register_variable(ctx,("t"+std::to_string(i+1)).c_str());for(auto p:v)if(!p)return false;return true;}
  void inputs(const PresetEquations::Output &b){*v[0]=b.seconds;*v[1]=b.fps;*v[2]=b.frame;for(unsigned i=0;i<6;++i)*v[3+i]=b.audio[i];for(unsigned i=0;i<32;++i)*v[20+i]=b.q[i];}
 };
 struct Slot{bool enabled=false,dots=false,thick=false,additive=false,spectrum=false;float scaling=1,smoothing=.5f;double color[4]={1,1,1,1},afterInit[8]={};int samples=512,separation=0;Context frame,point;PresetMesh mesh;};
 Slot slots[4];unsigned enabled=0;float waveScale=1;std::string error;
 bool compile(Context &c,const std::string &source,bool init=false){if(source.empty())return true;auto code=projectm_eval_code_compile(c.ctx,source.c_str());if(!code){int line=0,col=0;const char *why=projectm_eval_get_error(c.ctx,&line,&col);error="Custom point equation L"+std::to_string(line)+": "+(why?why:"compile failed");return false;}if(init)c.init=code;else c.code=code;return true;}
 void reset(Slot &s,const PresetEquations::Output &base){projectm_eval_context_reset_variables(s.frame.ctx);projectm_eval_context_reset_variables(s.point.ctx);s.frame.inputs(base);for(unsigned i=0;i<4;++i)*s.frame.v[9+i]=s.color[i];*s.frame.v[13]=s.samples;if(s.frame.init)projectm_eval_code_execute(s.frame.init);for(unsigned i=0;i<8;++i)s.afterInit[i]=*s.frame.v[52+i];s.mesh.count=0;}
 void restart(const PresetEquations::Output &base){for(auto &s:slots)if(s.enabled)reset(s,base);}
 bool load(const char *path,const PresetEquations::Output &base,std::shared_ptr<PresetMemory> memory={}){libprojectM::MilkdropPreset::PresetFileParser p;if(!p.Read(std::string(path))){error="Cannot read custom point layers";return false;}sharedMemory=memory?memory:preparePresetMemory(p,error);if(!sharedMemory)return false;waveScale=p.GetFloat("fWaveScale",1);if(!std::isfinite(waveScale) || waveScale<0 || waveScale>100){error="Custom point global scale limits";return false;}
  for(unsigned i=0;i<4;++i){auto &s=slots[i];auto prefix="wavecode_"+std::to_string(i)+"_";s.enabled=p.GetBool(prefix+"enabled",false);if(!s.enabled)continue;++enabled;
   s.spectrum=p.GetBool(prefix+"bSpectrum",false);s.separation=p.GetInt(prefix+"sep",0);
   s.samples=p.GetInt(prefix+"samples",512);s.scaling=p.GetFloat(prefix+"scaling",1);s.smoothing=p.GetFloat(prefix+"smoothing",.5f);s.dots=p.GetBool(prefix+"bUseDots",false);s.thick=p.GetBool(prefix+"bDrawThick",false);s.additive=p.GetBool(prefix+"bAdditive",false);
   const char *colors[]={"r","g","b","a"};for(unsigned j=0;j<4;++j){s.color[j]=p.GetFloat(prefix+colors[j],1);if(!std::isfinite(s.color[j])){error="Non-finite point colour";return false;}}
   if(!std::isfinite(s.scaling) || fabs(s.scaling)>100 || !std::isfinite(s.smoothing) || s.smoothing<0 || s.smoothing>1 || s.samples<0 || s.samples>512){error="Custom point sample/scale/smoothing limits";return false;}
   if(!s.frame.create(sharedMemory) || !s.point.create(sharedMemory)){error="Custom point contexts allocation";return false;}auto codePrefix="wave_"+std::to_string(i);
   if(!compile(s.frame,p.GetCode(codePrefix+"_init"),true) || !compile(s.frame,p.GetCode(codePrefix+"_per_frame")) || !compile(s.point,p.GetCode(codePrefix+"_per_point")))return false;reset(s,base);
  }return true;
 }
 static float modulo(double d){float x=d,m=256.f/255.f;return fmodf(fmodf(x,m)+m,m);}
 bool build(const float *left,const float *right,const PresetEquations::Output &base,const float *spectrumLeft=nullptr,const float *spectrumRight=nullptr){if(!left || !right){error="Missing custom point PCM";return false;}
  for(auto &s:slots){s.mesh.count=0;if(!s.enabled)continue;s.frame.inputs(base);for(unsigned i=0;i<4;++i)*s.frame.v[9+i]=s.color[i];*s.frame.v[13]=s.samples;for(unsigned i=0;i<8;++i)*s.frame.v[52+i]=s.afterInit[i];if(s.frame.code)projectm_eval_code_execute(s.frame.code);
   if(!std::isfinite(*s.frame.v[13]) || fabs(*s.frame.v[13])>100000){error="Invalid point sample count";return false;}int count=std::min(512,std::max(0,(int)*s.frame.v[13]));if(count<2)continue;
   s.point.inputs(base);for(unsigned i=0;i<32;++i)*s.point.v[20+i]=*s.frame.v[20+i];for(unsigned i=0;i<8;++i)*s.point.v[52+i]=*s.frame.v[52+i];
   if(s.spectrum && (!spectrumLeft || !spectrumRight)){error="Missing raw stereo spectrum input";return false;}
   const float *inputLeft=s.spectrum?spectrumLeft:left,*inputRight=s.spectrum?spectrumRight:right;
   const int sep=std::min(count-1,std::max(0,s.separation));
   float l[512],r[512];const float scaling=s.spectrum?(512.f-sep)/count:count>480?480.f/count:1,previous=sqrtf(s.smoothing*.98f),current=1-previous,mult=s.scaling*waveScale*(s.spectrum?.15f:.004f),inputScale=s.spectrum?1.f:128.f;
   l[0]=inputLeft[0]*inputScale;r[0]=inputRight[0]*inputScale;for(int j=1;j<count;++j){unsigned at=(unsigned)(j*scaling);l[j]=inputLeft[at]*inputScale*current+l[j-1]*previous;r[j]=inputRight[at]*inputScale*current+r[j-1]*previous;}
   for(int j=count-2;j>=0;--j){l[j]=l[j]*current+l[j+1]*previous;r[j]=r[j]*current+r[j+1]*previous;}
   PresetVertex points[512];for(int j=0;j<count;++j){*s.point.v[14]=j*(1.f/(count-1));*s.point.v[15]=l[j]*mult;*s.point.v[16]=r[j]*mult;*s.point.v[17]=.5f+l[j]*mult;*s.point.v[18]=.5f+r[j]*mult;for(unsigned c=0;c<4;++c)*s.point.v[9+c]=*s.frame.v[9+c];if(s.point.code)projectm_eval_code_execute(s.point.code);
    for(unsigned k=9;k<=18;++k){const double value=*s.point.v[k],limit=s.dots && k>=17?1e30:100000;
     if(!std::isfinite(value) || fabs(value)>limit){error="Custom point numeric output: field="+std::to_string(k)+" sample="+std::to_string(j)+" value="+std::to_string(value);return false;}}
    auto &p=points[j];p.position[0]=(*s.point.v[17]*2-1);p.position[1]=(1-*s.point.v[18]*2)/.5625;p.position[2]=0;p.position[3]=1;for(unsigned c=0;c<4;++c)p.color[c]=modulo(*s.point.v[9+c]);
   }
   // GL_POINTS outside the clip volume cannot cover the viewport. Cull only
   // after smoothing so projected stars keep their original neighbours and
   // every point equation still executes. A conservative margin retains edge
   // dots. Huge finite positions never reach the GPU's dot expansion path.
   auto append=[&](float x,float y,const float *color){if(s.dots && (fabsf(x)>1.02f || fabsf(y)>1.02f))return true;return s.mesh.add(x,y,color);};
   for(int j=0;j<count;++j){const auto &p=points[j];if(!append(p.position[0],p.position[1],p.color)){error="Point vertex capacity";return false;}if(j==count-1)break;const auto &a=points[j?j-1:0],&b=points[j+1],&c=points[std::min(count-1,j+2)];float x=(-.15f*a.position[0]+1.15f*p.position[0]+1.15f*b.position[0]-.15f*c.position[0])*.5f,y=(-.15f*a.position[1]+1.15f*p.position[1]+1.15f*b.position[1]-.15f*c.position[1])*.5f;if(!append(x,y,p.color)){error="Smoothed point capacity";return false;}}
  }return true;
 }
};
