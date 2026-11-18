#pragma once

// CPU-only neon halo. Fixed geometry and bounded rays avoid full-screen
// procedural shading or allocations in the frame loop.
class RadialPreset
{
    static constexpr int segments = 192;
    float unitX[segments], unitY[segments];
    float rotationX = 1.0f, rotationY = 0.0f;
    float energy = 0.0f;
    struct Point { int x, y; };
    Point halo[segments], inner[segments], outer[segments];
    static Color dim(Color c, unsigned scale)
    {
        return Color{(uint8_t)(c.r * scale / 255), (uint8_t)(c.g * scale / 255), (uint8_t)(c.b * scale / 255)};
    }
    static void pixel(Scene2D *scene, int x, int y, Color c)
    {
        if (x >= 0 && y >= 0 && x < 1920 && y < 1080) scene->DrawPixel(x,y,c);
    }
    static void line(Scene2D *scene, Point a, Point b, Color c, int width)
    {
        int dx=b.x-a.x, dy=b.y-a.y;
        int ax=dx<0?-dx:dx, ay=dy<0?-dy:dy;
        int steps=ax>ay?ax:ay;
        if (!steps) { pixel(scene,a.x,a.y,c); return; }
        for(int i=0;i<=steps;++i)
        {
            int x=a.x+dx*i/steps, y=a.y+dy*i/steps;
            pixel(scene,x,y,c);
            for(int r=1;r<=width;++r)
            {
                if(ax>ay) { pixel(scene,x,y-r,c); pixel(scene,x,y+r,c); }
                else { pixel(scene,x-r,y,c); pixel(scene,x+r,y,c); }
            }
        }
    }
public:
    RadialPreset()
    {
        // Unit-circle recurrence: 2*pi/192, no per-ray trig calls.
        float x=1.0f,y=0.0f;
        for(int i=0;i<segments;++i)
        {
            unitX[i]=x; unitY[i]=y;
            const float nextX=x*0.9994645875f-y*0.0327190828f;
            y=x*0.0327190828f+y*0.9994645875f; x=nextX;
        }
    }
    void draw(Scene2D *scene, const AudioWaveform &wave, float gain, int palette, bool paused, float dt)
    {
        const Color colors[4]={{0,220,255},{255,35,195},{90,255,65},{255,170,30}};
        const Color accent=colors[(palette+1)%4], base=colors[palette];
        float level=0.0f;
        for(unsigned i=0;i<AUDIO_WAVE_POINTS;++i)
            level += wave.points[i]<0?-wave.points[i]:wave.points[i];
        level=level/AUDIO_WAVE_POINTS*gain*5.0f;
        if(level>1.0f) level=1.0f;
        if(!paused)
        {
            energy += (level-energy)*0.22f;
            // Small-angle rotation, renormalized to prevent accumulated drift.
            if(dt>0.05f) dt=0.05f;
            const float angle=dt*(0.10f+energy*0.3f);
            const float c=1.0f-angle*angle*0.5f;
            const float next=rotationX*c-rotationY*angle;
            rotationY=rotationX*angle+rotationY*c; rotationX=next;
            const float correction=0.5f*(3.0f-rotationX*rotationX-rotationY*rotationY);
            rotationX*=correction; rotationY*=correction;
        }
        const float radius=170.0f+energy*65.0f;
        for(int i=0;i<segments;++i)
        {
            const float x=unitX[i]*rotationX-unitY[i]*rotationY;
            const float y=unitX[i]*rotationY+unitY[i]*rotationX;
            const unsigned sample=(unsigned)i*AUDIO_WAVE_POINTS/segments;
            float value=wave.points[sample]*gain;
            if(value>1.0f) value=1.0f;
            if(value< -1.0f) value=-1.0f;
            const float magnitude=value<0?-value:value;
            const float ring=radius+value*75.0f;
            const float ray=ring+12.0f+magnitude*135.0f;
            halo[i]=Point{960+(int)(x*ring),590+(int)(y*ring)};
            inner[i]=Point{960+(int)(x*(radius-30.0f)),590+(int)(y*(radius-30.0f))};
            outer[i]=Point{960+(int)(x*ray),590+(int)(y*ray)};
        }
        // Dim structure first, then glow, colored contour, and bright core.
        for(int i=0;i<segments;++i)
        {
            int next=(i+1)%segments;
            line(scene,inner[i],inner[next],dim(accent,55),0);
        }
        for(int i=0;i<segments;i+=3)
            line(scene,halo[i],outer[i],dim(base,120),1);
        for(int i=0;i<segments;++i)
        {
            int next=(i+1)%segments;
            line(scene,halo[i],halo[next],dim(base,25),6);
        }
        for(int i=0;i<segments;++i)
        {
            int next=(i+1)%segments;
            line(scene,halo[i],halo[next],dim(base,90),3);
        }
        for(int i=0;i<segments;++i)
        {
            int next=(i+1)%segments;
            line(scene,halo[i],halo[next],base,1);
        }
        for(int i=0;i<segments;++i)
        {
            int next=(i+1)%segments;
            line(scene,halo[i],halo[next],Color{185,250,255},0);
        }
        for(int i=0;i<segments;i+=6)
            line(scene,outer[i],outer[i],accent,0);
    }
};
