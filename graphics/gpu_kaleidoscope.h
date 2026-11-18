#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "audio_waveform.h"
#include <math.h>
#include <stdlib.h>

class GpuKaleidoscope
{
    static constexpr int rings=12,segments=48,sectorSteps=4;
    static constexpr float tau=6.283185307f;
    struct Node { float x,y,u,v; unsigned sample; } nodes[(rings+1)*(segments+1)];
    GpuScene::Vertex mapped[(rings+1)*(segments+1)],vertices[rings*segments*6];
    GLuint textures[4]={};
    float phase=0,energy=0;
public:
    bool Init(GpuScene *scene)
    {
        // Twelve alternating mirror sectors, with matching UVs at every seam.
        // The outer polygon extends beyond all four screen corners for coverage.
        for(int r=0;r<=rings;++r) for(int s=0;s<=segments;++s)
        {
            const float t=(float)r/rings;
            const float radius=1200*t*(0.35f+0.65f*t);
            const float angle=s*tau/segments;
            const int sector=s/sectorSteps;
            const int folded=(sector&1)?sectorSteps-s%sectorSteps:s%sectorSteps;
            const float mirror=folded*tau/segments;
            nodes[r*(segments+1)+s]=Node{
                960+radius*sinf(angle+tau*0.25f),540+radius*sinf(angle),
                radius/540*sinf(mirror+tau*0.25f),radius/540*sinf(mirror),
                (unsigned)(r*17+folded*37)%AUDIO_WAVE_POINTS};
        }
        // Shared organic pattern, four palettes: one expensive math pass.
        const Color a[4]={{255,25,165},{20,220,255},{255,150,20},{50,255,100}};
        const Color b[4]={{20,190,255},{160,30,255},{235,30,150},{20,150,255}};
        const Color c[4]={{255,200,35},{255,80,170},{100,245,60},{230,200,35}};
        static constexpr unsigned imageBytes=512*512*4;
        auto image=static_cast<unsigned char *>(malloc(imageBytes*4));
        if(!image) return false;
        for(int y=0;y<512;++y) for(int x=0;x<512;++x)
        {
            const float u=x*tau/512,v=y*tau/512;
            const float flow=sinf(3*u+2*v+0.9f*sinf(2*u-3*v));
            const float petals=sinf(4*u-3*v+0.8f*sinf(u+4*v));
            const float contour=sinf(2*u+3*v+flow*1.5f+petals);
            float edge=1-fabsf(contour)*7; if(edge<0) edge=0; edge*=edge;
            const float first=0.08f+0.5f*(0.5f+0.5f*flow);
            const float second=0.04f+0.42f*(0.5f+0.5f*petals);
            const float third=(1-first)*(1-second)*0.4f;
            for(int p=0;p<4;++p)
            {
                const float channels[3]={3+a[p].r*first+b[p].r*second+c[p].r*third+edge*135,
                    2+a[p].g*first+b[p].g*second+c[p].g*third+edge*135,
                    6+a[p].b*first+b[p].b*second+c[p].b*third+edge*145};
                const unsigned offset=p*imageBytes+(y*512+x)*4;
                for(int ch=0;ch<3;++ch) image[offset+ch]=(unsigned char)(channels[ch]>255?255:channels[ch]);
                image[offset+3]=255;
            }
        }
        for(int p=0;p<4;++p)
        {
            textures[p]=scene->createTexture(512,512,image+p*imageBytes,true);
            if(!textures[p]) { free(image); return false; }
        }
        free(image);
        return glGetError()==GL_NO_ERROR;
    }

    void draw(GpuScene *scene,const AudioWaveform &wave,float gain,int palette,bool paused,float dt)
    {
        if(dt<0) dt=0;
        if(dt>0.05f) dt=0.05f;
        if(!paused)
        {
            float target=0;
            for(unsigned i=0;i<AUDIO_WAVE_POINTS;++i) target+=fabsf(wave.points[i]);
            target=target/AUDIO_WAVE_POINTS*gain*8;
            if(target>1) target=1;
            energy+=(target-energy)*0.18f;
            phase+=dt*(0.18f+energy*0.22f);
            if(phase>tau) phase-=tau;
        }
        const float sine=sinf(phase),cosine=sinf(phase+tau*0.25f);
        const float zoom=0.62f+0.1f*sinf(phase*2)+energy*0.16f;
        const float uOffset=0.5f+0.23f*sinf(phase*2);
        const float vOffset=0.5f+0.21f*sinf(phase*3+1);
        for(unsigned i=0;i<(rings+1)*(segments+1);++i)
        {
            const Node &node=nodes[i];
            float signal=wave.points[node.sample]*gain*3;
            if(signal>1) signal=1; if(signal< -1) signal=-1;
            const float scale=zoom*(1+signal*0.11f);
            mapped[i]=GpuScene::Vertex{node.x,node.y,
                uOffset+(node.u*cosine-node.v*sine)*scale,
                vOffset+(node.u*sine+node.v*cosine)*scale};
        }
        unsigned count=0;
        for(int r=0;r<rings;++r) for(int s=0;s<segments;++s)
        {
            const unsigned i=r*(segments+1)+s,j=i+segments+1;
            vertices[count++]=mapped[i]; vertices[count++]=mapped[j]; vertices[count++]=mapped[j+1];
            vertices[count++]=mapped[i]; vertices[count++]=mapped[j+1]; vertices[count++]=mapped[i+1];
        }
        scene->drawMesh(textures[palette&3],vertices,count);
    }
};
