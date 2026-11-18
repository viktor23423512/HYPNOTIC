#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "cymatics_model.h"
#include <stdlib.h>
#include <string.h>

class GpuCymatics
{
    CymaticsModel model;
    GLuint texture=0;
    unsigned char *pixels=nullptr;
    int lastPalette=-1;
    bool painted=false;
public:
    static constexpr unsigned width=1024,height=576;
    bool Init(GpuScene *scene)
    {
        pixels=static_cast<unsigned char *>(calloc(width*height,4));if(!pixels)return false;
        model.Init();texture=scene->createTexture(width,height,pixels,false);
        if(!texture){free(pixels);pixels=nullptr;return false;}return true;
    }
    void draw(GpuScene *scene,const AudioSpectrum &audio,float gain,unsigned palette,bool paused,float dt,unsigned variation=1)
    {
        model.variation=variation;
        model.update(audio,gain,dt,paused);
        if(!painted || !paused || lastPalette!=(int)palette)
        {
            memset(pixels,0,width*height*4);
            const unsigned colors[4][3]={{235,215,155},{110,225,255},{245,145,240},{255,165,65}};
            const unsigned *color=colors[palette%4];
            for(const auto &p:model.grains)
            {
                // Small, sharp individual grains. A faint surrounding halo uses
                // the same upload; no per-grain GL calls or frame allocations.
                const int x=2+(int)(p.x*(width-5)),y=2+(int)(p.y*(height-5));
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
                {
                    const float light=p.light*(dx==0 && dy==0?0.9f:0.045f);
                    auto dst=pixels+((y+dy)*width+x+dx)*4;
                    for(unsigned c=0;c<3;++c){unsigned v=dst[c]+(unsigned)(color[c]*light);dst[c]=(unsigned char)(v>255?255:v);}
                    dst[3]=255;
                }
            }
            scene->updateTextureRegion(texture,0,0,width,height,pixels);
            painted=true;lastPalette=(int)palette;
        }
        const GpuScene::Sprite quad={0,0,1920,1080,0.5f/width,0.5f/height,1-0.5f/width,1-0.5f/height,1};
        scene->drawSprites(texture,&quad,1);
    }
};
