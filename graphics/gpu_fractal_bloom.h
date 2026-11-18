#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "fractal_bloom_model.h"
#include <string.h>

class GpuFractalBloom
{
    FractalBloomModel model;
    struct Light {float x,y;unsigned group;};
    static constexpr unsigned maxLights=65536;
    Light lights[maxLights];
    struct Warp {float u,v,veil;};
    Warp field[33*33];
    unsigned char density[960*540];
    GpuScene::Vertex vertices[49152];
    unsigned visible=0,used=0;
    static constexpr float halfPi=1.570796327f;
    void quad(float x,float y,float r)
    {
        const GpuScene::Vertex q[4]={{x-r,y-r,0,0},{x+r,y-r,0,0},
            {x-r,y+r,0,0},{x+r,y+r,0,0}};
        const int order[6]={0,1,2,1,3,2};for(int i=0;i<6;++i)vertices[used++]=q[order[i]];
    }
    void layer(GpuScene *scene,unsigned group,unsigned stride,float radius,Color color,float exposure)
    {
        used=0;
        for(unsigned i=0;i<visible;i+=stride)
        {
            if(lights[i].group!=group)continue;
            quad(lights[i].x,lights[i].y,radius);
            if(used==49152){scene->drawSolidMesh(vertices,used,color,exposure);used=0;}
        }
        if(used)scene->drawSolidMesh(vertices,used,color,exposure);
    }
public:
    void seed(unsigned value){model.seed(value);}
    void draw(GpuScene *scene,const AudioSpectrum &s,float gain,int palette,bool paused,float dt)
    {
        model.update(s,gain,paused,dt);
        const float zoom=440+model.bass*18;
        memset(density,0,sizeof(density));visible=0;
        for(unsigned shell=0;shell<4;++shell)
        {
            const unsigned channel=shell&1;
            const auto &flow=model.journeys[channel].value;
            const float depth=shell<2?1.0f:.58f;
            const float direction=channel?1.0f:-1.0f;
            const float panX=flow.offsetX+direction*110,panY=flow.offsetY;
            const float warp=1+model.mids*.25f;
            // Evaluate the smooth deformation field on a small mapping grid;
            // the fine Julia geometry itself stays at full sample resolution.
            for(int gy=0;gy<=32;++gy)for(int gx=0;gx<=32;++gx)
            {
                const float px=-2+gx*.125f,py=-2+gy*.125f,r2=px*px+py*py;
                const float angle=flow.twist*r2+shell*.17f;
                const float sn=sinf(angle),cs=sinf(angle+halfPi);
                field[gy*33+gx]={px*cs-py*sn+flow.warpX*warp*sinf(py*flow.frequency+model.phase+shell),
                    px*sn+py*cs+flow.warpY*warp*sinf(px*flow.frequency-model.phase*.8f+shell),
                    .5f+.5f*sinf(px*3+py*4+model.phase*.65f+channel*2)};
            }
            for(unsigned i=0;i<FractalBloomModel::count&&visible<maxLights;++i)
            {
                const auto &p=model.points(channel)[i];
                const float r2=p.x*p.x+p.y*p.y;
                const float fx=(p.x+2)*8,fy=(p.y+2)*8;
                int gx=(int)fx,gy=(int)fy;
                if(gx<0)gx=0;if(gx>31)gx=31;if(gy<0)gy=0;if(gy>31)gy=31;
                const float dx=fx-gx,dy=fy-gy;
                const float weights[4]={(1-dx)*(1-dy),dx*(1-dy),(1-dx)*dy,dx*dy};
                const unsigned indices[4]={(unsigned)(gy*33+gx),(unsigned)(gy*33+gx+1),
                    (unsigned)((gy+1)*33+gx),(unsigned)((gy+1)*33+gx+1)};
                float u=0,v=0,veil=0;
                for(int j=0;j<4;++j){const auto &f=field[indices[j]];u+=f.u*weights[j];v+=f.v*weights[j];veil+=f.veil*weights[j];}
                const float x=960+panX+u*zoom*depth,y=540+panY+v*zoom*depth;
                if(x<3||x>=1917||y<3||y>=1077)continue;
                auto &cell=density[(unsigned)(y*.5f)*960+(unsigned)(x*.5f)];
                // Prevent dense preimages from blowing out to white. Keep
                // detail at native resolution, not a low-resolution image.
                if(cell>=2)continue;++cell;
                // Moving local fade fields dissolve/reveal portions of each
                // structure, rather than pulsing the entire picture together.
                if(veil<.1f)continue;
                const unsigned band=(unsigned)(8+r2*4+p.x*.35f+flow.hue);
                lights[visible++]={x,y,((band+shell)&3)+(veil<.3f?4u:0u)};
            }
        }
        static const Color colors[4][4]={
            {{45,155,255},{85,235,245},{170,80,255},{245,115,215}},
            {{255,125,30},{255,205,70},{245,65,105},{175,70,245}},
            {{30,220,160},{130,255,90},{35,145,240},{140,210,255}},
            {{170,195,255},{230,245,255},{105,150,245},{200,130,255}}};
        for(unsigned group=0;group<8;++group)
        {
            const Color c=colors[palette&3][group&3];
            const float fade=group<4?1.0f:.25f;
            // Sparse broad bloom, then fuller narrow glow and crisp detail.
            layer(scene,group,8,12+model.bass*4,c,(.018f+model.mids*.012f)*fade);
            layer(scene,group,2,3.8f+model.bass,c,(.065f+model.treble*.025f)*fade);
            layer(scene,group,1,.85f+model.treble*.25f,c,(.42f+model.mids*.15f)*fade);
        }
    }
};
