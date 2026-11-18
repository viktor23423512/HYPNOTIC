#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "audio_waveform.h"
#include <math.h>
#include <stdlib.h>

class GpuWormhole
{
    // TextureMapper needs one matrix pair per triangle. A 32x18 mapping mesh
    // keeps that work bounded; rasterization/texture sampling remain 1080p.
    static constexpr int columns=32,rows=18;
    struct Node { float x,y,angle,depth,radius; } nodes[(columns+1)*(rows+1)];
    GpuScene::Vertex vertices[columns*rows*6];
    GLuint textures[4]={};
    float phase=0,energy=0;
public:
    bool Init(GpuScene *scene,void (*progress)(void *,const char *)=nullptr,void *context=nullptr)
    {
        if(progress) progress(context,"TUNNEL 1 - BUILDING MAPPING GRID");
        for(int y=0;y<=rows;++y) for(int x=0;x<=columns;++x)
        {
            const float px=x*1920.0f/columns,py=y*1080.0f/rows;
            const float u=(px-960)/540,v=(py-540)/540;
            const float r=sqrtf(u*u+v*v);
            nodes[y*(columns+1)+x]=Node{px,py,atan2f(v,u),1/(r+0.16f),r};
        }
        const Color primary[4]={{45,235,80},{20,200,255},{235,35,210},{255,125,25}};
        const Color accent[4]={{210,165,40},{220,175,65},{50,170,255},{100,245,75}};
        static constexpr unsigned imageBytes=512*512*4;
        if(progress) progress(context,"TUNNEL 2 - ALLOCATING PALETTE IMAGES");
        unsigned char *image=static_cast<unsigned char *>(malloc(imageBytes*4));
        if(!image)
        {
            if(progress) progress(context,"TUNNEL FAILED - IMAGE ALLOCATION");
            return false;
        }
        // Geometry/shading is identical for every palette. Evaluate its costly
        // trigonometry once per texel, then apply the four sets of colors.
        if(progress) progress(context,"TUNNEL 3 - GENERATING ALL PALETTES");
        for(int y=0;y<512;++y) for(int x=0;x<512;++x)
        {
            const float u=x*6.283185307f/512,v=y*6.283185307f/512;
            const float ring=sinf(v+1.5f*sinf(u*5)+0.6f*sinf(u*3+v*2));
            const float cloud=0.5f+0.5f*sinf(u*4+v*2+sinf(u*7-v));
            float edge=1-fabsf(ring)*6; if(edge<0) edge=0; edge*=edge;
            const float wall=(0.2f+cloud*0.8f)*(0.3f+0.7f*(0.5f+0.5f*ring));
            const float shade=0.1f+wall*0.65f,gold=(1-cloud)*wall*0.5f;
            for(int p=0;p<4;++p)
            {
                const float channels[3]={5+primary[p].r*shade+accent[p].r*gold+edge*130,
                    2+primary[p].g*shade+accent[p].g*gold+edge*150,
                    12+primary[p].b*shade+accent[p].b*gold+edge*130};
                const unsigned offset=p*imageBytes+(y*512+x)*4;
                for(int c=0;c<3;++c) image[offset+c]=(unsigned char)(channels[c]>255?255:channels[c]);
                image[offset+3]=255;
            }
        }
        const char *uploading[4]={"TUNNEL 4A - UPLOADING GREEN","TUNNEL 4B - UPLOADING CYAN",
            "TUNNEL 4C - UPLOADING MAGENTA","TUNNEL 4D - UPLOADING AMBER"};
        for(int p=0;p<4;++p)
        {
            if(progress) progress(context,uploading[p]);
            textures[p]=scene->createTexture(512,512,image+p*imageBytes,true);
            if(!textures[p]) { free(image); return false; }
        }
        free(image);
        if(progress) progress(context,"TUNNEL 5 - ALL TEXTURES UPLOADED");
        return glGetError()==GL_NO_ERROR;
    }
    void draw(GpuScene *scene,const AudioWaveform &wave,float gain,int palette,bool paused,float dt)
    {
        if(dt>0.05f) dt=0.05f;
        if(!paused)
        {
            float target=0;
            for(unsigned i=0;i<AUDIO_WAVE_POINTS;++i) target+=fabsf(wave.points[i]);
            target=target/AUDIO_WAVE_POINTS*gain*10;
            if(target>1) target=1;
            energy+=(target-energy)*0.18f;
            phase+=dt*(0.6f+energy*0.4f);
            // Keep phase precise over long playback without a visible reset.
            if(phase>6283.185307f) phase-=6283.185307f;
        }
        unsigned count=0;
        for(int y=0;y<rows;++y) for(int x=0;x<columns;++x)
        {
            const int ids[4]={y*(columns+1)+x,y*(columns+1)+x+1,(y+1)*(columns+1)+x,(y+1)*(columns+1)+x+1};
            GpuScene::Vertex quad[4];
            float minU=100,maxU=-100;
            for(int n=0;n<4;++n)
            {
                const Node &node=nodes[ids[n]];
                const int sample=((int)((node.angle+3.141592654f)*40.743665f))&255;
                float signal=wave.points[sample]*gain*6;
                if(signal>1) signal=1; if(signal< -1) signal=-1;
                const float u=node.angle/6.283185307f;
                if(u<minU) minU=u; if(u>maxU) maxU=u;
                quad[n]={node.x,node.y,u,
                    node.depth*(0.7f+energy*0.03f)+phase*0.3f+signal*0.12f};
            }
            for(int n=0;n<4;++n)
            {
                if(maxU-minU>0.5f && quad[n].u<0) quad[n].u+=1;
                const Node &node=nodes[ids[n]];
                quad[n].u+=phase*0.01f+0.1f*sinf(node.depth*1.2f+phase*0.1f);
            }
            const int order[6]={0,1,2,1,3,2};
            for(int n=0;n<6;++n) vertices[count++]=quad[order[n]];
        }
        scene->drawMesh(textures[palette],vertices,count);
        // Tiny dark center closes the perspective singularity.
        for(int y=-32;y<=32;++y)
        {
            const int w=(int)sqrtf(32*32-y*y);
            scene->DrawRectangle(960-w,540+y,w*2+1,1,Color{0,0,0});
        }
    }
};
