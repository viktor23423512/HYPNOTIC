#pragma once
#include "audio_spectrum.h"

// Quadratic Julia-set boundary, computed through inverse iteration:
// z -> +/-sqrt(z-c). Both branches are retained, so the tree covers the set
// deterministically instead of joining unrelated samples into fake linework.
class FractalBloomModel
{
    unsigned randomState=0x4e97a3b1;
    bool seeded=false;
    static float clamp(float x){return x<0?0:(x>1?1:x);}
    static float band(const AudioSpectrum &s,unsigned first,unsigned end,float gain)
    {
        if(!s.available)return 0;
        float sum=0;for(unsigned i=first;i<end;++i)sum+=s.bands[i]*s.bands[i];
        return clamp(sqrtf(sum/(end-first))*gain);
    }
    static void follow(float &v,float target,float dt)
    {v+=(target-v)*(1-expf(-dt/(target>v?.12f:.8f)));}
public:
    struct Point {float x,y;};
    struct Flow {float x,y,warpX,warpY,frequency,twist,hue,offsetX,offsetY;};
    struct Journey {Flow from,to,value;float age,duration;unsigned style;};
    static constexpr unsigned count=16384,firstLeaf=count-1,treeSize=count*2-1;
    Point tree[treeSize*2];
    Journey journeys[2];
    float phase=0,bass=0,mids=0,treble=0,cx=-.4f,cy=.6f;
    bool ready=false;
private:
    float random()
    {randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return (randomState>>8)*(1.0f/16777216);}
    Flow destination(unsigned &style)
    {
        static const Point shapes[9]={{-.8f,.156f},{-.70176f,-.3842f},{-.4f,.6f},
            {.285f,.01f},{-.835f,-.232f},{-.123f,.745f},{.32f,.043f},{-.55f,-.48f},{-.11f,-.66f}};
        style=(style+1+(unsigned)(random()*8))%9;
        return {shapes[style].x+(random()-.5f)*.035f,shapes[style].y+(random()-.5f)*.035f,
            .07f+random()*.18f,.07f+random()*.18f,1.5f+random()*2.5f,
            (random()-.5f)*.7f,random()*3,(random()-.5f)*260,(random()-.5f)*200};
    }
    static Flow blend(const Flow &a,const Flow &b,float t)
    {
        const float u=t*t*t*(10+t*(-15+t*6));
        return {a.x+(b.x-a.x)*u,a.y+(b.y-a.y)*u,
            a.warpX+(b.warpX-a.warpX)*u,a.warpY+(b.warpY-a.warpY)*u,
            a.frequency+(b.frequency-a.frequency)*u,a.twist+(b.twist-a.twist)*u,
            a.hue+(b.hue-a.hue)*u,a.offsetX+(b.offsetX-a.offsetX)*u,a.offsetY+(b.offsetY-a.offsetY)*u};
    }
public:
    void seed(unsigned value)
    {
        randomState=value?value:0x4e97a3b1;seeded=true;ready=false;
        phase=bass=mids=treble=0;
        for(unsigned channel=0;channel<2;++channel)
        {
            auto &j=journeys[channel];j.style=channel*4;
            j.from=destination(j.style);j.to=destination(j.style);j.value=j.from;
            j.age=0;j.duration=3.2f+random()*2.8f;
        }
    }
    static Point root(Point z)
    {
        const float magnitude=sqrtf(z.x*z.x+z.y*z.y);
        if(magnitude<1e-20f)return {0,0};
        // Avoid cancellation in magnitude +/- real close to either axis.
        // Recover the smaller component through 2*real*imag = z.imag.
        if(z.x>=0)
        {
            const float a=sqrtf((magnitude+z.x)*.5f);
            return {a,z.y/(2*a)};
        }
        const float b=(z.y<0?-1.0f:1.0f)*sqrtf((magnitude-z.x)*.5f);
        return {z.y/(2*b),b};
    }
    void update(const AudioSpectrum &s,float gain,bool paused,float dt)
    {
        if(!seeded)seed(0x4e97a3b1);
        if(!(dt>=0))dt=0;if(dt>.05f)dt=.05f;
        if(!paused)
        {
            follow(bass,band(s,8,42,gain),dt);
            follow(mids,band(s,42,89,gain),dt);
            follow(treble,band(s,89,128,gain),dt);
            phase+=dt*(.4f+bass*.16f+mids*.12f);
            if(phase>6283.185307f)phase-=6283.185307f;
            for(auto &j:journeys)
            {
                j.age+=dt*(1+mids*.55f);
                if(j.age>=j.duration)
                {
                    j.age-=j.duration;j.from=j.to;j.to=destination(j.style);
                    j.duration=3.2f+random()*2.8f;
                }
                j.value=blend(j.from,j.to,j.age/j.duration);
            }
        }
        if(ready&&(paused||dt==0))return;
        for(unsigned channel=0;channel<2;++channel)
        {
            const Flow &f=journeys[channel].value;
            const float x=f.x+mids*.018f*sinf(phase*.7f+channel*2),
                y=f.y+treble*.018f*sinf(phase*.5f+channel*3);
            if(channel==0){cx=x;cy=y;}
            Point *branch=tree+channel*treeSize;
            const Point discriminant=root({1-4*x,-4*y});
            branch[0]={(1+discriminant.x)*.5f,discriminant.y*.5f};
            for(unsigned i=0;i<firstLeaf;++i)
            {
                const Point p=root({branch[i].x-x,branch[i].y-y});
                branch[i*2+1]=p;branch[i*2+2]={-p.x,-p.y};
            }
        }
        ready=true;
    }
    const Point *points(unsigned channel=0)const{return tree+channel*treeSize+firstLeaf;}
};
