#pragma once
#include "status_text.h"
#include <orbis/libkernel.h>
#include <stdio.h>

// Successful application presentations, averaged over at least one second.
// Sensor exports are optional: unavailable/denied readings stay visibly N/A.
class PerformanceOverlay
{
    typedef int (*CpuReader)(int *);
    typedef int (*SocReader)(int, int *);
    CpuReader readCpu=nullptr;
    SocReader readSoc=nullptr;
    uint64_t start=0, sensorTime=0;
    unsigned frames=0;
    bool started=false, sensorsSampled=false, measured=false;
    double fps=0;
    int cpu=-1, soc=-1;
public:
    void Init()
    {
        OrbisKernelModule modules[128];
        size_t count=0;
        if(sceKernelGetModuleList(modules,sizeof(modules),&count)!=0) return;
        if(count>128) count=128;
        for(size_t i=0;i<count;++i)
        {
            void *symbol=nullptr;
            if(!readCpu && sceKernelDlsym(modules[i],"sceKernelGetCpuTemperature",&symbol)==0 && symbol)
                readCpu=reinterpret_cast<CpuReader>(symbol);
            symbol=nullptr;
            if(!readSoc && sceKernelDlsym(modules[i],"sceKernelGetSocSensorTemperature",&symbol)==0 && symbol)
                readSoc=reinterpret_cast<SocReader>(symbol);
        }
    }
    void ResetAveraging(){start=sceKernelGetProcessTime();frames=0;started=true;measured=false;fps=0;}
    void Presented()
    {
        const uint64_t now=sceKernelGetProcessTime();
        if(!started) { start=now; started=true; return; }
        ++frames;
        if(now-start>=1000000)
        {
            fps=frames*1000000.0/(now-start);
            measured=true;
            frames=0;
            start=now;
        }
    }
    template<class Scene> void Draw(Scene *scene,int y=1024)
    {
        const uint64_t now=sceKernelGetProcessTime();
        if(!sensorsSampled || now-sensorTime>=1000000)
        {
            int value=0;
            cpu=(readCpu && readCpu(&value)==0 && value>0 && value<=125)?value:-1;
            value=0;
            soc=(readSoc && readSoc(0,&value)==0 && value>0 && value<=125)?value:-1;
            sensorTime=now;
            sensorsSampled=true;
        }
        char rate[32], cpuText[24], socText[24], line[112];
        if(measured) snprintf(rate,sizeof(rate),"FPS %.1f",fps);
        else snprintf(rate,sizeof(rate),"FPS MEASURING");
        if(cpu>=0) snprintf(cpuText,sizeof(cpuText),"CPU %d C",cpu);
        else snprintf(cpuText,sizeof(cpuText),"CPU N/A");
        if(soc>=0) snprintf(socText,sizeof(socText),"SOC %d C",soc);
        else snprintf(socText,sizeof(socText),"SOC N/A");
        snprintf(line,sizeof(line),"%s - %s - %s",rate,cpuText,socText);
        scene->DrawRectangle(48,y-5,1100,34,Color{4,10,16});
        drawStatusText(scene,line,y);
    }
};
