#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "matrix_eq_model.h"
#include "matrix_katakana.h"
#include <stdlib.h>
#include <string.h>

class GpuMatrixEq
{
    MatrixEqModel model;
    GLuint glyphAtlas=0,columnAtlas=0;
    unsigned char *glyphPixels=nullptr;
    unsigned char strip[32*MatrixEqModel::stripHeight*4];
    GpuScene::Sprite bars[MatrixEqModel::maxColumns*2],sprites[MatrixEqModel::maxParticles+MatrixEqModel::maxColumns];
    int lastGlow=-1;
    bool lastFlip=false;
    static unsigned mask(unsigned g,int x,int y)
    {
        if(x<0 || y<0 || x>=32 || y>=32)return 0;
        const unsigned pos=y*32+x,b=matrixGlyphMasks[g][pos/2];
        return ((pos&1)?b&15:b>>4)*17;
    }
    unsigned index(unsigned glyph,bool pale,int glow) const { return ((glow*7+50)/100*2+(pale?1:0))*MATRIX_GLYPHS+glyph; }
    GpuScene::Sprite glyphSprite(unsigned glyph,bool pale,float x,float y,float size,float opacity,int glow)
    {
        const unsigned tile=index(glyph,pale,glow),tx=(tile%32)*32,ty=(tile/32)*32;
        const float dim=32*size/22;
        return GpuScene::Sprite{x-dim*0.5f,y-dim*0.5f,dim,dim,(tx+0.5f)/1024,(ty+0.5f)/1024,
            (tx+31.5f)/1024,(ty+31.5f)/1024,opacity};
    }
    bool rebuildColumn(GpuScene *scene,unsigned col,int glow,bool flipped)
    {
        memset(strip,0,sizeof(strip));
        for(unsigned i=0;i<32*MatrixEqModel::stripHeight;++i)strip[i*4+3]=255;
        for(unsigned row=0;row<MatrixEqModel::maxRows;++row)
        {
            const unsigned tile=index(model.glyph(col,row),false,glow),tx=(tile%32)*32,ty=(tile/32)*32;
            for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x)
            {
                unsigned char *dst=strip+((row*MatrixEqModel::sourceStep+y)*32+x)*4;
                const unsigned char *src=glyphPixels+((ty+(flipped?31-y:y))*1024+tx+x)*4;
                for(unsigned c=0;c<3;++c) { const unsigned sum=dst[c]+src[c]; dst[c]=(unsigned char)(sum>255?255:sum); }
            }
        }
        return scene->updateTextureRegion(columnAtlas,(col%64)*32,(col/64)*MatrixEqModel::stripHeight,32,MatrixEqModel::stripHeight,strip);
    }
public:
    bool Init(GpuScene *scene)
    {
        glyphPixels=static_cast<unsigned char *>(calloc(1024*1024,4));
        if(!glyphPixels)return false;
        for(unsigned i=0;i<1024*1024;++i)glyphPixels[i*4+3]=255;
        for(unsigned g=0;g<MATRIX_GLYPHS;++g)for(int y=0;y<32;++y)for(int x=0;x<32;++x)
        {
            unsigned sum=0;
            for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx)sum+=mask(g,x+dx,y+dy)*(4-abs(dx))*(4-abs(dy));
            const unsigned body=mask(g,x,y),blur=sum/256;
            for(int glow=0;glow<8;++glow)for(int pale=0;pale<2;++pale)
            {
                const unsigned tile=(glow*2+pale)*MATRIX_GLYPHS+g;
                unsigned char *dst=glyphPixels+(((tile/32)*32+y)*1024+(tile%32)*32+x)*4;
                const unsigned colors[3]={pale?216u:50u,pale?255u:237u,pale?232u:116u};
                const unsigned halo[3]={43,255,115};
                for(unsigned c=0;c<3;++c)
                {
                    const unsigned v=body*colors[c]/255+blur*halo[c]*glow/(255*7);
                    dst[c]=(unsigned char)(v>255?255:v);
                }
            }
        }
        glyphAtlas=scene->createTexture(1024,1024,glyphPixels,false);
        auto empty=static_cast<unsigned char *>(calloc(2048*MatrixEqModel::atlasHeight,4));
        if(!empty)return false;
        columnAtlas=scene->createTexture(2048,MatrixEqModel::atlasHeight,empty,false); free(empty);
        return glyphAtlas && columnAtlas;
    }
    void draw(GpuScene *scene,const AudioSpectrum &spectrum,const MatrixEqSettings &settings,float dt,bool paused,float gain)
    {
        AudioSpectrum input=spectrum;
        if(input.available)
        {
            float headroom=1;
            for(float b:input.bands)if(b*gain>headroom)headroom=b*gain;
            for(auto &b:input.bands)b=b*gain/headroom;
        }
        model.update(input,settings,dt,paused);
        if(lastGlow!=settings.glow || lastFlip!=settings.flipped)
        {for(auto &d:model.dirty)d=true;lastGlow=settings.glow;lastFlip=settings.flipped;}
        for(unsigned i=0;i<model.columns;++i)if(model.dirty[i])
        {
            if(rebuildColumn(scene,i,settings.glow,settings.flipped))model.dirty[i]=false;
        }
        unsigned barCount=0,spriteCount=0;
        const float scale=settings.size/22.0f,step=MatrixEqModel::sourceStep*scale;
        for(unsigned i=0;i<model.columns;++i)
        {
            const unsigned count=1+(unsigned)(model.levels[i]*settings.height+0.5f);
            const float x=(i+0.5f)*1920/model.columns,sourceHeight=(count-1)*MatrixEqModel::sourceStep+32;
            const unsigned sx=(i%64)*32,sy=(i/64)*MatrixEqModel::stripHeight;
            bars[barCount++]={x-16*scale,22-16*scale,32*scale,sourceHeight*scale,
                (sx+0.5f)/2048,(sy+0.5f)/MatrixEqModel::atlasHeight,(sx+31.5f)/2048,(sy+sourceHeight-0.5f)/MatrixEqModel::atlasHeight,0.38f};
            const unsigned start=count>4?(count-4)*MatrixEqModel::sourceStep:0;
            bars[barCount++]={x-16*scale,22-16*scale+start*scale,32*scale,(sourceHeight-start)*scale,
                (sx+0.5f)/2048,(sy+start+0.5f)/MatrixEqModel::atlasHeight,(sx+31.5f)/2048,(sy+sourceHeight-0.5f)/MatrixEqModel::atlasHeight,0.25f};
            const float tip=22+(count-1)*step;
            sprites[spriteCount++]=glyphSprite(model.glyph(i,count-1),true,x,settings.flipped?1080-tip:tip,
                (float)settings.size,0.9f,settings.glow);
        }
        for(const auto &p:model.particles)if(p.alive)
        {
            float fade=1-p.age/p.life; if(fade<0)fade=0; fade*=sqrtf(fade);
            sprites[spriteCount++]=glyphSprite(p.glyph,p.pale,p.x,p.y,p.size,p.brightness*fade,settings.glow);
        }
        if(settings.flipped)for(unsigned i=0;i<barCount;++i)
        {
            auto &bar=bars[i];bar.y=1080-bar.y-bar.h;
            const float v=bar.v0;bar.v0=bar.v1;bar.v1=v;
        }
        scene->drawSprites(columnAtlas,bars,barCount);
        scene->drawSprites(glyphAtlas,sprites,spriteCount);
    }
};
