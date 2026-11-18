#pragma once
#include <projectm-eval.h>
#include "preset_memory.hpp"
#include <PresetFileParser.hpp>
#include <cmath>
#include <string>

// First renderer-adaptation seam: real projectM file parser and evaluator,
// with only the explicitly listed native output variables consumed so far.
// This is not a full projectM renderer or arbitrary preset compatibility.
class PresetEquations {
    projectm_eval_context *context=nullptr;
    projectm_eval_code *perFrame=nullptr,*initial=nullptr;
    PRJM_EVAL_F *time=nullptr,*fps=nullptr,*frame=nullptr;
    PRJM_EVAL_F *zoom=nullptr,*rot=nullptr,*decay=nullptr;
    PRJM_EVAL_F *red=nullptr,*green=nullptr,*blue=nullptr;
    PRJM_EVAL_F *pushX=nullptr,*pushY=nullptr,*centerX=nullptr,*centerY=nullptr,*waveX=nullptr,*waveY=nullptr,*mystery=nullptr;
    PRJM_EVAL_F *bass=nullptr,*mid=nullptr,*treb=nullptr;
    PRJM_EVAL_F *bassAtt=nullptr,*midAtt=nullptr,*trebAtt=nullptr;
    double defaults[17]={1,0,0.98,1,1,1,0,0,.5,.5,.5,.5,0,0,1,1,1};
    PRJM_EVAL_F *warpValue=nullptr,*stretchX=nullptr,*stretchY=nullptr,*volume=nullptr,*volumeAtt=nullptr,*waveAlpha=nullptr,*q[32]={};
    PRJM_EVAL_F *wrapParameter=nullptr;double wrapDefault=1;
    PRJM_EVAL_F *layerParameters[19]={};double layerDefaults[19]={};
    PRJM_EVAL_F *engineInputs[6]={},*zoomExp=nullptr;double zoomExpDefault=1;
    PRJM_EVAL_F *blurParameters[7]={};double blurDefaults[7]={0,1,0,1,0,1,0};
    PRJM_EVAL_F *finalParameters[5]={};double finalDefaults[5]={1,1,0,0,0};
    unsigned long frames=0;
    bool compilerError(const char *stage);
public:
    std::shared_ptr<PresetMemory> sharedMemory;
    std::string error;
    struct Output { double zoom,rot,decay,red,green,blue,dx,dy; double cx=.5,cy=.5,waveX=.5,waveY=.5,mystery=0;
        double blurMin[3]={0,0,0},blurMax[3]={1,1,1},blurEdge=0;
        double border[10]={},motion[9]={};
        double textureWrap=1;
        double zoomExponent=1;
        double waveAlpha=1,warpValue=0,stretchX=1,stretchY=1,q[32]={},audio[6]={},seconds=0,fps=60,frame=0;
        double gamma=1,echoZoom=1,echoAlpha=0,echoOrient=0,darkenCenter=0;
    } output={};
    Output initialOutput()const{Output value{};value.zoom=defaults[0];value.rot=defaults[1];value.decay=defaults[2];value.gamma=finalParameters[0]?*finalParameters[0]:1;value.echoZoom=finalParameters[1]?*finalParameters[1]:1;value.echoAlpha=finalParameters[2]?*finalParameters[2]:0;value.echoOrient=finalParameters[3]?*finalParameters[3]:0;value.darkenCenter=finalParameters[4]?*finalParameters[4]:0;value.textureWrap=wrapParameter?*wrapParameter:1;for(unsigned i=0;i<32;++i)value.q[i]=q[i]?*q[i]:0;for(unsigned i=0;i<3;++i){value.blurMin[i]=blurParameters[i*2]?*blurParameters[i*2]:0;value.blurMax[i]=blurParameters[i*2+1]?*blurParameters[i*2+1]:1;}value.blurEdge=blurParameters[6]?*blurParameters[6]:0;return value;}
    PresetEquations()=default;
    PresetEquations(const PresetEquations&)=delete;
    PresetEquations& operator=(const PresetEquations&)=delete;
    ~PresetEquations();
    void restart();
    bool load(const char *path,bool spatial=false,bool shaders=false);
    bool evaluate(double seconds,double framesPerSecond,double bassLevel=0,double midLevel=0,double trebleLevel=0,double bassAverage=-1,double midAverage=-1,double trebleAverage=-1);
};

// Independent equation checks also run on the console before GPU startup.
bool runEquationChecks(std::string &error);
