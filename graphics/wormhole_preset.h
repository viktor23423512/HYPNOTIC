#pragma once

// Procedural tunnel, shaded at 30 Hz and softly upscaled.
// No allocations, per-pixel trig, or full-resolution procedural shader.
class WormholePreset
{
    static constexpr int width=480,height=270;
    struct Map { float radius,angle,depth; } map[width*height];
    Color image[width*height] = {};
    float sine[2048];
    float phase=0, energy=0, pending=0;
    bool initialized=false;
    int lastPalette=-1;
    float lastGain=-1;
    static float absolute(float x) { return x<0?-x:x; }
    static float root(float n)
    {
        float x=n>1?n:1;
        for(int i=0;i<8;++i) x=0.5f*(x+n/x);
        return x;
    }
    static float angle(float y,float x)
    {
        const float a=absolute(y)+0.000001f;
        float r,t;
        if(x>=0) { r=(x-a)/(x+a); t=0.7853981634f; }
        else { r=(x+a)/(a-x); t=2.3561944902f; }
        t+=(0.1963f*r*r-0.9817f)*r;
        return y<0?-t:t;
    }
    float sin(float radians) const { return sine[((int)(radians*325.949323f))&2047]; }
    static unsigned char channel(float value)
    {
        return value<0?0:value>255?255:(unsigned char)value;
    }
    static Color average(Color a,Color b)
    { return Color{(uint8_t)((a.r+b.r)/2),(uint8_t)((a.g+b.g)/2),(uint8_t)((a.b+b.b)/2)}; }
    void shade(const AudioWaveform &wave,float gain,int palette)
    {
        const Color primary[4]={{45,235,80},{20,200,255},{235,35,210},{255,125,25}};
        const Color secondary[4]={{210,165,40},{220,175,65},{50,170,255},{100,245,75}};
        const Color base=primary[palette], accent=secondary[palette];
        for(int i=0;i<width*height;++i)
        {
            const Map &p=map[i];
            const int sample=((int)((p.angle+3.14159265f)*40.743665f))&255;
            float audio=wave.points[sample]*gain*7;
            if(audio>1) audio=1;
            if(audio< -1) audio=-1;
            const float twist=p.angle+phase+0.55f*sin(p.depth*1.4f-phase*2);
            const float ripple=0.75f*sin(twist*5+p.depth*0.65f+phase*2)
                              +0.4f*sin(twist*3-p.depth+phase*3);
            // Advancing phase moves contour layers outward from the center.
            const float rings=sin(p.depth*5+phase*12+ripple*2+audio*(1.0f+energy));
            float edge=1-absolute(rings)*5;
            if(edge<0) edge=0;
            edge*=edge;
            const float cloud=0.5f+0.5f*sin(twist*4+p.depth*2+sin(twist*7-phase*2));
            const float wall=(0.2f+0.8f*cloud)*(0.3f+0.7f*(0.5f+0.5f*rings));
            float fade=(p.radius-0.065f)*6;
            if(fade<0) fade=0;
            if(fade>1) fade=1;
            const float shade=(0.14f+wall*0.6f)*(0.8f+energy*0.2f)*fade;
            const float glow=edge*(0.5f+0.5f*cloud)*fade;
            const float gold=(1-cloud)*wall*0.5f*fade;
            image[i]=Color{
                channel(5+base.r*shade+accent.r*gold+glow*120),
                channel(2+base.g*shade+accent.g*gold+glow*150),
                channel(12+base.b*shade+accent.b*gold+glow*130)};
        }
    }
public:
    WormholePreset()
    {
        for(int i=0;i<2048;++i)
        {
            float x=i*6.283185307f/2048;
            float sign=1;
            if(x>3.141592654f) { x-=3.141592654f; sign=-1; }
            const float product=x*(3.141592654f-x);
            sine[i]=sign*16*product/(49.348022f-4*product);
        }
        for(int y=0;y<height;++y) for(int x=0;x<width;++x)
        {
            float u=(x-(width-1)*0.5f)/(height*0.5f);
            float v=(y-(height-1)*0.5f)/(height*0.5f);
            float r=root(u*u+v*v);
            map[y*width+x]=Map{r,angle(v,u),1/(r+0.16f)};
        }
    }
    void draw(Scene2D *scene,const AudioWaveform &wave,float gain,int palette,bool paused,float dt)
    {
        if(dt>0.05f) dt=0.05f;
        if(!paused)
        {
            float level=0;
            for(unsigned i=0;i<AUDIO_WAVE_POINTS;++i) level+=absolute(wave.points[i]);
            level=level/AUDIO_WAVE_POINTS*gain*10;
            if(level>1) level=1;
            energy+=(level-energy)*0.18f;
            phase+=dt*(0.12f+energy*0.12f);
            if(phase>=6.283185307f) phase-=6.283185307f;
            pending+=dt;
        }
        if(!initialized || ((!paused) && pending>=1.0f/30) || palette!=lastPalette || gain!=lastGain)
        {
            shade(wave,gain,palette);
            initialized=true; pending=0; lastPalette=palette; lastGain=gain;
        }
        // Half-grid bilinear interpolation, each result fills a 2x2 block.
        // The framebuffer is fully covered, so no preceding clear is needed.
        for(int y=0;y<height*2;++y)
        {
            const int row=y/2,nextRow=row+1<height?row+1:row;
            for(int x=0;x<width*2;++x)
            {
                int col=x/2,nextCol=col+1<width?col+1:col;
                Color a=image[row*width+col],b=image[nextRow*width+col];
                if(x&1) { a=average(a,image[row*width+nextCol]); b=average(b,image[nextRow*width+nextCol]); }
                const Color color=(y&1)?average(a,b):a;
                scene->DrawRectangle(x*2,y*2,2,2,color);
            }
        }
    }
};
