#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "audio_stereo_scope.h"
#include <math.h>

class GpuXyScope
{
    static constexpr unsigned history=64;
    struct Trace {float left[1024],right[1024],age=2,scale=440;unsigned count=0;} traces[history];
    GpuScene::Vertex vertices[1023*6];
    unsigned next=0;
    uint64_t sequence=0,epoch=0;
    float autoGain=1;
    static float clamp(float v){return v< -470?-470:v>470?470:v;}
    static void point(const Trace &t,unsigned i,bool midSide,float &x,float &y)
    {
        const float l=t.left[i],r=t.right[i];
        x=960+clamp((midSide?(l-r)*.5f:l)*t.scale);
        y=540-clamp((midSide?(l+r)*.5f:r)*t.scale);
    }
    unsigned mesh(const Trace &t,float width,bool midSide,unsigned stride)
    {
        unsigned used=0;
        for(unsigned start=0;start+1<t.count;start+=stride)
        {
            const unsigned end=start+stride<t.count?start+stride:t.count-1;
            float ax,ay,bx,by;point(t,start,midSide,ax,ay);point(t,end,midSide,bx,by);
            float dx=bx-ax,dy=by-ay;const float length=sqrtf(dx*dx+dy*dy);
            // A tiny square preserves a visible point for silence/constant input.
            float nx=width,ny=0;if(length>.001f){nx=-dy*width/length;ny=dx*width/length;}
            const float ey=length>.001f?0:width;
            auto out=vertices+used;used+=6;
            out[0]={ax+nx,ay+ny-ey,0,0};out[1]={bx+nx,by+ny+ey,0,0};out[2]={ax-nx,ay-ny-ey,0,0};
            out[3]=out[1];out[4]={bx-nx,by-ny+ey,0,0};out[5]=out[2];
        }
        return used;
    }
public:
    void draw(GpuScene *scene,const AudioStereoScope &audio,float gain,unsigned palette,bool paused,float dt,bool midSide=false,float persistence=.2f)
    {
        if(dt<0)dt=0;if(dt>.05f)dt=.05f;
        if(persistence<.2f)persistence=.2f;if(persistence>1.2f)persistence=1.2f;
        if(!paused)
        {
            for(auto &t:traces)t.age+=dt;
            if(audio.epoch!=epoch){for(auto &t:traces)t.count=0;epoch=audio.epoch;}
            if(audio.available && audio.count>1 && audio.sequence!=sequence)
            {
                sequence=audio.sequence;float peak=0;
                for(unsigned i=0;i<audio.count;++i){float a=fabsf(audio.left[i]),b=fabsf(audio.right[i]);if(a>peak)peak=a;if(b>peak)peak=b;}
                float desired=0.8f/(peak>.1f?peak:.1f);if(desired<1)desired=1;
                autoGain+=(desired-autoGain)*0.2f;
                auto &t=traces[next++%history];t.count=audio.count;t.age=0;t.scale=autoGain*gain*440;
                for(unsigned i=0;i<t.count;++i)
                {t.left[i]=audio.left[i];t.right[i]=audio.right[i];}
            }
        }
        const Color colors[4][3]={{{0,30,45},{0,180,220},{180,255,255}},{{35,0,40},{210,20,180},{255,190,255}},
            {{0,35,10},{30,210,60},{205,255,210}},{{40,18,0},{230,130,10},{255,240,180}}};
        const float widths[3]={5,1.8f,.65f};
        // Long persistence must not turn a correlated stereo track into an
        // opaque white stripe. Keep the two newest traces clear and cap the
        // combined exposure of the older phosphor history.
        float oldExposure=0;
        const float persistenceScale=persistence>.2f?sqrtf(.2f/persistence):1;
        for(unsigned i=0;i<history-2;++i)
        {
            const Trace &t=traces[(next+i)%history];
            if(t.count<2 || t.age>=persistence)continue;
            const float fade=1-t.age/persistence;oldExposure+=fade*fade*persistenceScale;
        }
        const float historyExposure=oldExposure>1?1/oldExposure:1;
        for(unsigned layer=0;layer<3;++layer)for(unsigned i=0;i<history;++i)
        {
            const Trace &t=traces[(next+i)%history];if(t.count<2 || t.age>=persistence)continue;
            float fade=1-t.age/persistence;fade*=fade;
            const unsigned rank=history-1-i,stride=rank<2?1:rank<6?4:8;
            if(rank>=2)fade*=persistenceScale*historyExposure;
            const unsigned count=mesh(t,widths[layer],midSide,stride);
            scene->drawSolidMesh(vertices,count,colors[palette%4][layer],fade*(layer==0?.4f:layer==1?.6f:.85f));
        }
    }
};
