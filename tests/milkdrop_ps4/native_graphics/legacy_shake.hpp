#pragma once
#include "geometry_preset.hpp"
#include <cstring>
#include "standard_wave_math.hpp"

// Native adapter for the audited, unmodified Geiss centered-spiro preset bank.
// Geometry follows projectM 4.1.8 CenteredSpiro/WaveformMath conventions.
// The existing HYPNOTIC audio frontend is retained, not claimed bit-identical.
class LegacyShake {
    float scale=1,smoothing=0.9f,alpha=0.5f,x=0.5f,y=0.5f;
    bool maximize=true;float modStart=1,modEnd=1;StandardWaveMath math;
public:
    bool fixedFinal=false;bool finalFilters[4]={};
    bool shaderMode=false,dynamicAlpha=false;bool silent=false;bool thick=true,dots=false,additive=false,modulate=false;int mode=2;
    void restart(){memset(left,0,sizeof(left));memset(right,0,sizeof(right));}
private:
    float left[512]={},right[512]={};
public:
    PresetMesh mesh,secondMesh;
    std::string error;
    float gamma=1;
    bool load(const char *path,bool spatial=false,bool shaders=false,bool extended=false) {
        libprojectM::MilkdropPreset::PresetFileParser parser;
        if(!parser.Read(std::string(path))){error="Cannot read community preset";return false;}
        // This is a deliberately narrow, explicit feature profile. The file's
        // equations remain untouched; unsupported layers do not silently pass.
        shaderMode=shaders && extended;dynamicAlpha=extended;
        mode=parser.GetInt("nWaveMode",-1);silent=extended && parser.GetFloat("fWaveAlpha",0)>-0.00001f && parser.GetFloat("fWaveAlpha",0)<=0;
        if(!shaderMode && !silent && (mode<0 || mode>7)){error="Waveform mode "+std::to_string(mode)+" is not supported yet (available: time-domain modes 0-7)";return false;}
        if(!shaders && (!parser.GetCode("warp_").empty() || !parser.GetCode("comp_").empty())){error="Custom warp/composite shader needs bindings not available in this loader";return false;}
        if(!extended && !shaderMode && parser.GetFloat("fVideoEchoAlpha",0)!=0){error="Video echo is not supported yet (fVideoEchoAlpha)";return false;}
        dots=parser.GetBool("bWaveDots",false);additive=parser.GetBool("bAdditiveWaves",false);modulate=parser.GetBool("bModWaveAlphaByVolume",false);
        modStart=parser.GetFloat("fModWaveAlphaStart",1);modEnd=parser.GetFloat("fModWaveAlphaEnd",1);
        if(!std::isfinite(modStart) || !std::isfinite(modEnd)){error="Non-finite waveform alpha thresholds";return false;}
        if((!extended && !shaders && parser.GetFloat("warp",0)!=0) || (!spatial && !parser.GetCode("per_pixel_").empty())){error="Unsupported community preset profile";return false;}
        fixedFinal=extended && parser.GetCode("comp_").empty();
        const char *filters[]={"bBrighten","bDarken","bSolarize","bInvert"};for(unsigned i=0;i<4;++i)finalFilters[i]=parser.GetBool(filters[i],false);
        const char *disabled[]={"bDarkenCenter","bBrighten","bDarken","bSolarize","bInvert","bRedBlueStereo"};
        for(const char *name:disabled)if(parser.GetBool(name,false) && (std::string(name)=="bRedBlueStereo" || (!extended && (!shaderMode || std::string(name)=="bDarkenCenter")))){error=std::string("Final filter is not supported yet: ")+name;return false;}
        for(unsigned i=0;i<4;++i)if((!extended && parser.GetBool("shapecode_"+std::to_string(i)+"_enabled",false)) ||
            (!extended && parser.GetBool("wavecode_"+std::to_string(i)+"_enabled",false))){error="Custom wave/shape slot "+std::to_string(i)+" is not supported in this loader yet";return false;}
        const char *neutral[]={"sx","sy","fZoomExponent","cx","cy"};
        const float expected[]={1,1,1,0.5f,0.5f};
        for(unsigned i=0;i<5;++i)if(!extended && parser.GetFloat(neutral[i],expected[i])!=expected[i]){error="Unsupported warp center/stretch";return false;}
        if((!extended && !parser.GetBool("bTexWrap",true)) || (!extended && (parser.GetFloat("mv_a",0)!=0 ||
           (parser.GetFloat("ob_a",0)*parser.GetFloat("ob_size",0)!=0) ||
           (parser.GetFloat("ib_a",0)*parser.GetFloat("ib_size",0)!=0)))) {
            error="Unsupported border, vectors or edge mode";return false;
        }
        scale=parser.GetFloat("fWaveScale",1);smoothing=parser.GetFloat("fWaveSmoothing",0.9f);
        alpha=parser.GetFloat("fWaveAlpha",0.5f);x=parser.GetFloat("wave_x",0.5f);y=parser.GetFloat("wave_y",0.5f);
        maximize=parser.GetBool("bMaximizeWaveColor",true);gamma=parser.GetFloat("fGammaAdj",1);
        if(!std::isfinite(smoothing) || !std::isfinite(alpha) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(gamma) || !std::isfinite(scale) || scale<0 || scale>100 || smoothing<0 || smoothing>=1 ||
           (!extended && (gamma<=0 || gamma>4))) {error="Community parameter limits";return false;}
        thick=parser.GetBool("bWaveThick",false);restart();
        return true;
    }
    bool build(const float *rawLeft,const float *rawRight,const PresetEquations::Output &preset,float seconds=0) {
        if((dynamicAlpha && preset.waveAlpha<=0) || (silent && !dynamicAlpha)){mesh.count=secondMesh.count=0;return true;}
        if(mode<0 || mode>7){error="Active waveform mode "+std::to_string(mode)+" is not supported yet";return false;}
        const float frameAlpha=dynamicAlpha?(float)preset.waveAlpha:alpha;
        if(!rawLeft || !rawRight){error="No community PCM block";return false;}
        for(unsigned i=0;i<480;++i) {
            if(!std::isfinite(rawLeft[i]) || !std::isfinite(rawRight[i])){error="Non-finite PCM";return false;}
            left[i]=rawLeft[i]*128.0f;right[i]=rawRight[i]*128.0f;
        }
        // Upstream PCM uses normalized samples times 128; WaveformMath divides
        // by 128 during scaling. Retain those internal units so persistent tail
        // samples are treated identically on later frames, too.
        const float inputScale=scale/128.0f,mix=inputScale*(1-smoothing);
        left[0]*=inputScale;right[0]*=inputScale;
        for(unsigned i=1;i<512;++i) {
            left[i]=left[i]*mix+left[i-1]*smoothing;
            right[i]=right[i]*mix+right[i-1]*smoothing;
        }
        const float originX=(float)preset.waveX*2-1,originY=(float)preset.waveY*2-1;
        if(!math.generate(mode,left,right,originX,originY,(float)preset.mystery,seconds)){error="Unsupported waveform math";return false;}
        float opacity=frameAlpha;
        if(mode==2 || mode==5)opacity*=.09f;
        else if(mode==3)opacity*=.15f*1.3f*(float)(preset.audio[2]*preset.audio[2]);
        if(modulate){const float volume=(float)((preset.audio[0]+preset.audio[1]+preset.audio[2])/3);opacity=volume<=modStart?0:volume>=modEnd?frameAlpha:frameAlpha*(volume-modStart)/(modEnd-modStart);}
        opacity=std::max(0.f,std::min(1.f,opacity));
        float color[4]={(float)preset.red,(float)preset.green,(float)preset.blue,opacity};
        if(maximize) {
            const float maximum=std::max(color[0],std::max(color[1],color[2]));
            if(maximum>0.01f)for(unsigned i=0;i<3;++i)color[i]/=maximum;
        }
        mesh.count=secondMesh.count=0;
        const bool close=(mode==0 || mode==1) && !dots;
        auto smooth=[&](const StandardWaveMath::Points &points,PresetMesh &out){
            const unsigned n=points.count;if(!n)return true;
            for(unsigned i=0;i<n-1;++i){
                const unsigned below=i?i-1:0,above=i+1,above2=std::min(n-1,i+2);
                if(!out.add(points.data[i].x,-points.data[i].y,color))return false;
                const float x=(-.15f*points.data[below].x+1.15f*points.data[i].x+1.15f*points.data[above].x-.15f*points.data[above2].x)*.5f;
                const float y=(-.15f*points.data[below].y+1.15f*points.data[i].y+1.15f*points.data[above].y-.15f*points.data[above2].y)*.5f;
                if(!out.add(x,-y,color))return false;
            }
            if(!out.add(points.data[n-1].x,-points.data[n-1].y,color))return false;
            if(close && !out.add(points.data[0].x,-points.data[0].y,color))return false;
            return true;
        };
        if(!smooth(math.m_wave1Vertices,mesh) || !smooth(math.m_wave2Vertices,secondMesh)){error="Waveform vertex capacity or non-finite coordinate";return false;}
        return true;
    }
};

