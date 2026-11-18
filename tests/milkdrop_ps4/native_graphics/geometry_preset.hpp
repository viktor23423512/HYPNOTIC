#pragma once
#include "preset_equations.hpp"
#include <array>
#include <algorithm>

struct PresetVertex { float position[4]; float color[4]; };
static_assert(sizeof(PresetVertex)==32,"std140 geometry vertex layout");
struct PresetMesh {
    std::array<PresetVertex,1024> vertices{};
    unsigned count=0;
    bool add(float x,float y,const float color[4]) {
        if(count>=vertices.size() || !std::isfinite(x) || !std::isfinite(y))return false;
        auto &v=vertices[count++];v.position[0]=x;v.position[1]=y;v.position[2]=0;v.position[3]=1;
        for(unsigned c=0;c<4;++c)v.color[c]=std::max(0.0f,std::min(1.0f,color[c]));
        return true;
    }
};

// Bounded adapter for custom wave 0 per-point and untextured shape 0 per-frame
// equations. Uses real projectM syntax/evaluation, not the full projectM renderer.
class GeometryPreset {
    struct Block {
        projectm_eval_context *context=nullptr;
        projectm_eval_code *code=nullptr;
        std::array<PRJM_EVAL_F*,32> values{};
        ~Block(){if(code)projectm_eval_code_destroy(code);if(context)projectm_eval_context_destroy(context);}
        bool create(const char *const *names,unsigned count) {
            context=projectm_eval_context_create(nullptr,nullptr);if(!context)return false;
            for(unsigned i=0;i<count;++i) {
                values[i]=projectm_eval_context_register_variable(context,names[i]);
                if(!values[i])return false;
            }
            return true;
        }
        bool compile(const std::string &text) {
            if(text.empty())return false;
            code=projectm_eval_code_compile(context,text.c_str());return code!=nullptr;
        }
        void set(unsigned i,double value){*values[i]=value;}
        double get(unsigned i)const{return *values[i];}
        void run(){projectm_eval_code_execute(code);}
    } wave,shape;
    float shapeDefaults[14]={};
    float waveColor[4]={};
public:
    std::string error;
    PresetMesh mesh;
    bool load(const char *path) {
        libprojectM::MilkdropPreset::PresetFileParser parser;
        if(!parser.Read(std::string(path))){error="Cannot read geometry preset";return false;}
        if(!parser.GetBool("wavecode_0_enabled",false) || !parser.GetBool("shapecode_0_enabled",false) ||
           parser.GetBool("shapecode_0_textured",false) || parser.GetInt("shapecode_0_num_inst",1)!=1 ||
           parser.GetInt("wavecode_0_samples",64)!=64 || parser.GetBool("wavecode_0_bSpectrum",false) ||
           !parser.GetCode("wave_0_per_frame").empty() || !parser.GetCode("wave_0_init").empty()) {
            error="Unsupported geometry fixture options";return false;
        }
        const char *waveNames[]={"time","sample","value1","value2","x","y","r","g","b","a","bass","mid","treb"};
        const char *shapeNames[]={"time","x","y","rad","ang","sides","r","g","b","a","r2","g2","b2","a2","bass","mid","treb"};
        if(!wave.create(waveNames,13) || !shape.create(shapeNames,17)){error="Geometry context allocation";return false;}
        if(!wave.compile(parser.GetCode("wave_0_per_point")) || !shape.compile(parser.GetCode("shape_0_per_frame"))) {
            error="Geometry equation compilation";return false;
        }
        const char *properties[]={"x","y","rad","ang","sides","r","g","b","a","r2","g2","b2","a2","bass","mid","treb"};
        const float defaults[]={0.5f,0.5f,0.25f,0,6,1,0.2f,0.5f,0.3f,0.3f,0.1f,1,0.5f};
        for(unsigned i=0;i<13;++i)shapeDefaults[i+1]=parser.GetFloat(std::string("shapecode_0_")+properties[i],defaults[i]);
        for(unsigned i=0;i<4;++i)waveColor[i]=parser.GetFloat(std::string("wavecode_0_")+waveNames[6+i],1);
        // This initial adapter deliberately rejects shape init/q/t-variable
        // sharing rather than implying the remaining engine semantics work.
        if(!parser.GetCode("shape_0_init").empty()){error="Shape init not implemented in this adapter";return false;}
        return true;
    }
    bool build(double seconds,const float *pcmLeft=nullptr,const float *pcmRight=nullptr,unsigned pcmCount=0,
               double bassLevel=0,double midLevel=0,double trebleLevel=0) {
        if(!std::isfinite(seconds)){error="Invalid geometry time";return false;}
        mesh.count=0;
        float points[64][2],colors[64][4];
        for(unsigned i=0;i<64;++i) {
            const double sample=i/63.0;
            // Deliberately labelled synthetic PCM until audio is connected.
            const double syntheticLeft=0.65*std::sin(sample*12.566370614359172+seconds*1.3)+
                              0.25*std::sin(sample*31.41592653589793-seconds*0.8);
            const double syntheticRight=0.75*std::sin(sample*18.84955592153876-seconds*0.9);
            const unsigned pcmIndex=pcmCount?i*(pcmCount-1)/63:0;
            const double left=pcmLeft && pcmCount?std::max(-1.0f,std::min(1.0f,pcmLeft[pcmIndex]*3)):syntheticLeft;
            const double right=pcmRight && pcmCount?std::max(-1.0f,std::min(1.0f,pcmRight[pcmIndex]*3)):syntheticRight;
            wave.set(10,bassLevel);wave.set(11,midLevel);wave.set(12,trebleLevel);
            wave.set(0,seconds);wave.set(1,sample);wave.set(2,left);wave.set(3,right);
            wave.set(4,sample);wave.set(5,0.5);
            for(unsigned c=0;c<4;++c)wave.set(6+c,waveColor[c]);
            wave.run();
            for(unsigned v=4;v<10;++v)if(!std::isfinite(wave.get(v))){error="Non-finite wave output";return false;}
            if(std::fabs(wave.get(4))>2 || std::fabs(wave.get(5))>2){error="Wave output range";return false;}
            points[i][0]=(float)(wave.get(4)*2-1);points[i][1]=(float)(1-wave.get(5)*2);
            for(unsigned c=0;c<4;++c)colors[i][c]=(float)wave.get(6+c);
        }
        // Triangulate line segments at a fixed 2-pixel width in the 512x288
        // diagnostic target. All vertices live in the fixed mesh array.
        for(unsigned i=0;i<63;++i) {
            const float dx=(points[i+1][0]-points[i][0])*256;
            const float dy=(points[i+1][1]-points[i][1])*144;
            const float length=std::sqrt(dx*dx+dy*dy);
            if(length<0.0001f)continue;
            const float nx=-dy/length/256,ny=dx/length/144;
            const float x=points[i][0],y=points[i][1],xx=points[i+1][0],yy=points[i+1][1];
            if(!mesh.add(x+nx,y+ny,colors[i]) || !mesh.add(x-nx,y-ny,colors[i]) || !mesh.add(xx+nx,yy+ny,colors[i+1]) ||
               !mesh.add(xx+nx,yy+ny,colors[i+1]) || !mesh.add(x-nx,y-ny,colors[i]) || !mesh.add(xx-nx,yy-ny,colors[i+1])) {
                error="Wave vertex capacity";return false;
            }
        }
        shape.set(0,seconds);for(unsigned i=1;i<14;++i)shape.set(i,shapeDefaults[i]);
        shape.set(14,bassLevel);shape.set(15,midLevel);shape.set(16,trebleLevel);
        shape.run();
        for(unsigned i=1;i<14;++i)if(!std::isfinite(shape.get(i))){error="Non-finite shape output";return false;}
        if(shape.get(5)<3 || shape.get(5)>12 || shape.get(3)<0 || shape.get(3)>1 ||
           std::fabs(shape.get(1))>2 || std::fabs(shape.get(2))>2 || std::fabs(shape.get(4))>10000) {
            error="Shape output range";return false;
        }
        const unsigned sides=(unsigned)shape.get(5);
        const float x=(float)(shape.get(1)*2-1),y=(float)(1-shape.get(2)*2);
        const float radius=(float)shape.get(3),angle=(float)shape.get(4);
        float center[4],edge[4];
        for(unsigned c=0;c<4;++c){center[c]=(float)shape.get(6+c);edge[c]=(float)shape.get(10+c);}
        for(unsigned i=0;i<sides;++i) {
            const float a=angle+0.7853981634f+i*6.2831853072f/sides;
            const float b=angle+0.7853981634f+(i+1)*6.2831853072f/sides;
            if(!mesh.add(x,y,center) ||
               !mesh.add(x+radius*std::cos(a)*(288.0f/512),y+radius*std::sin(a),edge) ||
               !mesh.add(x+radius*std::cos(b)*(288.0f/512),y+radius*std::sin(b),edge)) {
                error="Shape vertex capacity";return false;
            }
        }
        return mesh.count>0 && mesh.count%3==0;
    }
};
