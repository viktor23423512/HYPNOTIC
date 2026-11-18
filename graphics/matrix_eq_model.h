#pragma once
#include "audio_spectrum.h"
#include <math.h>

struct MatrixEqSettings
{
    enum Layout {BassLeft,BassRight,BassCentre,BassOutside};
    Layout layout=BassCentre;
    bool flipped=false;
    int height=24,size=20,density=65,speed=135,fadeHalfSeconds=10,glow=55;
    bool demo=true;
    void adjust(int row,int direction)
    {
        if(row==0){layout=(Layout)(((int)layout+direction+4)%4);return;}
        if(row==1){flipped=!flipped;return;}
        row-=2;
        int *value=nullptr,low=0,high=100,step=1;
        switch(row)
        {
            case 0:value=&height;low=3;high=24;break;
            case 1:value=&size;low=9;high=24;break;
            case 2:value=&density;step=5;break;
            case 3:value=&speed;low=20;high=360;step=5;break;
            case 4:value=&fadeHalfSeconds;low=1;high=20;break;
            case 5:value=&glow;step=5;break;
            case 6:demo=!demo;return;
        }
        if(value) { *value+=direction*step; if(*value<low)*value=low; if(*value>high)*value=high; }
    }
    const char *layoutName() const
    {switch(layout){case BassLeft:return "BASS LEFT";case BassRight:return "BASS RIGHT";case BassCentre:return "MIRROR BASS CENTRE";default:return "MIRROR BASS OUTSIDE";}}
};

class MatrixEqModel
{
    uint32_t rng=0x4d415458;
    unsigned nextParticle=0,refreshCursor=0,liveCount=0;
    float refreshCredit=0;
    bool flipped=false;
    float random() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return (rng>>8)*(1.0f/16777216); }
    float range(float a,float b) { return a+(b-a)*random(); }
public:
    static constexpr unsigned maxColumns=128,maxParticles=1024,maxRows=25;
    static constexpr unsigned sourceStep=26,stripHeight=672,atlasHeight=stripHeight*2;
    struct Particle { float x=0,y=0,vy=0,age=0,life=0,size=0,brightness=0,mutate=0; unsigned glyph=0; bool pale=false,alive=false; } particles[maxParticles];
    float levels[maxColumns]={},emission[maxColumns]={};
    unsigned epochs[maxColumns]={};
    bool dirty[maxColumns]={};
    unsigned columns=0;
    float time=0;
    static float outwardPosition(unsigned column,unsigned count,MatrixEqSettings::Layout layout)
    {
        if(count<2)return 0;
        if(layout==MatrixEqSettings::BassLeft)return (float)column/(count-1);
        if(layout==MatrixEqSettings::BassRight)return 1-(float)column/(count-1);
        const unsigned depth=column<count-1-column?column:count-1-column;
        const unsigned half=(count-1)/2;
        const float outward=half?(float)(half-depth)/half:0;
        return layout==MatrixEqSettings::BassOutside?1-outward:outward;
    }
    static float displayBand(unsigned column,unsigned count,MatrixEqSettings::Layout layout=MatrixEqSettings::BassCentre)
    {
        const float outward=outwardPosition(column,count,layout);
        // Log bands below one 2048-point FFT bin reused the same sample across
        // a broad centre plateau. Space neighbouring displayed frequencies at
        // least one 48 kHz FFT bin apart, then curve toward the treble.
        const unsigned steps=(layout==MatrixEqSettings::BassLeft || layout==MatrixEqSettings::BassRight)?count-1:(count-1)/2;
        const float linear=steps*(48000.0f/2048);
        const float frequency=30+linear*outward+(16000-30-linear)*outward*outward*outward;
        float edge=20,next=edge*1.053611229f;unsigned band=0;
        while(band+1<AUDIO_SPECTRUM_BANDS && next<=frequency){edge=next;next*=1.053611229f;++band;}
        if(band==AUDIO_SPECTRUM_BANDS-1)return (float)band;
        return band+(frequency-edge)/(next-edge);
    }
    unsigned glyph(unsigned col,unsigned row) const { return (col*7+row*11+epochs[col])%46; }
    void update(const AudioSpectrum &spectrum,const MatrixEqSettings &s,float dt,bool paused)
    {
        unsigned desired=(unsigned)(1920+s.size+2)/(s.size+3);
        if(desired>maxColumns)desired=maxColumns;
        if(!(desired&1))--desired; // One central bass column, rather than a duplicated pair.
        if(desired!=columns)
        {
            columns=desired;
            for(unsigned i=0;i<maxColumns;++i) { levels[i]=emission[i]=0; epochs[i]=(unsigned)(time*2); dirty[i]=true; }
        }
        if(flipped!=s.flipped)
        {
            for(auto &p:particles)if(p.alive){p.y=1080-p.y;p.vy=-p.vy;}
            flipped=s.flipped;
        }
        if(paused) return;
        if(dt<0)dt=0; if(dt>0.05f)dt=0.05f;
        time+=dt;
        for(auto &p:particles) if(p.alive)
        {
            p.age+=dt; p.y+=p.vy*dt;
            if(p.age>=p.life || p.y>1112 || p.y< -32) { p.alive=false; --liveCount; continue; }
            if(p.age>=p.mutate) { p.glyph=(unsigned)(random()*46); p.mutate=p.age+range(0.15f,0.85f); }
        }
        refreshCredit+=columns*dt*2;
        unsigned changes=(unsigned)refreshCredit; if(changes>16)changes=16;
        refreshCredit-=changes;
        while(changes--) { const unsigned c=refreshCursor++%columns; epochs[c]=(unsigned)(time*2); dirty[c]=true; }
        const float scale=s.size/22.0f,step=sourceStep*scale;
        float demoBeat=sinf(time*12.56637061f); if(demoBeat<0)demoBeat=0;
        demoBeat*=demoBeat; demoBeat*=demoBeat; demoBeat*=demoBeat;
        float targets[maxColumns];
        for(unsigned i=0;i<columns;++i)
        {
            const float outward=outwardPosition(i,columns,s.layout);
            float target=0;
            if(spectrum.available)
            {
                const float pos=displayBand(i,columns,s.layout);
                const unsigned a=(unsigned)pos,b=a+1<AUDIO_SPECTRUM_BANDS?a+1:a;
                target=spectrum.bands[a]+(spectrum.bands[b]-spectrum.bands[a])*(pos-a);
            }
            else if(s.demo)
            {
                const float k=outward*(columns-1);
                const float a=sinf(time*2.6f+k*0.33f),b=sinf(time*5.1f+k*0.71f),c=sinf(time*1.4f+k*0.11f);
                target=0.12f+0.24f*a*a+0.18f*b*b+0.17f*c*c+demoBeat*(0.38f*(1-outward)+0.12f);
            }
            if(target>1)target=1; if(target<0)target=0;
            targets[i]=target;
        }
        if(s.layout==MatrixEqSettings::BassCentre)
        {
            const unsigned centre=columns/2;
            // Let the centre represent the nearby bass peak, not only the
            // quietest sub-bass sample. Neighbouring bars keep their own data.
            for(unsigned distance=1;distance<=2 && distance<=centre;++distance)
                if(targets[centre+distance]>targets[centre])targets[centre]=targets[centre+distance];
        }
        for(unsigned i=0;i<columns;++i)
        {
            const float target=targets[i];
            const float tau=target>levels[i]?0.045f:0.18f;
            levels[i]+=(target-levels[i])*(1-expf(-dt/tau));
            const unsigned count=1+(unsigned)(levels[i]*s.height+0.5f);
            const float tip=22+(count-1)*step;
            const float boost=spectrum.available && spectrum.peak>0.65f?1+(spectrum.peak-0.65f)*2.5f:1;
            const float rate=s.density*0.01f*(1.8f+levels[i]*7)*boost;
            emission[i]+=rate*dt;
            while(emission[i]>=1)
            {
                emission[i]-=1;
                if(liveCount==maxParticles) { emission[i]=0; break; }
                unsigned slot=maxParticles;
                for(unsigned n=0;n<maxParticles;++n) { const unsigned j=nextParticle++%maxParticles; if(!particles[j].alive) { slot=j; break; } }
                if(slot==maxParticles) break;
                Particle &p=particles[slot];
                p.x=(i+0.5f)*1920/columns; p.y=tip+s.size*0.6f;
                p.vy=s.speed*range(0.75f,1.3f); p.age=0;
                if(s.flipped){p.y=1080-p.y;p.vy=-p.vy;}
                p.life=s.fadeHalfSeconds*0.5f*range(0.65f,1.2f);
                p.size=s.size*range(0.8f,1.1f); p.brightness=range(0.55f,1);
                p.glyph=glyph(i,count-1); p.pale=random()>0.82f; p.alive=true; p.mutate=range(0.15f,0.85f);
                ++liveCount;
            }
        }
    }
};
