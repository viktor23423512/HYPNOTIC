#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "storm_model.h"
#include <stdlib.h>

class GpuStorm
{
    static constexpr int width=1024,height=512;
    GLuint sky=0,lit[3]={},foreground=0;
    StormModel model;
    GpuScene::Vertex geometry[3*128*6];
    static float clamp(float v){return v<0?0:(v>1?1:v);}
    static float lattice(int x,int y,int period,unsigned seed)
    {
        unsigned h=((unsigned)x&(period-1))*0x8da6b343u
            ^((unsigned)y&(period-1))*0xd8163841u^seed;
        h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;
        return (h>>8)*(1.0f/16777216);
    }
    static float noise(float u,float v,int period,unsigned seed)
    {
        const int x=(int)floorf(u),y=(int)floorf(v);
        float fx=u-x,fy=v-y;
        fx=fx*fx*fx*(fx*(fx*6-15)+10);fy=fy*fy*fy*(fy*(fy*6-15)+10);
        const float a=lattice(x,y,period,seed),b=lattice(x+1,y,period,seed);
        const float c=lattice(x,y+1,period,seed),d=lattice(x+1,y+1,period,seed);
        return (a+(b-a)*fx)*(1-fy)+(c+(d-c)*fx)*fy;
    }
    static float cloud(float u,float v,unsigned seed)
    {
        // Periodic, domain-warped multi-octave density. Broad billows carry
        // finer turbulent edges; all cloud shading is prepared once, on demand.
        const float warpU=noise(u*4,v*4,4,seed)*.09f;
        const float warpV=noise(u*4+2,v*4,4,seed+19)*.09f;
        u+=warpU;v+=warpV;
        float f=0,weight=.52f;int period=4;
        for(int octave=0;octave<6;++octave)
        {
            f+=weight*noise(u*period,v*period,period,seed+octave*173);
            weight*=.5f;period*=2;
        }
        return clamp((f-.18f)*1.8f);
    }
    void cleanup()
    {
        const GLuint ids[5]={sky,lit[0],lit[1],lit[2],foreground};
        glDeleteTextures(5,ids);sky=foreground=0;for(auto &id:lit)id=0;
    }
    static void line(GpuScene::Vertex *out,unsigned &n,float x,float y,float xx,float yy,float w)
    {
        const float dx=xx-x,dy=yy-y,inv=w/sqrtf(dx*dx+dy*dy+.00001f);
        const float nx=-dy*inv,ny=dx*inv;
        const GpuScene::Vertex q[4]={{x+nx,y+ny,0,0},{x-nx,y-ny,0,0},
            {xx+nx,yy+ny,0,0},{xx-nx,yy-ny,0,0}};
        const int order[6]={0,1,2,1,3,2};for(int i=0;i<6;++i)out[n++]=q[order[i]];
    }
    static void normal(const StormModel::Segment &s,float &nx,float &ny)
    {
        const float dx=s.xx-s.x,dy=s.yy-s.y,inv=1/sqrtf(dx*dx+dy*dy+.00001f);
        nx=-dy*inv;ny=dx*inv;
    }
    static void join(float nx,float ny,float otherX,float otherY,float width,float &outX,float &outY)
    {
        const float x=nx+otherX,y=ny+otherY,inv=1/sqrtf(x*x+y*y+.00001f);
        const float dot=(x*nx+y*ny)*inv;
        const float scale=width/(dot>.4f?dot:.4f);
        outX=x*inv*scale;outY=y*inv*scale;
    }
    void boltLine(const StormModel::Bolt &b,int i,float width,unsigned &n)
    {
        const auto &s=b.segments[i];float nx,ny;normal(s,nx,ny);
        float ax=nx*width*s.width,ay=ny*width*s.width,bx=ax,by=ay;
        // Continuous mitered strips prevent gaps/rectangular patches in wide
        // bloom at each jagged joint. Branches retain their own tapered strip.
        if(i>0)
        {
            const auto &p=b.segments[i-1];
            if(p.xx==s.x&&p.yy==s.y){float px,py;normal(p,px,py);join(nx,ny,px,py,width*(s.width+p.width)*.5f,ax,ay);}
        }
        if(i+1<b.count)
        {
            const auto &q=b.segments[i+1];
            if(s.xx==q.x&&s.yy==q.y){float qx,qy;normal(q,qx,qy);join(nx,ny,qx,qy,width*(s.width+q.width)*.5f,bx,by);}
        }
        const GpuScene::Vertex q[4]={{s.x+ax,s.y+ay,0,0},{s.x-ax,s.y-ay,0,0},
            {s.xx+bx,s.yy+by,0,0},{s.xx-bx,s.yy-by,0,0}};
        const int order[6]={0,1,2,1,3,2};for(int j=0;j<6;++j)geometry[n++]=q[order[j]];
    }
    void bolts(GpuScene *scene,bool front)
    {
        const float widths[8]={32,24,17,11,7,4,2,.65f};
        const float strength[8]={.006f,.009f,.016f,.035f,.06f,.1f,.2f,.95f};
        for(int layer=0;layer<8;++layer)for(const auto &b:model.bolts)
        {
            const float exposure=StormModel::brightness(b);
            if(b.front!=front||exposure<.008f)continue;
            unsigned n=0;
            for(int i=0;i<b.count;++i)
                boltLine(b,i,widths[layer],n);
            const Color color=layer==7?Color{235,245,255}:(layer>4?Color{175,215,255}:Color{85,135,255});
            scene->drawSolidMesh(geometry,n,color,exposure*strength[layer]*(front?1:.6f));
        }
    }
public:
    bool Init(GpuScene *scene)
    {
        constexpr unsigned pixels=width*height;
        auto density=static_cast<float *>(malloc(pixels*sizeof(float)));
        auto image=static_cast<unsigned char *>(malloc(pixels*4));
        if(!density||!image){free(density);free(image);return false;}
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)
            density[y*width+x]=cloud((float)x/width,(float)y/height,0x7bf193u);
        for(int pass=0;pass<5;++pass)
        {
            for(int y=0;y<height;++y)for(int x=0;x<width;++x)
            {
                const unsigned i=y*width+x;
                const float d=density[i];
                const float slope=clamp(.42f+(d-density[((y+height-5)%height)*width+(x+4)%width])*8);
                if(pass==0)
                {
                    const float body=d*d;
                    image[i*4]=(unsigned char)(7+body*34+slope*d*25);
                    image[i*4+1]=(unsigned char)(12+body*40+slope*d*31);
                    image[i*4+2]=(unsigned char)(22+body*49+slope*d*39);
                    image[i*4+3]=255;
                }
                else if(pass<4)
                {
                    float dx=(float)x/width-(.17f+(pass-1)*.33f);
                    if(dx>.5f)dx-=1;if(dx<-.5f)dx+=1;
                    const float dy=(float)y/height-.28f;
                    const float light=expf(-(dx*dx*24+dy*dy*12))*(.15f+d*.6f+slope*d*.4f);
                    image[i*4]=(unsigned char)(125*light);
                    image[i*4+1]=(unsigned char)(155*light);
                    image[i*4+2]=(unsigned char)(210*light);
                    image[i*4+3]=255;
                }
                else
                {
                    const float front=cloud((float)x/width,(float)y/height,0x2fd391u);
                    const float alpha=clamp((front-.22f)*1.15f)*.78f;
                    // Premultiplied foreground clouds occlude internal bolts.
                    image[i*4]=(unsigned char)((10+front*26)*alpha);
                    image[i*4+1]=(unsigned char)((15+front*29)*alpha);
                    image[i*4+2]=(unsigned char)((25+front*33)*alpha);
                    image[i*4+3]=(unsigned char)(255*alpha);
                }
            }
            const GLuint id=scene->createTexture(width,height,image,true);
            if(pass==0)sky=id;else if(pass<4)lit[pass-1]=id;else foreground=id;
            if(!id){free(image);free(density);cleanup();return false;}
        }
        free(image);free(density);model.init();
        if(glGetError()!=GL_NO_ERROR){cleanup();return false;}
        return true;
    }
    void draw(GpuScene *scene,const AudioSpectrum &s,float gain,bool paused,float dt,bool rain)
    {
        model.update(s,gain,dt,paused,rain);
        const float shift=model.time*.008f;
        const GpuScene::Vertex quad[6]={{0,0,shift,0},{1920,0,shift+1,0},{0,1080,shift,1},
            {1920,0,shift+1,0},{1920,1080,shift+1,1},{0,1080,shift,1}};
        scene->drawMesh(sky,quad,6);
        const GpuScene::Sprite distant={0,0,1920,1080,shift,0,shift+1,1,model.distantFlash()};
        if(distant.opacity>.004f)scene->drawSprites(lit[model.distantZone],&distant,1);
        const GpuScene::Sprite light={0,0,1920,1080,shift,0,shift+1,1,model.flash()};
        if(light.opacity>.004f)scene->drawSprites(lit[model.zone],&light,1);
        bolts(scene,false);
        // A nearer layer moves at a different speed, producing cloud parallax.
        const GpuScene::Sprite fog={0,0,1920,1080,model.time*.013f,model.time*.003f,
            model.time*.013f+1,model.time*.003f+1,.83f};
        scene->drawSprites(foreground,&fog,1,false);
        bolts(scene,true);
        if(rain)for(int group=0;group<3;++group)
        {
            unsigned n=0;
            for(const auto &d:model.drops)
            {
                const int bucket=d.depth<.5f?0:(d.depth<.75f?1:2);
                if(bucket!=group)continue;
                line(geometry,n,d.x,d.y,d.x-d.length*.16f,d.y+d.length,.35f+d.depth*.5f);
            }
            scene->drawSolidMesh(geometry,n,Color{110,145,190},.1f+group*.07f+model.flash()*.12f+model.distantFlash()*.06f);
        }
    }
};
