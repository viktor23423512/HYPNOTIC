#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "fractal_dive_model.h"

class GpuFractalDive
{
    FractalDiveModel model;
    struct Line {float x,y,xx,yy;unsigned group;};
    static constexpr unsigned maxLines=10000;
    Line lines[maxLines];unsigned lineCount=0,nodes=0,used=0;
    GpuScene::Vertex geometry[49152];
    struct Job {unsigned seed,skip,depth;float x,y,scale;};
    Job queue[1024];unsigned pending=0;
    float pixels=900;
    static bool clip(float p,float q,float &first,float &last)
    {
        if(p==0)return q>=0;
        const float r=q/p;
        if(p<0){if(r>last)return false;if(r>first)first=r;}
        else{if(r<first)return false;if(r<last)last=r;}
        return true;
    }
    void add(float x,float y,float xx,float yy,unsigned group)
    {
        if(lineCount==maxLines)return;
        if((x< -20&&xx< -20)||(x>1940&&xx>1940)||(y< -20&&yy< -20)||(y>1100&&yy>1100))return;
        float first=0,last=1;const float dx=xx-x,dy=yy-y;
        if(!clip(-dx,x+10,first,last)||!clip(dx,1930-x,first,last)
            ||!clip(-dy,y+10,first,last)||!clip(dy,1090-y,first,last))return;
        lines[lineCount++]={x+first*dx,y+first*dy,x+last*dx,y+last*dy,group};
    }
    void ornament(unsigned seed,float x,float y,float scale)
    {
        if(lineCount==maxLines)return;
        const unsigned count=FractalDiveModel::childCount(seed);
        const float direction=FractalDiveModel::hash(seed+119)&1?1.0f:-1.0f;
        const float turns=.8f+FractalDiveModel::unit(seed+89)*.8f;
        int steps=(int)(scale*pixels*.25f);if(steps<10)steps=10;if(steps>160)steps=160;
        for(unsigned arm=0;arm<count;++arm)
        {
            if(lineCount==maxLines)return;
            const auto c=FractalDiveModel::child(seed,arm);
            const float centreX=960+(x+scale*c.x-model.cameraX)*pixels,
                centreY=540+(y+scale*c.y-model.cameraY)*pixels;
            const float start=FractalDiveModel::unit(c.seed+31)*6.283185307f;
            const float outer=scale*pixels*(.28f+FractalDiveModel::unit(c.seed+43)*.055f);
            float px=0,py=0;
            for(int i=0;i<=steps;++i)
            {
                const float t=(float)i/steps;
                const float angle=start+direction*t*turns*6.283185307f;
                const float radius=outer*(1-.77f*t);
                const float a=centreX+radius*sinf(angle+1.570796327f),
                    b=centreY+radius*sinf(angle);
                if(i)add(px,py,a,b,(FractalDiveModel::hash(c.seed+97)+(unsigned)(t*3))&3);
                px=a;py=b;
            }
        }
    }
    void enqueue(unsigned seed,float x,float y,float scale,unsigned depth,unsigned skip=~0u)
    {
        if(pending>=1024||depth>=10)return;
        const float px=960+(x-model.cameraX)*pixels,py=540+(y-model.cameraY)*pixels;
        const float radius=scale*pixels*1.15f;
        if(radius<10||px+radius< -20||px-radius>1940||py+radius< -20||py-radius>1100)return;
        unsigned at=pending++;
        while(at){const unsigned parent=(at-1)/2;if(queue[parent].scale>=scale)break;queue[at]=queue[parent];at=parent;}
        queue[at]={seed,skip,depth,x,y,scale};
    }
    Job pop()
    {
        const Job out=queue[0],last=queue[--pending];unsigned at=0;
        while(at*2+1<pending)
        {
            unsigned child=at*2+1;if(child+1<pending&&queue[child+1].scale>queue[child].scale)++child;
            if(queue[child].scale<=last.scale)break;queue[at]=queue[child];at=child;
        }
        queue[at]=last;return out;
    }
    void segment(const Line &s,float width)
    {
        const float dx=s.xx-s.x,dy=s.yy-s.y,inv=width/sqrtf(dx*dx+dy*dy+.00001f);
        const float nx=-dy*inv,ny=dx*inv;
        const GpuScene::Vertex q[4]={{s.x+nx,s.y+ny,0,0},{s.x-nx,s.y-ny,0,0},
            {s.xx+nx,s.yy+ny,0,0},{s.xx-nx,s.yy-ny,0,0}};
        const int order[6]={0,1,2,1,3,2};for(int i=0;i<6;++i)geometry[used++]=q[order[i]];
    }
public:
    void seed(unsigned value){model.seed(value);}
    void draw(GpuScene *scene,float dt,bool paused,float speed,int palette)
    {
        model.update(dt,paused,speed);pixels=900*model.zoom;nodes=lineCount=pending=0;
        enqueue(model.rootSeed,0,0,1,0);
        // Retain nearby parents and off-path siblings when re-rooting, so
        // the surrounding picture does not disappear at a level boundary.
        for(int a=(int)model.ancestorCount-1;a>=0;--a)
        {
            const auto &p=model.ancestors[a];
            enqueue(p.seed,p.x,p.y,p.scale,0,p.selected);
        }
        // Largest visible structures first. This prevents fine descendants of
        // one branch consuming the budget before the rest of the scene exists.
        while(pending&&nodes<600&&lineCount<maxLines)
        {
            const Job j=pop();++nodes;ornament(j.seed,j.x,j.y,j.scale);
            for(unsigned i=0;i<FractalDiveModel::childCount(j.seed);++i)if(i!=j.skip)
            {
                const auto c=FractalDiveModel::child(j.seed,i);
                enqueue(c.seed,j.x+j.scale*c.x,j.y+j.scale*c.y,j.scale*c.scale,j.depth+1);
            }
        }
        const Color colors[4][4]={
            {{45,165,255},{65,235,225},{175,85,255},{250,125,215}},
            {{255,155,40},{255,220,95},{245,75,125},{190,85,245}},
            {{45,230,130},{155,245,75},{35,175,235},{155,220,255}},
            {{155,185,255},{220,240,255},{105,160,245},{215,145,255}}};
        const float widths[3]={4.5f,1.7f,.6f},opacity[3]={.035f,.13f,.48f};
        for(unsigned layer=0;layer<3;++layer)for(unsigned group=0;group<4;++group)
        {
            used=0;
            for(unsigned i=0;i<lineCount;++i)if(lines[i].group==group)
            {
                segment(lines[i],widths[layer]);
                if(used==49152){scene->drawSolidMesh(geometry,used,colors[palette&3][group],opacity[layer]);used=0;}
            }
            if(used)scene->drawSolidMesh(geometry,used,colors[palette&3][group],opacity[layer]);
        }
    }
};
