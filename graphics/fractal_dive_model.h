#pragma once
#include <math.h>
#include <stdint.h>

// A random recursive fractal: each child has its own repeatable seed and finer
// descendants. Re-rooting into a child keeps numeric coordinates small forever.
class FractalDiveModel
{
    unsigned state=0x6a25be91;
    unsigned choose()
    {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
    void chooseTarget(){selected=choose()%childCount(rootSeed);target=child(rootSeed,selected);}
public:
    struct Child {unsigned seed;float x,y,scale;};
    struct Ancestor {unsigned seed,selected;float x,y,scale;};
    Ancestor ancestors[5];unsigned ancestorCount=0,rootSeed=0,selected=0,levels=0;
    Child target={};float zoom=1,cameraX=0,cameraY=0;
    bool ready=false;
    static unsigned hash(unsigned v)
    {v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
    static float unit(unsigned v){return (hash(v)>>8)*(1.0f/16777216);}
    static unsigned childCount(unsigned seed){return 4+hash(seed^0xa9375u)%3;}
    static Child child(unsigned seed,unsigned index)
    {
        const unsigned key=hash(seed^(index+1)*0x9e3779b9u);
        const float angle=6.283185307f*(unit(seed)+index/(float)childCount(seed))+(unit(key+3)-.5f)*.18f;
        const float radius=.47f+unit(key+7)*.13f;
        return {key,radius*sinf(angle+1.570796327f),radius*sinf(angle),.26f+unit(key+13)*.07f};
    }
    void seed(unsigned value)
    {
        state=value?value:0x6a25be91;rootSeed=hash(choose());ancestorCount=levels=0;
        zoom=1;cameraX=cameraY=0;ready=true;chooseTarget();
    }
    void enterChild()
    {
        for(unsigned i=0;i<ancestorCount;++i)
        {
            auto &a=ancestors[i];a.x=(a.x-target.x)/target.scale;
            a.y=(a.y-target.y)/target.scale;a.scale/=target.scale;
        }
        if(ancestorCount==5){for(unsigned i=1;i<5;++i)ancestors[i-1]=ancestors[i];--ancestorCount;}
        ancestors[ancestorCount++]={rootSeed,selected,-target.x/target.scale,-target.y/target.scale,1/target.scale};
        zoom*=target.scale;rootSeed=target.seed;++levels;chooseTarget();
    }
    void update(float dt,bool paused,float speed)
    {
        if(!ready)seed(0x6a25be91);
        if(paused)return;
        if(!(dt>=0))dt=0;if(dt>.05f)dt=.05f;
        if(speed<.5f)speed=.5f;if(speed>2)speed=2;
        zoom*=expf(dt*.2f*speed);
        if(zoom>=1/target.scale)enterChild();
        float t=logf(zoom)/logf(1/target.scale);if(t<0)t=0;if(t>1)t=1;
        const float ease=t*t*t*(10+t*(-15+t*6));
        cameraX=target.x*ease;cameraY=target.y*ease;
    }
};
