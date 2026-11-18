#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "audio_spectrum.h"
#include <stdlib.h>

// An Alchemy-inspired flowing colour field, rather than a tunnel or a fluid
// simulation. The existing precompiled texture shader handles all rasterization.
class GpuLiquidAura
{
    static constexpr int columns=24,rows=14,loops=8,steps=160;
    static constexpr float tau=6.283185307f;
    GLuint textures[4]={};
    GpuScene::Vertex mapped[(columns+1)*(rows+1)],mesh[columns*rows*6];
    struct Point { float x,y,nx,ny; } paths[loops][steps+1];
    GpuScene::Vertex ribbons[4*steps*6];
    float phase=0,bass=0,mids=0,treble=0;

    static float clamp(float v) { return v<0?0:(v>1?1:v); }
    static float region(const AudioSpectrum &s,int first,int end,float gain)
    {
        if(!s.available)return 0;
        float sum=0;
        for(int i=first;i<end;++i)sum+=s.bands[i]*s.bands[i];
        return clamp(sqrtf(sum/(end-first))*gain);
    }
    static void follow(float &value,float target,float dt)
    {
        const float duration=target>value?.07f:.55f;
        value+=(target-value)*(1-expf(-dt/duration));
    }
public:
    bool Init(GpuScene *scene)
    {
        // Generate once and retain all palettes. No per-frame texture uploads,
        // allocations, readbacks, framebuffer changes or runtime shader compiler.
        constexpr unsigned bytes=512*512*4;
        auto image=static_cast<unsigned char *>(malloc(bytes*4));
        if(!image)return false;
        const Color a[4]={{10,180,210},{245,30,135},{50,210,110},{255,100,20}};
        const Color b[4]={{125,25,235},{45,85,245},{20,105,235},{160,20,165}};
        for(int y=0;y<512;++y)for(int x=0;x<512;++x)
        {
            const float u=x*tau/512,v=y*tau/512;
            const float f=sinf(u*2+.8f*sinf(v*3))+.6f*sinf(v*2+.9f*sinf(u*3))
                +.3f*sinf(5*u-4*v)+.12f*sinf(9*u+7*v);
            const float g=sinf(v*3-u+.7f*sinf(u*2+v*2));
            const float cloud=clamp(.46f+f*.28f);
            const float shade=cloud*cloud*.76f;
            const float blend=.5f+.5f*g;
            float edge=clamp(1-fabsf(f+.15f)*5);edge*=edge;
            for(int p=0;p<4;++p)
            {
                const float channels[3]={a[p].r*blend+b[p].r*(1-blend),
                    a[p].g*blend+b[p].g*(1-blend),a[p].b*blend+b[p].b*(1-blend)};
                const unsigned at=p*bytes+(y*512+x)*4;
                for(int c=0;c<3;++c)image[at+c]=(unsigned char)(3+channels[c]*shade+edge*channels[c]*.24f);
                image[at+3]=255;
            }
        }
        bool ok=true;
        for(int p=0;p<4;++p)
        {
            textures[p]=scene->createTexture(512,512,image+p*bytes,true);
            if(!textures[p]){ok=false;break;}
        }
        free(image);
        // Failed initialization is never drawn or retried by the player.
        ok=ok && glGetError()==GL_NO_ERROR;
        if(!ok)
        {
            glDeleteTextures(4,textures);
            for(auto &texture:textures)texture=0;
        }
        return ok;
    }

    void draw(GpuScene *scene,const AudioSpectrum &s,float gain,int palette,bool paused,float dt)
    {
        if(!(dt>=0))dt=0;
        if(dt>.05f)dt=.05f;
        if(!paused)
        {
            follow(bass,region(s,8,42,gain),dt);
            follow(mids,region(s,42,89,gain),dt);
            follow(treble,region(s,89,128,gain),dt);
            // Very slow ambient flow remains with no input; audio supplies the
            // expansion and detail. A common phase keeps motion continuous.
            phase+=dt*(.12f+bass*.19f+mids*.08f);
            if(phase>=tau*100)phase-=tau*100;
        }
        const float t=phase;
        for(int y=0;y<=rows;++y)for(int x=0;x<=columns;++x)
        {
            const float px=(float)x/columns,py=(float)y/rows;
            const float u=(px-.5f)*1.6f,v=py-.5f;
            const float swell=.86f-bass*.17f;
            const float flowX=.15f*sinf(v*5+t)+.08f*sinf(u*4-v*3-t*2);
            const float flowY=.14f*sinf(u*4-t)+.07f*sinf(v*5+u*2+t*2);
            mapped[y*(columns+1)+x]={px*1920,py*1080,
                .5f+u*swell+flowX+.07f*sinf(t),
                .5f+v*swell+flowY+.06f*sinf(t*2)+mids*.02f*sinf(u*8+t*3)};
        }
        unsigned n=0;
        for(int y=0;y<rows;++y)for(int x=0;x<columns;++x)
        {
            const int i=y*(columns+1)+x,j=i+columns+1;
            mesh[n++]=mapped[i];mesh[n++]=mapped[i+1];mesh[n++]=mapped[j];
            mesh[n++]=mapped[i+1];mesh[n++]=mapped[j+1];mesh[n++]=mapped[j];
        }
        scene->drawMesh(textures[palette&3],mesh,n);

        // Broad irregular closed ribbons drift through the field: no fixed
        // centre, perspective singularity or kaleidoscope symmetry. Normals are
        // shared by adjacent segments to avoid luminous cracks at their joins.
        for(int k=0;k<loops;++k)
        {
            const float seed=k*1.73f;
            const float cx=960+470*sinf(t+seed),cy=540+285*sinf(t*2+seed*2);
            const float radius=150+k*44+bass*100;
            for(int i=0;i<steps;++i)
            {
                const float a=i*tau/steps;
                const float r=radius*(1+.22f*sinf(a*3+t*2+seed)+.12f*sinf(a*5-t+seed)
                    +treble*.035f*sinf(a*11+t*3));
                paths[k][i]={cx+r*1.5f*sinf(a+1.570796327f),
                    cy+r*.8f*sinf(a)+mids*35*sinf(a*4+t),0,0};
            }
            for(int i=0;i<steps;++i)
            {
                const Point &prev=paths[k][(i+steps-1)%steps],&next=paths[k][(i+1)%steps];
                const float dx=next.x-prev.x,dy=next.y-prev.y;
                const float inv=1/sqrtf(dx*dx+dy*dy+.00001f);
                paths[k][i].nx=-dy*inv;paths[k][i].ny=dx*inv;
            }
            paths[k][steps]=paths[k][0];
        }
        const Color primary[4]={{35,210,235},{245,65,170},{85,230,140},{255,150,60}};
        const Color accent[4]={{140,70,245},{100,110,255},{45,155,240},{205,65,190}};
        const float widths[3]={22,7,1.8f},opacity[3]={.045f,.12f,.48f};
        for(int group=0;group<2;++group)for(int layer=0;layer<3;++layer)
        {
            n=0;
            const float width=widths[layer]*(.8f+bass*.5f);
            for(int k=group;k<loops;k+=2)for(int i=0;i<steps;++i)
            {
                const Point &a=paths[k][i],&b=paths[k][i+1];
                const GpuScene::Vertex q[4]={{a.x+a.nx*width,a.y+a.ny*width,0,0},
                    {a.x-a.nx*width,a.y-a.ny*width,0,0},
                    {b.x+b.nx*width,b.y+b.ny*width,0,0},
                    {b.x-b.nx*width,b.y-b.ny*width,0,0}};
                const int order[6]={0,1,2,1,3,2};
                for(int j=0;j<6;++j)ribbons[n++]=q[order[j]];
            }
            scene->drawSolidMesh(ribbons,n,group?accent[palette&3]:primary[palette&3],
                opacity[layer]*(.65f+mids*.35f));
        }
    }
};
