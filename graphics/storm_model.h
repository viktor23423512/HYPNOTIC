#pragma once
#include "audio_spectrum.h"

// Fixed-size, deterministic animation state. No audio-thread work or allocation.
class StormModel
{
    unsigned randomState=0x73ac4591;
    unsigned cloudFlashState=0x2c97a1e5;
    float diffuseCooldown=0;
    float previous[128]={},slowEnergy=0,cooldown=0;
    bool hadAudio=false;
    static float clamp(float v){return v<0?0:(v>1?1:v);}
    void segment(float x,float y,float xx,float yy,float width)
    {
        if(building->count<128)building->segments[building->count++]={x,y,xx,yy,width};
    }
    struct Point {float x,y;};
    void path(Point a,Point b,int depth,float width,bool taper)
    {
        Point points[65];const int count=1<<depth;
        points[0]=a;points[count]=b;
        for(int stride=count;stride>1;stride/=2)for(int i=0;i<count;i+=stride)
        {
            const Point p=points[i],q=points[i+stride];
            const float dx=q.x-p.x,dy=q.y-p.y,offset=(random()-.5f)*.48f;
            points[i+stride/2]={(p.x+q.x)*.5f-dy*offset,(p.y+q.y)*.5f+dx*offset};
        }
        for(int i=0;i<count;++i)segment(points[i].x,points[i].y,points[i+1].x,points[i+1].y,
            width*(taper?1-.72f*i/count:1));
    }
public:
    struct Segment {float x,y,xx,yy,width;};
    struct Bolt {Segment segments[128];int count=0;float age=1,power=0;bool front=false;};
    struct Drop {float x,y,speed,length,depth;};
    Bolt bolts[3];Drop drops[240];
    float time=0,flashAge=1,flashPower=0;
    float distantAge=3,distantPower=0;
    unsigned distantZone=0,distantFlashes=0;
    unsigned zone=1,strikes=0;
private:
    Bolt *building=nullptr;
public:
    float random(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return (randomState>>8)*(1.0f/16777216);}
    void init()
    {
        for(auto &d:drops)
        {
            d.x=random()*2080-80;d.y=random()*1240-80;d.depth=.25f+random()*.75f;
            d.speed=400+d.depth*850;d.length=8+d.depth*24;
        }
    }
    void strike(float power)
    {
        zone=(unsigned)(random()*3);flashAge=0;flashPower=.35f+power*.6f;++strikes;
        int slot=0;for(int i=1;i<3;++i)if(bolts[i].age>bolts[slot].age)slot=i;
        Bolt &bolt=bolts[slot];bolt.count=0;bolt.age=0;bolt.power=.55f+power*.45f;
        // Alternate guaranteed emerging bolts with internal sheet lightning.
        bolt.front=(strikes&1) || power>.72f;
        building=&bolt;
        float worldX=.17f+zone*.33f-time*.008f;
        while(worldX<0)worldX+=1;
        while(worldX>=1)worldX-=1;
        float x=worldX*1920;if(x<90)x=90;if(x>1830)x=1830;
        const Point origin={x+(random()-.5f)*120,180+random()*200};
        const bool across=random()<.28f;
        const Point end={origin.x+(random()-.5f)*(across?1250:600),across?origin.y+200+random()*180:920+random()*240};
        path(origin,end,6,1.4f,true);
        // Branches attach to the actual jagged trunk, never to an unrelated line.
        for(int b=0;b<5;++b)
        {
            const int source=8+(int)(random()*43);
            const Segment &s=bolt.segments[source];
            const float direction=random()<.5f?-1:1;
            const Point tip={s.x+direction*(100+random()*260),s.y+100+random()*240};
            path({s.x,s.y},tip,3,.65f-b*.065f,true);
        }
        building=nullptr;
    }
    static float brightness(const Bolt &b)
    {
        if(b.age>=.42f)return 0;
        if(b.age<.025f)return b.power;
        if(b.age<.055f)return b.power*.2f;
        if(b.age<.09f)return b.power*.85f;
        return b.power*.6f*expf(-(b.age-.09f)*20);
    }
    float flash()const{return flashPower*expf(-flashAge*9);}
    float distantFlash()const{return distantPower*expf(-distantAge*5);}
    void update(const AudioSpectrum &s,float gain,float dt,bool paused,bool rain)
    {
        if(paused)return;
        if(!(dt>=0))dt=0;if(dt>.05f)dt=.05f;
        // Both cloud-layer scroll rates complete whole tiles after 1000 s.
        time+=dt;if(time>1000)time-=1000;
        flashAge+=dt;if(flashAge>3)flashAge=3;
        distantAge+=dt;if(distantAge>3)distantAge=3;
        diffuseCooldown-=dt;
        for(auto &b:bolts){b.age+=dt;if(b.age>1)b.age=1;}
        cooldown-=dt;
        float flux=0,energy=0;
        for(unsigned i=0;i<128;++i)
        {
            const float value=s.available?clamp(s.bands[i]*gain):0;
            const float rise=value-previous[i];if(rise>0)flux+=rise;
            const float weight=i<48?1.6f:(i<92?1.0f:.4f);
            energy+=value*weight;previous[i]=value;
        }
        flux/=128;energy/=128;
        const float difference=energy-slowEnergy;
        slowEnergy+=(energy-slowEnergy)*(1-expf(-dt/.65f));
        // No random timed lightning on silent/missing input, and no repeated
        // triggering on a sustained loud note. At most one onset per 420 ms.
        bool majorStrike=false;
        if(dt>0 && s.available && cooldown<=0 && energy>.075f
            && (flux>.035f || (difference>.14f && flux>.012f) || (!hadAudio&&energy>.12f)))
        {
            strike(clamp(energy*.7f+flux*2.5f));cooldown=.42f;majorStrike=true;
        }
        if(dt>0 && s.available && !majorStrike && diffuseCooldown<=0 && energy>.075f && flux>.015f)
        {
            // Lower-energy musical changes create diffuse sheet lightning
            // only. Separate RNG preserves the existing branching bolt shapes.
            cloudFlashState^=cloudFlashState<<13;cloudFlashState^=cloudFlashState>>17;cloudFlashState^=cloudFlashState<<5;
            distantZone=cloudFlashState%3;distantPower=clamp(.2f+energy*.35f+flux*1.3f);
            distantAge=0;++distantFlashes;diffuseCooldown=.3f;
        }
        hadAudio=s.available;
        if(rain)for(auto &d:drops)
        {
            d.x-=d.speed*dt*.16f;d.y+=d.speed*dt;
            // Periodic motion keeps density stable and is independent of the
            // frame count. No RNG changes when a drop wraps at a screen edge.
            if(d.y>1160)d.y-=1240;
            if(d.x< -80)d.x+=2080;
        }
    }
};
