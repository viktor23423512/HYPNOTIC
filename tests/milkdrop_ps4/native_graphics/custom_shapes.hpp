#pragma once
#include "geometry_preset.hpp"
#include <vector>
#include "transform_limits.hpp"
// Native untextured custom shapes: four independent real equation contexts.
// Defaults/t-after-init/Q handoff follow projectM ShapePerFrameContext.
class CustomShapes {
public:
    std::shared_ptr<PresetMemory> sharedMemory;
    struct Slot {
        bool enabled=false,additive=false,thick=false;
        projectm_eval_context *context=nullptr;projectm_eval_code *init=nullptr,*code=nullptr;
        PRJM_EVAL_F *v[70]={};double defaults[21]={},t[8]={};
        PresetMesh fill,border;
        unsigned instances=1;PRJM_EVAL_F *instance=nullptr,*texZoom=nullptr,*texAngle=nullptr;double defaultTexZoom=1,defaultTexAngle=0;
        std::vector<std::array<double,23>> copies;
        ~Slot(){if(init)projectm_eval_code_destroy(init);if(code)projectm_eval_code_destroy(code);if(context)projectm_eval_context_destroy(context);}
        void inputs(const PresetEquations::Output &base){
            for(unsigned i=0;i<21;++i)*v[i]=defaults[i];
            *texZoom=defaultTexZoom;*texAngle=defaultTexAngle;
            *v[21]=base.seconds;*v[22]=base.fps;*v[23]=base.frame;
            for(unsigned i=0;i<6;++i)*v[24+i]=base.audio[i];
            for(unsigned i=0;i<32;++i)*v[30+i]=base.q[i];
            for(unsigned i=0;i<8;++i)*v[62+i]=t[i];
        }
        void restart(const PresetEquations::Output &base){
            if(!enabled)return;
            projectm_eval_context_reset_variables(context);
            for(auto &value:t)value=0;
            inputs(base);*instance=0;if(init)projectm_eval_code_execute(init);
            for(unsigned i=0;i<8;++i)t[i]=*v[62+i];
        }
    } slots[4];
    std::string error;unsigned enabled=0;
    bool load(const char *path,const PresetEquations::Output &initial,std::shared_ptr<PresetMemory> memory={}){
        libprojectM::MilkdropPreset::PresetFileParser p;if(!p.Read(std::string(path))){error="Cannot read custom shapes";return false;}
        sharedMemory=memory?memory:preparePresetMemory(p,error);if(!sharedMemory)return false;
        const char *properties[]={"x","y","rad","ang","sides","r","g","b","a","r2","g2","b2","a2","border_r","border_g","border_b","border_a","additive","thick","textured","num_inst"};
        const double defaults[]={.5,.5,.1,0,4,1,0,0,1,0,1,0,0,1,1,1,0,0,0,0,1};
        for(unsigned index=0;index<4;++index){
            auto &s=slots[index];const auto prefix="shapecode_"+std::to_string(index)+"_";
            s.enabled=p.GetBool(prefix+"enabled",false);if(!s.enabled)continue;++enabled;
            if(!p.GetString(prefix+"image", "").empty()){error="External custom shape image not bound yet: "+p.GetString(prefix+"image", "");return false;}
            const int count=p.GetInt(prefix+"num_inst",1);if(count<0 || count>1024){error="Custom shape instance limit (1024)";return false;}s.instances=(unsigned)count;s.copies.resize(s.instances);
            s.context=projectm_eval_context_create(sharedMemory->buffer,&sharedMemory->registers);if(!s.context){error="Shape context allocation";return false;}
            for(unsigned i=0;i<21;++i){s.defaults[i]=p.GetFloat(prefix+(i==18?"thickOutline":properties[i]),defaults[i]);s.v[i]=projectm_eval_context_register_variable(s.context,properties[i]);}
            const char *inputs[]={"time","fps","frame","bass","mid","treb","bass_att","mid_att","treb_att"};
            for(unsigned i=0;i<9;++i)s.v[21+i]=projectm_eval_context_register_variable(s.context,inputs[i]);
            for(unsigned i=0;i<32;++i)s.v[30+i]=projectm_eval_context_register_variable(s.context,("q"+std::to_string(i+1)).c_str());
            for(unsigned i=0;i<8;++i)s.v[62+i]=projectm_eval_context_register_variable(s.context,("t"+std::to_string(i+1)).c_str());
            // The original file instance count controls the loop, as in projectM.
            if(!(s.instance=projectm_eval_context_register_variable(s.context,"instance"))){error="Shape instance allocation";return false;}
            s.texZoom=projectm_eval_context_register_variable(s.context,"tex_zoom");s.texAngle=projectm_eval_context_register_variable(s.context,"tex_ang");s.defaultTexZoom=p.GetFloat(prefix+"tex_zoom",1);s.defaultTexAngle=p.GetFloat(prefix+"tex_ang",0);
            if(!s.texZoom || !s.texAngle){error="Shape texture variable allocation";return false;}
            for(auto value:s.v)if(!value){error="Shape variable allocation";return false;}
            const auto frame=p.GetCode("shape_"+std::to_string(index)+"_per_frame"),init=p.GetCode("shape_"+std::to_string(index)+"_init");
            if(!init.empty())s.init=projectm_eval_code_compile(s.context,init.c_str());
            if(!frame.empty())s.code=projectm_eval_code_compile(s.context,frame.c_str());
            if((!init.empty() && !s.init) || (!frame.empty() && !s.code)){int line=0,col=0;const char *why=projectm_eval_get_error(s.context,&line,&col);error="Shape "+std::to_string(index)+" L"+std::to_string(line)+": "+(why?why:"Equation compile failed");return false;}
            s.restart(initial);
        }
        return true;
    }
    void restart(const PresetEquations::Output &base){for(auto &s:slots)s.restart(base);}
    static float color(double value){
        // projectM color_modulo convention, bounded before conversion.
        const float x=(float)value;const float m=256.f/255.f;return fmodf(fmodf(x,m)+m,m);
    }
    static bool offscreen(const std::array<double,23> &v){
        // Test in double precision before narrowing far-off-screen positions.
        const double rx=fabs(v[2])*.5625*.5,ry=fabs(v[2])*.5,margin=.01;
        return v[0]+rx < -margin || v[0]-rx > 1+margin || v[1]+ry < -margin || v[1]-ry > 1+margin;
    }
    static bool geometry(Slot &s,const std::array<double,23> &v,std::string &error){
        s.fill.count=s.border.count=0;if(offscreen(v))return true;
            const unsigned sides=(unsigned)std::max(3.,std::min(100.,v[4]));
            const float x=v[0]*2-1,y=1-v[1]*2,radius=v[2],angle=v[3];
            float center[4],edge[4],border[4];
            for(unsigned i=0;i<4;++i){center[i]=color(v[5+i]);edge[i]=color(v[9+i]);border[i]=(float)v[13+i];}
            s.additive=(int)v[17]!=0;s.thick=(int)v[18]!=0;
            for(unsigned i=0;i<=sides;++i){
                const float a=angle+.7853981634f+(i%sides)*6.2831853072f/sides;
                const float xx=x+radius*cosf(a)*.5625f,yy=y+radius*sinf(a);
                if(v[16]>.0001 && !s.border.add(xx,yy,border)){error="Shape border capacity";return false;}
                if(i==sides)break;
                const float b=angle+.7853981634f+((i+1)%sides)*6.2831853072f/sides;
                if(!s.fill.add(x,y,center) || !s.fill.add(xx,yy,edge) || !s.fill.add(x+radius*cosf(b)*.5625f,y+radius*sinf(b),edge)){error="Shape fill capacity";return false;}
            }
        if(v[19]!=0){
            // Triangle fan emitted as independent triangles: centre, corner i,
            // corner i+1. UVs follow projectM's tex_ang/tex_zoom convention.
            for(unsigned i=0;i<sides;++i){
                auto &c=s.fill.vertices[i*3];c.position[2]=c.position[3]=.5f;
                for(unsigned j=0;j<2;++j){float a=(i+j)%sides*6.2831853072f/sides+(float)v[22]+.7853981634f;
                    auto &p=s.fill.vertices[i*3+1+j];p.position[2]=.5f+.5f*cosf(a)*.5625f/v[21];p.position[3]=.5f+.5f*sinf(a)/v[21];}
            }
        }
        return true;
    }
    bool build(const PresetEquations::Output &base){
        for(auto &s:slots){
            s.fill.count=s.border.count=0;if(!s.enabled)continue;
            for(unsigned copy=0;copy<s.instances;++copy){
                s.inputs(base);*s.instance=copy;if(s.code)projectm_eval_code_execute(s.code);
                auto &v=s.copies[copy];
                for(unsigned i=0;i<21;++i){v[i]=*s.v[i];if(!std::isfinite(v[i]) || (i>1 && std::fabs(v[i])>100000)){error="Non-finite/out-of-range custom shape output";return false;}}
                v[21]=*s.texZoom;v[22]=*s.texAngle;
                if(!std::isfinite(v[21]) || !std::isfinite(v[22]) || fabs(v[22])>100000 || (v[19]!=0 && (fabs(v[21])<1e-6 || fabs(v[21])>100000))){error="Invalid shape texture transform";return false;}
                if((!validPresetScalar(v[0]) || !validPresetScalar(v[1]) || !validPresetScalar(v[2])) && !offscreen(v)){error="Custom shape numeric budget: x="+std::to_string(v[0])+" y="+std::to_string(v[1])+" radius="+std::to_string(v[2]);return false;}
            }
            if(s.instances && !geometry(s,s.copies[0],error))return false;
        }
        return true;
    }
};
