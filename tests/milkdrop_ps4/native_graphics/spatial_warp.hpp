#pragma once
#include "geometry_preset.hpp"
#include "engine_inputs.hpp"
#include "transform_limits.hpp"

// Audited legacy per-pixel equations run on a fixed 16x10 mesh, like projectM's
// mesh-based per-pixel stage. GPU interpolates UVs and samples history at pixels.
// Extended imports retain per-pixel scratch state and frame Q inputs.
class SpatialWarp {
    std::shared_ptr<PresetMemory> sharedMemory;
    projectm_eval_context *context=nullptr;projectm_eval_code *code=nullptr;
    PRJM_EVAL_F *variables[63]={};
    float coordinates[187][2]={};
    bool extended=false;bool full=false;float warpAmount=0,warpSpeed=1,warpScale=1,zoomExponent=1;
public:
    static constexpr unsigned columns=16,rows=10;
    PresetMesh mesh;std::string error;bool enabled=false;
    ~SpatialWarp(){if(code)projectm_eval_code_destroy(code);if(context)projectm_eval_context_destroy(context);}
    bool load(const char *path,bool fullPreset=false,bool extendedPreset=false,std::shared_ptr<PresetMemory> memory={}){
        extended=extendedPreset;
        libprojectM::MilkdropPreset::PresetFileParser parser;
        if(!parser.Read(std::string(path))){error="Spatial preset file";return false;}
        full=fullPreset;warpAmount=parser.GetFloat("warp",0);warpSpeed=parser.GetFloat("fWarpAnimSpeed",1);warpScale=parser.GetFloat("fWarpScale",1);zoomExponent=parser.GetFloat("fZoomExponent",1);
        if(!std::isfinite(warpAmount) || !std::isfinite(warpSpeed) || !std::isfinite(warpScale) || !std::isfinite(zoomExponent) || warpScale<=0 || zoomExponent<=0){error="Stock warp parameters";return false;}
        const auto source=parser.GetCode("per_pixel_");enabled=full || extended || !source.empty();
        if(source.empty())return true;
        sharedMemory=memory?memory:preparePresetMemory(parser,error);if(!sharedMemory)return false;
        context=projectm_eval_context_create(sharedMemory->buffer,&sharedMemory->registers);if(!context){error="Spatial context";return false;}
        const char *names[]={"x","y","rad","ang","time","zoom","rot","dx","dy","warp"};
        for(unsigned i=0;i<10;++i){variables[i]=projectm_eval_context_register_variable(context,names[i]);if(!variables[i]){error="Spatial variable";return false;}}
        if(extended){
            const char *extra[]={"cx","cy","sx","sy","bass","mid","treb","bass_att","mid_att","treb_att","fps","frame","vol","vol_att"};
            for(unsigned i=0;i<14;++i)variables[10+i]=projectm_eval_context_register_variable(context,extra[i]);
            for(unsigned i=0;i<32;++i)variables[24+i]=projectm_eval_context_register_variable(context,("q"+std::to_string(i+1)).c_str());
            for(unsigned i=10;i<56;++i)if(!variables[i]){error="Spatial input allocation";return false;}
        }
        if(extended){for(unsigned i=0;i<6;++i)variables[56+i]=projectm_eval_context_register_variable(context,engineInputNames[i]);variables[62]=projectm_eval_context_register_variable(context,"zoomexp");for(unsigned i=56;i<63;++i)if(!variables[i]){error="Spatial engine input allocation";return false;}}
        code=projectm_eval_code_compile(context,source.c_str());
        if(!code){int line=0,col=0;const char *why=projectm_eval_get_error(context,&line,&col);error=why?why:"Spatial compilation";return false;}
        return true;
    }
    void restart(){if(context)projectm_eval_context_reset_variables(context);}
    // Aspect/origin conventions follow projectM PerPixelMesh and its stock warp
    // vertex shader. Native history images have top-left texture coordinates.
    static void sampleUV(float x,float y,const PresetEquations::Output &v,float dt,float &u,float &w){
        const float frames=dt*60;
        const float inverseZoom=powf((float)v.zoom,-frames),a=(float)v.rot*frames;
        const float px=x*.5f*inverseZoom,py=y*.5625f*.5f*inverseZoom;
        u=px*cosf(a)-py*sinf(a)-((float)v.dx*frames)+.5f;
        w=(px*sinf(a)+py*cosf(a)-((float)v.dy*frames))/.5625f+.5f;
    }
    void stockUV(float x,float y,const PresetEquations::Output &v,float time,float &u,float &w)const{
        // Original MilkDrop 2 milkdropfs.cpp CPU warp calculation (f05b0d).
        // powf accepts a negative base with an integral effective exponent.
        // Keep forward-power overflow -> zero reciprocal, as in the original.
        const float radius=hypotf(x,y*.5625f),z=powf((float)v.zoom,powf(extended?(float)v.zoomExponent:zoomExponent,radius*2-1));
        const float inverse=1.f/z;u=x*.5f*inverse+.5f;w=y*.5625f*.5f*inverse+.5f;
        if(extended){u=(u-(float)v.cx)/(float)v.stretchX+(float)v.cx;w=(w-(float)v.cy)/(float)v.stretchY+(float)v.cy;}
        const float amount=extended?(float)v.warpValue:warpAmount;
        const float wt=time*warpSpeed,inv=1.0f/warpScale;
        const float f[]={11.68f+4*cosf(wt*1.413f+10),8.77f+3*cosf(wt*1.113f+7),10.54f+3*cosf(wt*1.233f+3),11.49f+4*cosf(wt*.933f+5)};
        u+=amount*.0035f*sinf(wt*.333f+inv*(x*f[0]-y*f[3]));
        w+=amount*.0035f*cosf(wt*.375f-inv*(x*f[2]+y*f[1]));
        u+=amount*.0035f*cosf(wt*.753f+inv*(x*f[1]-y*f[2]));
        w+=amount*.0035f*sinf(wt*.825f+inv*(x*f[0]+y*f[3]));
        const float px=u-(float)v.cx,py=w-(float)v.cy,c=cosf((float)v.rot),s=sinf((float)v.rot);
        u=px*c-py*s+(float)v.cx;w=px*s+py*c+(float)v.cy;
        u-=(float)v.dx;w-=(float)v.dy;u=(u-.5f)+.5f;w=(w-.5f)*(1.f/.5625f)+.5f;
    }
    bool build(const PresetEquations::Output &base,float seconds,float dt,bool execute=true){
        if(!(dt>0) || !std::isfinite(dt) || dt>.1f || !std::isfinite(seconds)){error="Spatial frame time";return false;}
        // Upstream loads read-only inputs and frame Q once before row-major
        // evaluation. User scratch variables persist between vertices/frames;
        // only transform outputs reset per vertex. No per-vertex allocation.
        if(code && execute && extended){
            loadEngineInputs(variables+56,true);
            for(unsigned i=0;i<6;++i)*variables[14+i]=base.audio[i];
            *variables[4]=seconds;*variables[20]=base.fps;*variables[21]=base.frame;
            *variables[22]=(base.audio[0]+base.audio[1]+base.audio[2])/3;*variables[23]=(base.audio[3]+base.audio[4]+base.audio[5])/3;
            for(unsigned i=0;i<32;++i)*variables[24+i]=base.q[i];
        }
        for(unsigned row=0;row<=rows;++row)for(unsigned col=0;col<=columns;++col){
            const float x=(float)col/columns*2-1,y=(float)row/rows*2-1;
            auto output=base;
            if(code && execute){
                *variables[0]=x*.5f+.5f;*variables[1]=y*.5625f*.5f+.5f;
                *variables[2]=hypotf(x,y*.5625f);
                *variables[3]=(row==rows/2 && col==columns/2)?0:-atan2f(y*.5625f,x);
                if(!extended)*variables[4]=seconds;
                *variables[5]=base.zoom;*variables[6]=base.rot;
                *variables[7]=base.dx;*variables[8]=base.dy;*variables[9]=extended?base.warpValue:0;
                if(extended){
                    *variables[10]=base.cx;*variables[11]=base.cy;*variables[12]=base.stretchX;*variables[13]=base.stretchY;
                    *variables[62]=base.zoomExponent;
                }
                projectm_eval_code_execute(code);
                output.zoom=*variables[5];output.rot=*variables[6];output.dx=*variables[7];output.dy=*variables[8];
                if(extended){output.zoomExponent=*variables[62];if(!std::isfinite(output.zoomExponent) || output.zoomExponent<=0 || output.zoomExponent>100){error="Per-pixel zoom exponent limits";return false;}output.warpValue=*variables[9];output.cx=*variables[10];output.cy=*variables[11];output.stretchX=*variables[12];output.stretchY=*variables[13];
                    if(!validPresetScalar(output.warpValue) || !validPresetScalar(output.cx) || !validPresetScalar(output.cy) || !validPresetStretch(output.stretchX) || !validPresetStretch(output.stretchY)){error="Per-pixel warp/stretch limits";return false;}}
                if(!extended && *variables[9]!=0){error="Unsupported spatial warp term";return false;}
            }
            if(!validPresetZoom(output.zoom) || !validPresetScalar(output.rot) ||
               !validPresetScalar(output.dx) || !validPresetScalar(output.dy)){error="Spatial transform: row="+std::to_string(row)+" col="+std::to_string(col)+" zoom="+std::to_string(output.zoom)+" rot="+std::to_string(output.rot)+" dx="+std::to_string(output.dx)+" dy="+std::to_string(output.dy);return false;}
            auto &uv=coordinates[row*(columns+1)+col];
            if((full || extended) && execute)stockUV(x,y,output,seconds,uv[0],uv[1]);else sampleUV(x,y,output,dt,uv[0],uv[1]);
            if(!std::isfinite(uv[0]) || !std::isfinite(uv[1])){error="Spatial UV";return false;}
        }
        mesh.count=0;const float decay=powf(presetSurfaceDecay(base.decay),dt*60);
        if(!std::isfinite(decay) || decay<0 || decay>1){error="Spatial decay";return false;}
        for(unsigned row=0;row<rows;++row)for(unsigned col=0;col<columns;++col){
            const unsigned index=row*(columns+1)+col;
            const unsigned triangle[]={index,index+1,index+columns+1,index+1,index+columns+1,index+columns+2};
            for(unsigned at:triangle){
                const float x=(float)(at%(columns+1))/columns*2-1,y=(float)(at/(columns+1))/rows*2-1;
                const float data[]={coordinates[at][0],coordinates[at][1],decay,1};
                if(!mesh.add(x,-y,data)){error="Spatial mesh capacity";return false;}
                // These attributes are UVs, not colors: retain values outside
                // [0,1] so the pixel shader can wrap after interpolation.
                mesh.vertices[mesh.count-1].color[0]=data[0];mesh.vertices[mesh.count-1].color[1]=data[1];
                if(full && execute){mesh.vertices[mesh.count-1].color[2]=x*.5f+.5f;mesh.vertices[mesh.count-1].color[3]=y*.5f+.5f;}
            }
        }
        return mesh.count==960;
    }
};
