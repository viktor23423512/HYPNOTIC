#ifdef HYPNOTIC_GPU_DIAGNOSTIC
#include "gpu_scene.h"
#include "status_text.h"
#include "performance_overlay.h"
#include "../common/log.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
std::stringstream debugLogStream;

// Keep a real VideoOut import in the startup dependency/import tables so
// GoldHEN can discover it before Piglet is loaded. Never submit a fake flip:
// Piglet retains sole ownership of presentation and its registered buffers.
static decltype(&sceVideoOutSubmitFlip) volatile earlyVideoOutImport=&sceVideoOutSubmitFlip;

// Local PS4 diagnostic notifications, not network messages. Each records the
// last attempted initialization stage even if EGL later crashes.
static void reportStage(const char *text)
{
    OrbisNotificationRequest request={};
    request.type=NotificationRequest;
    request.targetId=-1;
    snprintf(request.message,sizeof(request.message),"HYPNOTIC 0.58 %s",text);
    printf("%s\n",request.message);
    sceKernelSendNotificationRequest(0,&request,sizeof(request),0);
    sceKernelUsleep(1200000);
}
int main()
{
    setvbuf(stdout,NULL,_IONBF,0);
    printf("Early VideoOut SubmitFlip import: %016llX\n",
        (unsigned long long)(uintptr_t)earlyVideoOutImport);
    reportStage("GPU 1 - MAIN REACHED - NO AUDIO OR USB");
    auto scene=new GpuScene;
    if(!scene->Init(false,reportStage))
    {
        // Repeat the final diagnosis so the user does not have to catch it once.
        for(;;) { reportStage(scene->error()); sceKernelUsleep(6800000); }
    }
    reportStage("GPU 6 - STARTING TEXTURED DRAW TEST");
    // The previous test only sampled constant atlas colors. This checkerboard
    // exercises varying UVs, including a non-rectangular, two-triangle mesh.
    unsigned char checker[16*16*4];
    for(int y=0;y<16;++y) for(int x=0;x<16;++x)
    {
        const int offset=(y*16+x)*4;
        const bool bright=((x/2+y/2)&1)!=0;
        checker[offset]=bright?240:20;
        checker[offset+1]=bright?240:70;
        checker[offset+2]=bright?255:100;
        checker[offset+3]=255;
    }
    const GLuint checkerTexture=scene->createTexture(16,16,checker,false);
    bool notified=false;
    PerformanceOverlay performance;
    performance.Init();
    unsigned frame=0;
    for(;;)
    {
        scene->Begin();
        scene->DrawRectangle(48,260,1824,490,Color{4,10,16});
        drawStatusText(scene,"HYPNOTIC 0.58 - GPU TEXTURE COORDINATE TEST",300);
        drawStatusText(scene,"EXPECT FOUR COLORS AND TWO CHECKERBOARDS BELOW",348);
        const Color colors[4]={{0,220,255},{255,35,195},{90,255,65},{255,170,30}};
        for(int i=0;i<4;++i) scene->DrawRectangle(96+i*440,420,360,160,colors[i]);
        scene->DrawRectangle(96,630,1728,12,Color{20,35,44});
        scene->DrawRectangle(96+(frame%420)*4,626,48,20,Color{0,220,255});
        drawStatusText(scene,"EXPECTED - CYAN MAGENTA GREEN AMBER - MOVING CYAN MARKER",700);
        performance.Draw(scene,800);
        const float shift=40*sinf(frame*0.025f);
        const GpuScene::Vertex mesh[6]={
            {1220+shift,760,0,0},{1770,780,1,0},{1260,980,0,1},
            {1770,780,1,0},{1730-shift,960,1,1},{1260,980,0,1}};
        scene->drawMesh(checkerTexture,mesh,6);
        const GpuScene::Vertex rectangle[6]={
            {96,860,0,0},{576,860,1,0},{96,980,0,1},
            {576,860,1,0},{576,980,1,1},{96,980,0,1}};
        scene->drawMesh(checkerTexture,rectangle,6);
        drawStatusText(scene,"UV TEST - LEFT RECTANGLE - RIGHT MOVING QUAD - BOTH CHECKERED",1010);
        if(!scene->Present())
        {
            for(;;) { reportStage(scene->error()); sceKernelUsleep(6800000); }
        }
        if(!notified) { reportStage("GPU 7 - FIRST TEXTURED FRAME PRESENTED"); notified=true; }
        performance.Presented();
        ++frame;
    }
}
#endif
