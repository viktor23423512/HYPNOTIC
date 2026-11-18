#pragma once
#include "audio_spectrum.h"
#include <math.h>

// A damped sand transport model over a small standing-wave field. Music excites
// two dominant modes; the grains seek low vibration, rather than tracing shapes.
class CymaticsModel
{
    uint32_t rng=0x53414e44;
    bool initialized=false;
    float basis[4][6][96*64],field[96*64],gx[96*64],gy[96*64];
    float weights[6]={1,0,0,0,0,0},energy=0,envelope=0,cooldown=0;
    float families[4]={0,1,0,0};
    float accumulator=0,previousLevels[6]={},fluxEnvelope=0;
    unsigned bursts=0;
    float random() { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (rng>>8)*(1.0f/16777216); }
    static float sample(const float *grid,float x,float y)
    {
        const float px=x*95,py=y*63;
        unsigned ix=(unsigned)px,iy=(unsigned)py;if(ix>94)ix=94;if(iy>62)iy=62;
        const float fx=px-ix,fy=py-iy;const unsigned at=iy*96+ix;
        return (grid[at]*(1-fx)+grid[at+1]*fx)*(1-fy)+(grid[at+96]*(1-fx)+grid[at+97]*fx)*fy;
    }
public:
    static constexpr unsigned grainCount=18000;
    static constexpr unsigned variationCount=4;
    unsigned variation=1; // Ornate; the original diagonal mode remains selectable.
    struct Grain { float x,y,vx,vy,light; } grains[grainCount];
    float time=0;
    void Init()
    {
        if(initialized)return;
        const unsigned n[6]={2,3,4,5,6,8},m[6]={3,4,6,7,9,11};
        const unsigned ornateN[6]={2,2,4,4,6,8},ornateM[6]={4,6,6,8,10,10};
        static float cx[12][96],cy[12][64];
        for(unsigned k=0;k<12;++k)
        {
            for(unsigned x=0;x<96;++x)cx[k][x]=sinf(3.141592654f*k*x/95+1.570796327f);
            for(unsigned y=0;y<64;++y)cy[k][y]=sinf(3.141592654f*k*y/63+1.570796327f);
        }
        for(unsigned k=0;k<6;++k)for(unsigned y=0;y<64;++y)for(unsigned x=0;x<96;++x)
        {
            const float a=cx[n[k]][x]*cy[m[k]][y],b=cx[m[k]][x]*cy[n[k]][y];
            const unsigned i=y*96+x;
            basis[0][k][i]=(a-b)*0.5f; // Original: antisymmetric plate modes.
            // Same-parity modes avoid the forced anti-diagonal of odd/even sums.
            basis[1][k][i]=(cx[ornateN[k]][x]*cy[ornateM[k]][y]+cx[ornateM[k]][x]*cy[ornateN[k]][y])*0.5f;
            basis[2][k][i]=a;          // Cells: rectangular standing-wave modes.
            basis[3][k][i]=(a-0.55f*b)/1.55f; // Weave: unequal crossed modes.
        }
        for(auto &p:grains)p={random(),random(),0,0,0.4f+random()*0.6f};
        initialized=true;rebuild();
    }
    void rebuild()
    {
        for(unsigned i=0;i<96*64;++i)
        {
            float v=0;
            for(unsigned f=0;f<variationCount;++f)if(families[f]>.0001f)
                for(unsigned k=0;k<6;++k)v+=families[f]*weights[k]*basis[f][k][i];
            field[i]=v;
        }
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<96;++x)
        {
            const unsigned a=y*96+x;
            const unsigned l=x?x-1:0,r=x<95?x+1:95,t=y?y-1:0,b=y<63?y+1:63;
            gx[a]=(field[y*96+r]-field[y*96+l])*95/(r-l);
            gy[a]=(field[b*96+x]-field[t*96+x])*63/(b-t);
        }
    }
    float vibration(float x,float y) const { return sample(field,x,y); }
    unsigned scatterEvents() const{return bursts;}
    void update(const AudioSpectrum &audio,float gain,float dt,bool paused)
    {
        Init();if(paused || dt<=0)return;if(dt>0.05f)dt=0.05f;
        accumulator+=dt;
        const float tick=1.0f/120;
        for(unsigned n=0;n<6 && accumulator+0.0000001f>=tick;++n)
        {step(audio,gain,tick);accumulator-=tick;if(accumulator<0)accumulator=0;}
    }
private:
    void step(const AudioSpectrum &audio,float gain,float dt)
    {
        time+=dt;cooldown-=dt;
        const float blend=1-expf(-dt/0.65f);
        for(unsigned f=0;f<variationCount;++f)families[f]+=((f==variation%variationCount?1.0f:0)-families[f])*blend;
        float levels[6]={},loud=0;
        const unsigned edges[7]={6,30,48,66,83,102,128};
        if(audio.available)for(unsigned k=0;k<6;++k)
        {
            float sum=0;for(unsigned b=edges[k];b<edges[k+1];++b)sum+=audio.bands[b]*audio.bands[b];
            levels[k]=sqrtf(sum/(edges[k+1]-edges[k]))*gain;
            if(levels[k]>loud)loud=levels[k];
        }
        if(loud>1)loud=1;
        energy+=(loud-energy)*(1-expf(-dt/0.12f));
        // Spectral changes can trigger kicks even when a loud/limited track's
        // PCM peak barely changes. No format-specific behavior or fake beats.
        float flux=0;
        for(unsigned k=0;k<6;++k){float rise=levels[k]-previousLevels[k];if(rise>0)flux+=rise;previousLevels[k]=levels[k];}
        flux/=6;
        const float peak=audio.available?audio.peak*gain:0;
        const bool onset=flux>fluxEnvelope*1.8f+0.008f;
        const bool beat=audio.available && loud>.025f && cooldown<=0
            && ((peak>.08f && peak>envelope*1.45f+.025f) || onset);
        envelope+=(peak-envelope)*(1-expf(-dt/0.35f));
        fluxEnvelope+=(flux-fluxEnvelope)*(1-expf(-dt/.25f));
        if(beat){cooldown=.18f;++bursts;}
        if(loud>0.025f)
        {
            unsigned a=0,b=1;if(levels[b]>levels[a]){unsigned t=a;a=b;b=t;}
            for(unsigned k=2;k<6;++k)if(levels[k]>levels[a]){b=a;a=k;}else if(levels[k]>levels[b])b=k;
            const float total=levels[a]+levels[b]+0.00001f;
            for(unsigned k=0;k<6;++k)
            {
                const float target=(k==a || k==b)?levels[k]/total:0;
                weights[k]+=(target-weights[k])*(1-expf(-dt/1.1f));
            }
        }
        rebuild();
        const unsigned steps=1;const float h=dt; // update() already supplies fixed 120 Hz ticks.
        const float damping=expf(-h*7),noiseScale=sqrtf(h)*(0.003f+energy*0.035f);
        for(auto &p:grains)
        {
            if(beat) { p.vx+=(random()-0.5f)*(0.15f+energy*0.25f);p.vy+=(random()-0.5f)*(0.15f+energy*0.25f); }
            for(unsigned s=0;s<steps;++s)
            {
                const float v=sample(field,p.x,p.y),dx=sample(gx,p.x,p.y),dy=sample(gy,p.x,p.y);
                const float force=-v*0.95f/(1+0.025f*(dx*dx+dy*dy));
                p.vx=(p.vx+force*dx*h)*damping;p.vy=(p.vy+force*dy*h)*damping;
                // Keep a small, audio-dependent vibration at settled nodes;
                // multiplying entirely by |v| previously glued grains there.
                const float jitter=noiseScale*(fabsf(v)+energy*.18f);
                p.x+=p.vx*h+(random()-0.5f)*jitter;p.y+=p.vy*h+(random()-0.5f)*jitter;
                if(p.x<0){p.x=0;p.vx=fabsf(p.vx)*0.3f;}if(p.x>1){p.x=1;p.vx=-fabsf(p.vx)*0.3f;}
                if(p.y<0){p.y=0;p.vy=fabsf(p.vy)*0.3f;}if(p.y>1){p.y=1;p.vy=-fabsf(p.vy)*0.3f;}
            }
        }
    }
};
