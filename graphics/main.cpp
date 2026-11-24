#ifndef HYPNOTIC_GPU_DIAGNOSTIC
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/Sysmodule.h>

#include "../common/graphics.h"
#include "../common/log.h"
#include "audio.h"
#include "touchpad_routing.h"
#include "music_navigation.h"
#include "music_menu.h"
#include "music_search_input.h"
static MusicSearchInput musicSearch;
static bool presetSearchTarget=false;
static MusicFolderBrowser musicBrowser;
static MusicNavigation musicNavigation;
static TouchpadRouting touchpadRouting;
#include "milkdrop_player.h"
#include "milkdrop_bridge_timing.h"
#include "milkdrop_browser.h"
#include "gpu_scene.h"
#include "ui_style.h"
#include "settings_menu.h"
using UiScene=ThemedScene<GpuScene>;
#include "gpu_wormhole.h"
#include "gpu_kaleidoscope.h"
#include "gpu_matrix_eq.h"
#include "gpu_cymatics.h"
#include "gpu_cube_orbit.h"
#include "builtin_settings.h"
#include "gpu_xy_scope.h"
#include "gpu_liquid_aura.h"
#include "gpu_storm.h"
#define Scene2D GpuScene
#include "radial_preset.h"
#undef Scene2D
#include "trigger_scrub.h"
#include "microphone.h"
#include "filesystem_access.h"
#include "status_text.h"
#include "performance_overlay.h"
#include "player_hud.h"
#include "preset_cycle.h"
#include "menu_repeat.h"
#include "visualizer_menu.h"
#include "ui_preferences.h"
static PresetCycleTimer presetCycle;

// Dimensions
#define FRAME_WIDTH     1920
#define FRAME_HEIGHT    1080
#define FRAME_DEPTH        4

// Logging
std::stringstream debugLogStream;

int frameID = 0;
static GpuWormhole gpuWormhole;
static bool gpuWormholeReady=false;
static bool gpuWormholeAttempted=false;
static GpuKaleidoscope gpuKaleidoscope;
static bool gpuKaleidoscopeReady=false,gpuKaleidoscopeAttempted=false;
static GpuMatrixEq gpuMatrixEq;
static MatrixEqSettings matrixSettings;
static bool gpuMatrixReady=false,gpuMatrixAttempted=false;
static GpuCymatics gpuCymatics;
static GpuCubeOrbit gpuCubeOrbit;
static CubeOrbitSettings cubeOrbitSettings;
static bool gpuCymaticsReady=false,gpuCymaticsAttempted=false;
static GpuXyScope gpuXyScope;
static GpuLiquidAura gpuLiquidAura;
static bool gpuLiquidReady=false,gpuLiquidAttempted=false;
static GpuStorm gpuStorm;
static bool gpuStormReady=false,gpuStormAttempted=false;

// Routine diagnostics go to the log without popups or artificial delays.
static void reportStartup(const char *text)
{
    printf("HYPNOTIC 0.84 %s\n",text);
}

static void reportFailure(const char *text)
{
    OrbisNotificationRequest request={};
    request.type=NotificationRequest;
    request.targetId=-1;
    snprintf(request.message,sizeof(request.message),"HYPNOTIC 0.84 %s",text);
    printf("%s\n",request.message);
    sceKernelSendNotificationRequest(0,&request,sizeof(request),0);
}

static void holdStartupFailure(const char *text)
{
    reportFailure(text);
    for(;;) sceKernelUsleep(8000000);
}

static bool loadPlayerModule(OrbisSysModuleInternal id,const char *name,
    const char *const *exports,unsigned count)
{
    const int32_t result=(int32_t)sceSysmoduleLoadModuleInternal(id);
    char message[192];
    snprintf(message,sizeof(message),"PLAYER MODULE %s LOAD %08X",name,(unsigned)result);
    reportStartup(message);
    // A module may already be loaded. Check its exports rather than guessing
    // which negative status represents that case on a particular firmware.
    OrbisKernelModule handles[128];
    size_t available=0;
    if(sceKernelGetModuleList(handles,sizeof(handles),&available)!=0) return false;
    if(available>128) available=128;
    for(size_t i=0;i<available;++i)
    {
        OrbisKernelModuleInfo info={};
        info.size=sizeof(info);
        if(sceKernelGetModuleInfo(handles[i],&info)!=0 || !strstr(info.name,name)) continue;
        for(unsigned e=0;e<count;++e)
        {
            void *address=nullptr;
            if(sceKernelDlsym(handles[i],exports[e],&address)!=0 || !address)
            {
                snprintf(message,sizeof(message),"PLAYER MISSING EXPORT %s",exports[e]);
                reportStartup(message);
                return false;
            }
        }
        return true;
    }
    return false;
}

static void loadPlayerModules()
{
    const char *user[]={"sceUserServiceInitialize","sceUserServiceGetInitialUser"};
    const char *pad[]={"scePadInit","scePadOpen","scePadReadState"};
    const char *audio[]={"sceAudioOutInit","sceAudioOutOpen","sceAudioOutOutput","sceAudioOutClose"};
    if(!loadPlayerModule(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE,"libSceUserService",user,2))
        holdStartupFailure("PLAYER USER SERVICE MODULE UNAVAILABLE");
    if(!loadPlayerModule(ORBIS_SYSMODULE_INTERNAL_PAD,"libScePad",pad,3))
        holdStartupFailure("PLAYER PAD MODULE UNAVAILABLE");
    if(!loadPlayerModule(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT,"libSceAudioOut",audio,4))
        holdStartupFailure("PLAYER AUDIO OUT MODULE UNAVAILABLE");
}

static void showPresetLoading(GpuScene *scene,const char *name)
{
    scene->Begin();
    char text[96];
    snprintf(text,sizeof(text),"PREPARING %s",name);
    drawStatusText(scene,text,440);
    if(!scene->Present()) holdStartupFailure(scene->error());
    reportStartup(text);
}

static MilkdropBrowser milkdropBrowser;
static GLuint milkdropTexture=0;
static MilkdropBridgeTiming milkdropBridgeTiming;
static unsigned char milkdropUpload[1920*1080*4];

struct VisualizerState
{
    bool paused = false;
    int hudSelection=3;
    HudVisibility hud;
    bool diagnostics=false;
    bool microphone=false;
    bool menuOpen = true;
    bool visualizerMenuOpen = false;
    bool appSettingsOpen=false,appCreditsOpen=false,appMicTestOpen=false,settingsReturnMusic=false,settingsReturnVisualizers=false;int appSettingsRow=0,appSettingsTab=0,appSettingsRows[4]={},appEqRow=0;bool appEqOpen=false,volumeOpen=false;
    bool matrixSettingsOpen=false,matrixSettingsReturn=false;
    bool builtinSettingsOpen=false,builtinSettingsReturn=false;int builtinSettingsPreset=5,builtinSettingsRow=0;
    int matrixSettingSelection=0;
    int selection = 0;
    int presetSelection = 0;
    bool milkdropLibrary=false;
    int pendingMilkdrop=-1;
    bool cancelMilkdrop=false,presetPreparing=false;
    bool milkdropFault=false;
    int palette = 0;
    int preset = 0;
    int sensitivity = 2; // Visual gain: 0.5x through 2x, default 1x
    int sandVariation=1;
    uint64_t sandHintUntil=0;
    bool scopeMidSide=true;
    int scopeTrail=2;
    uint64_t scopeHintUntil=0;
    bool stormRain=true;
    uint64_t stormHintUntil=0;
};

static constexpr int presetCount=10;
static const char *presetNames[presetCount]={"WAVEFORM","NEON HALO","WORMHOLE","KALEIDOSCOPE","MATRIX EQ","CYMATICS SAND","OSCILLOSCOPE","LIQUID AURA","STORM SKY","CUBE ORBIT EQ"};

static VisualizerLibrary visualizerLibrary(milkdropBrowser,presetNames,presetCount);
static void recordVisualizer(VisualizerState &state){visualizerLibrary.applied(state.preset==11?visualizerLibrary.resolve(milkdropBrowser.activePath):-state.preset-1,sceKernelGetProcessTime());}

static void cycleVisualizer(VisualizerState &state,int direction){
 if(state.preset==11){int next=milkdropBrowser.cycle(direction);if(next>=0)state.pendingMilkdrop=next;}
 else{state.cancelMilkdrop=true;state.preset=(state.preset+presetCount+direction)%presetCount;state.presetSelection=state.preset;recordVisualizer(state);}
 presetCycle.reset();
}

static void warmNextBuiltin(GpuScene *scene,const VisualizerState &state){
 if(!presetCycle.enabled||state.preset>=11||state.microphone)return;
 const int next=(state.preset+1)%presetCount;const uint64_t start=sceKernelGetProcessTime();bool attempted=false;
 if(next==2&&!gpuWormholeAttempted){attempted=gpuWormholeAttempted=true;gpuWormholeReady=gpuWormhole.Init(scene);}
 if(next==3&&!gpuKaleidoscopeAttempted){attempted=gpuKaleidoscopeAttempted=true;gpuKaleidoscopeReady=gpuKaleidoscope.Init(scene);}
 if(next==4&&!gpuMatrixAttempted){attempted=gpuMatrixAttempted=true;gpuMatrixReady=gpuMatrixEq.Init(scene);}
 if(next==5&&!gpuCymaticsAttempted){attempted=gpuCymaticsAttempted=true;gpuCymaticsReady=gpuCymatics.Init(scene);}
 if(next==7&&!gpuLiquidAttempted){attempted=gpuLiquidAttempted=true;gpuLiquidReady=gpuLiquidAura.Init(scene);}
 if(next==8&&!gpuStormAttempted){attempted=gpuStormAttempted=true;gpuStormReady=gpuStorm.Init(scene);}
 if(attempted)printf("HYPNOTIC BUILTIN PREWARM %d %llu US\n",next,(unsigned long long)(sceKernelGetProcessTime()-start));
}

int openController()
{
    reportStartup("PLAYER 8A - PAD INIT");
    if (scePadInit() != 0)
    {
        DEBUGLOG << "Pad initialization failed; continuing without controls";
        return -1;
    }
    OrbisUserServiceInitializeParams params = {};
    params.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    reportStartup("PLAYER 8B - USER SERVICE INIT");
    const int result = sceUserServiceInitialize(&params);
    if (result < 0)
        DEBUGLOG << "User service initialization returned: " << result;
    int32_t user = ORBIS_USER_SERVICE_USER_ID_INVALID;
    reportStartup("PLAYER 8C - GET INITIAL USER");
    if (sceUserServiceGetInitialUser(&user) < 0)
    {
        DEBUGLOG << "Unable to find controller user";
        return -1;
    }
    reportStartup("PLAYER 8D - OPEN PAD");
    const int pad = scePadOpen(user, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);
    if (pad < 0)
        DEBUGLOG << "Unable to open controller: " << pad;
    OrbisPadInformation info={};if(scePadGetControllerInformation(pad,&info)==0)touchpadRouting.setWidth(info.touchResolutionX);
    return pad;
}

void updateControls(int pad, VisualizerState &state, uint32_t &previousButtons)
{
    static TriggerScrub scrub;
    static MenuRepeat repeat;
    static uint64_t lastInputTime = 0;
    const uint64_t now = sceKernelGetProcessTime();
    const uint64_t delta = lastInputTime ? now - lastInputTime : 0;
    lastInputTime = now;
    OrbisPadData data = {};
    if (pad < 0 || scePadReadState(pad, &data) < 0 || !data.connected)
    {
        previousButtons = 0;touchpadRouting.reset();
        repeat.reset();
        scrub.reset();
        return;
    }
    touchpadRouting.observe(data.touch.fingers,data.touch.touch[0].x,now);
    // Act only on new presses, rather than repeating every rendered frame.
    const uint32_t pressed = data.buttons & ~previousButtons;
    previousButtons = data.buttons;
    const uint32_t directions=ORBIS_PAD_BUTTON_UP|ORBIS_PAD_BUTTON_DOWN|ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT;
    int context=state.volumeOpen?7:musicSearch.open?1:presetCycle.menuOpen?2:(state.matrixSettingsOpen||state.builtinSettingsOpen||state.appSettingsOpen)?3:state.visualizerMenuOpen?4:state.menuOpen?5:6;
    const uint32_t nav=pressed|repeat.update(context!=6?data.buttons:0,directions,ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT,now,context);
    const bool noMenu=!state.volumeOpen&&!state.menuOpen&&!state.visualizerMenuOpen&&!state.matrixSettingsOpen&&!state.builtinSettingsOpen&&!state.appSettingsOpen&&!musicSearch.open&&!presetCycle.menuOpen;
    const bool wasHud=state.hud.visible(now,state.paused);
    if(noMenu && (pressed||data.analogButtons.l2>24||data.analogButtons.r2>24)){
        if(!wasHud)state.hudSelection=3;state.hud.reveal(now);
    }
    if((data.buttons&(ORBIS_PAD_BUTTON_L3|ORBIS_PAD_BUTTON_R3))==(ORBIS_PAD_BUTTON_L3|ORBIS_PAD_BUTTON_R3) && (pressed&(ORBIS_PAD_BUTTON_L3|ORBIS_PAD_BUTTON_R3))){state.diagnostics=!state.diagnostics;milkdropBridgeTiming.debug(state.diagnostics,now);scrub.reset();return;}
    if(noMenu && (pressed&ORBIS_PAD_BUTTON_L3)){state.hud.toggle(now);scrub.reset();return;}
    if(noMenu&&state.diagnostics&&(pressed&ORBIS_PAD_BUTTON_SQUARE)){visualizerLibrary.renderHD=!visualizerLibrary.renderHD;visualizerLibrary.touch(now);milkdropPlayerRequestHD(visualizerLibrary.renderHD);return;}
    if(pressed&(ORBIS_PAD_BUTTON_OPTIONS|ORBIS_PAD_BUTTON_TOUCH_PAD|ORBIS_PAD_BUTTON_TRIANGLE))state.volumeOpen=false;
    if(pressed&ORBIS_PAD_BUTTON_OPTIONS){
        state.appEqOpen=false;
        if(state.appMicTestOpen){pauseMicrophone(!state.microphone);state.appMicTestOpen=false;}
        
    if(state.appSettingsOpen){state.appSettingsOpen=false;state.appCreditsOpen=false;state.menuOpen=state.settingsReturnMusic;state.visualizerMenuOpen=state.settingsReturnVisualizers;}
        else{state.settingsReturnMusic=state.menuOpen;state.settingsReturnVisualizers=state.visualizerMenuOpen;state.appSettingsOpen=true;state.appCreditsOpen=false;state.menuOpen=state.visualizerMenuOpen=false;state.matrixSettingsOpen=state.builtinSettingsOpen=false;musicSearch.open=false;presetCycle.menuOpen=false;}
        scrub.reset();return;
    }
    if((pressed&ORBIS_PAD_BUTTON_TOUCH_PAD)||((pressed&ORBIS_PAD_BUTTON_L3)&&(state.menuOpen||state.visualizerMenuOpen))){
        const bool search=(pressed&ORBIS_PAD_BUTTON_L3)||touchpadRouting.searchSide(now);
        const bool visualSearch=state.visualizerMenuOpen;
        if(state.appMicTestOpen){pauseMicrophone(!state.microphone);state.appMicTestOpen=false;}
        state.appSettingsOpen=state.appCreditsOpen=state.appEqOpen=false;state.matrixSettingsOpen=state.builtinSettingsOpen=false;presetCycle.menuOpen=false;
        if(search){presetSearchTarget=visualSearch;if(!visualSearch){state.menuOpen=true;state.visualizerMenuOpen=false;}musicSearch.start(presetSearchTarget?visualizerLibrary.query.c_str():musicNavigation.searchResults?musicBrowser.query:"");}
        else{state.menuOpen=true;state.visualizerMenuOpen=false;musicSearch.open=false;}
        scrub.reset();return;
    }
    if(state.volumeOpen){scrub.reset();auto &audio=visualizerLibrary.appearance.audio;if(nav&(ORBIS_PAD_BUTTON_UP|ORBIS_PAD_BUTTON_DOWN)){audio.adjustVolume((nav&ORBIS_PAD_BUTTON_UP)?1:-1);visualizerLibrary.touch(now);}if(pressed&ORBIS_PAD_BUTTON_CIRCLE){audio.muted=!audio.muted;visualizerLibrary.touch(now);}if(pressed&ORBIS_PAD_BUTTON_CROSS)state.volumeOpen=false;state.hud.reveal(now);return;}
    if(state.appSettingsOpen){
        scrub.reset();
        if(state.appMicTestOpen){if(pressed&(ORBIS_PAD_BUTTON_CIRCLE|ORBIS_PAD_BUTTON_CROSS)){pauseMicrophone(!state.microphone);state.appMicTestOpen=false;}return;}
        if(state.appCreditsOpen){if(pressed&(ORBIS_PAD_BUTTON_CIRCLE|ORBIS_PAD_BUTTON_CROSS))state.appCreditsOpen=false;return;}
        auto &ui=visualizerLibrary.appearance;
        if(state.appEqOpen){
            if((nav&ORBIS_PAD_BUTTON_LEFT)&&state.appEqRow>0)--state.appEqRow;
            if((nav&ORBIS_PAD_BUTTON_RIGHT)&&state.appEqRow<4)++state.appEqRow;
            int gain=(nav&ORBIS_PAD_BUTTON_UP)?1:(nav&ORBIS_PAD_BUTTON_DOWN)?-1:0;
            if(gain){ui.audio.bands[state.appEqRow]=UiSettings::bound(ui.audio.bands[state.appEqRow]+gain,-12,12);visualizerLibrary.touch(now);}
            if(pressed&ORBIS_PAD_BUTTON_SQUARE){ui.audio.flat();visualizerLibrary.touch(now);}
            if(pressed&ORBIS_PAD_BUTTON_TRIANGLE){ui.audio.eqEnabled=!ui.audio.eqEnabled;visualizerLibrary.touch(now);}
            if(pressed&ORBIS_PAD_BUTTON_CIRCLE)state.appEqOpen=false;return;
        }
        if(pressed&(ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_R1)){state.appSettingsRows[state.appSettingsTab]=state.appSettingsRow;state.appSettingsTab=(state.appSettingsTab+((pressed&ORBIS_PAD_BUTTON_R1)?1:3))%4;state.appSettingsRow=state.appSettingsRows[state.appSettingsTab];return;}
        if((nav&ORBIS_PAD_BUTTON_UP)&&state.appSettingsRow>0)--state.appSettingsRow;
        if((nav&ORBIS_PAD_BUTTON_DOWN)&&state.appSettingsRow<appSettingsRowCount(state.appSettingsTab)-1)++state.appSettingsRow;
        const auto setting=appSettingsAction(state.appSettingsTab,state.appSettingsRow);const int d=(nav&ORBIS_PAD_BUTTON_RIGHT)?1:(nav&ORBIS_PAD_BUTTON_LEFT)?-1:0;
        if(d){switch(setting){case AppSetting::Volume:ui.audio.adjustVolume(d);break;case AppSetting::Style:ui.setStyle(1-ui.style);break;case AppSetting::Colour:ui.theme=(ui.theme+UiSettings::themeCount+d)%UiSettings::themeCount;break;case AppSetting::Font:ui.setFont((ui.font+UiSettings::fontCount+d)%UiSettings::fontCount);break;case AppSetting::HudTimeout:ui.hudSeconds=UiSettings::bound(ui.hudSeconds+d,3,15);state.hud.delay=(uint64_t)ui.hudSeconds*1000000;break;case AppSetting::Sensitivity:state.sensitivity=UiSettings::bound(state.sensitivity+d,0,6);ui.sensitivity=state.sensitivity;break;case AppSetting::Quality:visualizerLibrary.renderHD=!visualizerLibrary.renderHD;milkdropPlayerRequestHD(visualizerLibrary.renderHD);break;case AppSetting::Stats:ui.stats=!ui.stats;break;case AppSetting::DebugComparison:state.diagnostics=!state.diagnostics;milkdropBridgeTiming.debug(state.diagnostics,now);break;default:break;}visualizerLibrary.touch(now);}
        if(pressed&ORBIS_PAD_BUTTON_CROSS){switch(setting){case AppSetting::Volume:ui.audio.muted=!ui.audio.muted;visualizerLibrary.touch(now);break;case AppSetting::Equalizer:state.appEqOpen=true;state.appEqRow=0;break;case AppSetting::DebugComparison:state.diagnostics=!state.diagnostics;milkdropBridgeTiming.debug(state.diagnostics,now);break;case AppSetting::Credits:state.appCreditsOpen=true;break;case AppSetting::MicTest:state.appMicTestOpen=true;pauseMicrophone(false);startMicrophone();break;default:visualizerLibrary.touch(now);break;}}
        if(pressed&ORBIS_PAD_BUTTON_CIRCLE){state.appSettingsOpen=false;state.menuOpen=state.settingsReturnMusic;state.visualizerMenuOpen=state.settingsReturnVisualizers;}
        return;
    }
    if(presetCycle.menuOpen){
        if(nav&ORBIS_PAD_BUTTON_UP)presetCycle.move(-1);
        if(nav&ORBIS_PAD_BUTTON_DOWN)presetCycle.move(1);
        if(nav&ORBIS_PAD_BUTTON_LEFT)presetCycle.adjust(-1);
        if(nav&ORBIS_PAD_BUTTON_RIGHT)presetCycle.adjust(1);
        if(pressed&ORBIS_PAD_BUTTON_CROSS){presetCycle.apply();printf("HYPNOTIC AUTO PRESET MODE %u INTERVAL %u\n",presetCycle.enabled,presetCycle.seconds);}
        if(pressed&ORBIS_PAD_BUTTON_CIRCLE)presetCycle.cancel();scrub.reset();return;
    }
    if(musicSearch.open){
        if(nav&ORBIS_PAD_BUTTON_UP)musicSearch.move(0,-1);
        if(nav&ORBIS_PAD_BUTTON_DOWN)musicSearch.move(0,1);
        if(nav&ORBIS_PAD_BUTTON_LEFT)musicSearch.move(-1,0);
        if(nav&ORBIS_PAD_BUTTON_RIGHT)musicSearch.move(1,0);
        if(pressed&ORBIS_PAD_BUTTON_SQUARE)musicSearch.erase();
        if(pressed&ORBIS_PAD_BUTTON_TRIANGLE)musicSearch.clear();
        if(pressed&ORBIS_PAD_BUTTON_CROSS){if(musicSearch.choose()==1){if(presetSearchTarget)visualizerLibrary.search(musicSearch.text,now);else musicNavigation.search(musicSearch.text,musicBrowser);}}
        if(pressed&ORBIS_PAD_BUTTON_CIRCLE)musicSearch.open=false;
        scrub.reset();return;
    }
    if (pressed & ORBIS_PAD_BUTTON_TRIANGLE)
    {
        state.visualizerMenuOpen = !state.visualizerMenuOpen;
        if(state.visualizerMenuOpen&&!milkdropBrowser.scanned){milkdropBrowser.rescan();visualizerLibrary.rebuild();}
        state.menuOpen = false;
        state.matrixSettingsOpen=false;state.builtinSettingsOpen=false;state.appSettingsOpen=false;
        scrub.reset();
        return;
    }
    if(state.builtinSettingsOpen){
        scrub.reset();const int count=state.builtinSettingsPreset==5?3:state.builtinSettingsPreset==6?4:10;
        if((nav&ORBIS_PAD_BUTTON_UP)&&state.builtinSettingsRow>0)--state.builtinSettingsRow;
        if((nav&ORBIS_PAD_BUTTON_DOWN)&&state.builtinSettingsRow<count-1)++state.builtinSettingsRow;
        int d=(nav&ORBIS_PAD_BUTTON_RIGHT)?1:(nav&ORBIS_PAD_BUTTON_LEFT)?-1:0;
        if(d){int row=state.builtinSettingsRow;if(state.builtinSettingsPreset==5){if(row==0)state.sandVariation=(state.sandVariation+4+d)%4;else if(row==1)state.sensitivity=CubeOrbitSettings::bound(state.sensitivity+d,0,6);else state.palette=(state.palette+4+d)%4;}else if(state.builtinSettingsPreset==6){if(row==0)state.scopeMidSide=!state.scopeMidSide;else if(row==1)state.scopeTrail=(state.scopeTrail+4+d)%4;else if(row==2)state.sensitivity=CubeOrbitSettings::bound(state.sensitivity+d,0,6);else state.palette=(state.palette+4+d)%4;}else{if(row<8)cubeOrbitSettings.adjust(row,d);else if(row==8)state.sensitivity=CubeOrbitSettings::bound(state.sensitivity+d,0,6);else state.palette=(state.palette+4+d)%4;}}
        if(pressed&(ORBIS_PAD_BUTTON_CIRCLE|ORBIS_PAD_BUTTON_SQUARE)){state.builtinSettingsOpen=false;state.visualizerMenuOpen=state.builtinSettingsReturn;}
        return;
    }
    if(state.matrixSettingsOpen)
    {
        scrub.reset();
        if((nav&ORBIS_PAD_BUTTON_UP) && state.matrixSettingSelection>0)--state.matrixSettingSelection;
        if((nav&ORBIS_PAD_BUTTON_DOWN) && state.matrixSettingSelection<8)++state.matrixSettingSelection;
        if(nav&ORBIS_PAD_BUTTON_LEFT)matrixSettings.adjust(state.matrixSettingSelection,-1);
        if(nav&ORBIS_PAD_BUTTON_RIGHT)matrixSettings.adjust(state.matrixSettingSelection,1);
        if(pressed&(ORBIS_PAD_BUTTON_CIRCLE|ORBIS_PAD_BUTTON_SQUARE))
        { state.matrixSettingsOpen=false; state.visualizerMenuOpen=state.matrixSettingsReturn; }
        return;
    }
    if(state.visualizerMenuOpen)
    {
        scrub.reset();
        if(!milkdropBrowser.scanned){milkdropBrowser.rescan();visualizerLibrary.rebuild();}
        if(nav&ORBIS_PAD_BUTTON_LEFT)visualizerLibrary.changeTab(-1,now);
        if(nav&ORBIS_PAD_BUTTON_RIGHT)visualizerLibrary.changeTab(1,now);
        if(nav&ORBIS_PAD_BUTTON_UP)visualizerLibrary.move(-1,now);
        if(nav&ORBIS_PAD_BUTTON_DOWN)visualizerLibrary.move(1,now);
        if(pressed&ORBIS_PAD_BUTTON_R3)visualizerLibrary.toggleFavorite(now);
        const int chosen=visualizerLibrary.selected();
        if(pressed&ORBIS_PAD_BUTTON_SQUARE){
            if(chosen==-6||chosen==-7||chosen==-10){state.builtinSettingsOpen=true;state.builtinSettingsPreset=-chosen-1;state.builtinSettingsRow=0;state.builtinSettingsReturn=true;state.visualizerMenuOpen=false;return;}
            if(chosen==-5){state.matrixSettingsOpen=true;state.matrixSettingsReturn=true;state.visualizerMenuOpen=false;return;}
            if(chosen==-9)state.stormRain=!state.stormRain;
            else if(visualizerLibrary.tab==13){milkdropBrowser.rescan();visualizerLibrary.rebuild();}
        }
        if(pressed&ORBIS_PAD_BUTTON_CROSS){
            milkdropBrowser.error.clear();
            if(chosen>=0 && chosen<(int)milkdropBrowser.count){state.pendingMilkdrop=chosen;state.milkdropLibrary=true;}
            else if(chosen<0 && chosen!=VisualizerLibrary::none){state.cancelMilkdrop=true;state.preset=-chosen-1;state.presetSelection=state.preset;state.milkdropLibrary=false;recordVisualizer(state);presetCycle.reset();state.visualizerMenuOpen=false;state.hud.reveal(now);}
        }
        if(pressed&ORBIS_PAD_BUTTON_CIRCLE){if(visualizerLibrary.searching)visualizerLibrary.endSearch();else{state.visualizerMenuOpen=false;state.hud.reveal(now);}}
        return;
    }
    if (state.menuOpen)
    {
        if(pressed&ORBIS_PAD_BUTTON_SQUARE)
        {
            state.microphone=!state.microphone;state.paused=false;
            pauseUsbAudio(state.microphone);pauseMicrophone(!state.microphone);
            if(state.microphone)startMicrophone();
            scrub.reset();return;
        }
        if(state.microphone)
        {
            if(pressed&(ORBIS_PAD_BUTTON_CIRCLE|ORBIS_PAD_BUTTON_CROSS))state.menuOpen=false;
            scrub.reset();return;
        }
        musicNavigation.refresh(musicBrowser);
        if (nav & ORBIS_PAD_BUTTON_UP) musicBrowser.move(-1);
        if (nav & ORBIS_PAD_BUTTON_DOWN) musicBrowser.move(1);
        if (nav & ORBIS_PAD_BUTTON_LEFT) musicNavigation.changeSource(-1,musicBrowser);
        if (nav & ORBIS_PAD_BUTTON_RIGHT) musicNavigation.changeSource(1,musicBrowser);
        if (pressed & ORBIS_PAD_BUTTON_CROSS){const int track=musicNavigation.apply(musicBrowser);if(track>=0){
            state.paused=false;pauseUsbAudio(false);playUsbTrack(track);state.menuOpen=false;
        }}
        if (nav & ORBIS_PAD_BUTTON_L1) musicBrowser.move(-6);
        if (nav & ORBIS_PAD_BUTTON_R1) musicBrowser.move(6);
        if (state.menuOpen)
        {
            const int milliseconds = scrub.updateMs(data.analogButtons.l2, data.analogButtons.r2, delta);
            if (milliseconds) seekUsbAudioMs(milliseconds);
        }
        if (pressed & ORBIS_PAD_BUTTON_CIRCLE){if(!musicNavigation.back(musicBrowser))state.menuOpen=false;scrub.reset();}
        return; // Menu buttons cannot also trigger visualization controls.
    }
    if(!state.microphone){const int ms=scrub.updateMs(data.analogButtons.l2,data.analogButtons.r2,delta);if(ms)seekUsbAudioMs(ms);}else scrub.reset();
    if(pressed&(ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_R1)){
        cycleVisualizer(state,pressed&ORBIS_PAD_BUTTON_L1?-1:1);return;
    }
    // Settings must remain reachable while paused or while the HUD owns arrows.
    if((pressed&ORBIS_PAD_BUTTON_SQUARE)&&(state.preset==5||state.preset==6||state.preset==9)){state.builtinSettingsOpen=true;state.builtinSettingsPreset=state.preset;state.builtinSettingsRow=0;state.builtinSettingsReturn=false;return;}
    if(!state.microphone && state.hud.visible(now,state.paused)){
        if(wasHud&&(nav&ORBIS_PAD_BUTTON_LEFT))state.hudSelection=(state.hudSelection+6)%7;
        if(wasHud&&(nav&ORBIS_PAD_BUTTON_RIGHT))state.hudSelection=(state.hudSelection+1)%7;
        if(pressed&ORBIS_PAD_BUTTON_CROSS){switch(state.hudSelection){
          case 0:toggleUsbShuffle();break;
          case 1:cycleUsbRepeat();break;
          case 2:stepUsbTrack(-1);break;
          case 3:if(usbAudioFinished() && usbActiveTrack()>=0){playUsbTrack(usbActiveTrack());state.paused=false;}else state.paused=!state.paused;state.hud.reveal(now);break;
          case 4:stepUsbTrack(1);break;
          case 5:presetCycle.begin();break;
          case 6:state.volumeOpen=true;break;
        }}if(state.paused || (pressed&ORBIS_PAD_BUTTON_CROSS) || (wasHud&&(pressed&(ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT))))return;
    }
    if(state.diagnostics && state.preset==11 && (pressed&ORBIS_PAD_BUTTON_R3)){milkdropBridgeTiming.manual(now);printf("HYPNOTIC MILKDROP BRIDGE MANUAL MODE %u\n",milkdropBridgeTiming.mode);return;}
    if(state.preset==6)
    {
        if(pressed&(ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT))state.scopeMidSide=!state.scopeMidSide;
        if(pressed&(ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT|ORBIS_PAD_BUTTON_TOUCH_PAD))
            state.scopeHintUntil=sceKernelGetProcessTime()+4000000;
    }
    if(state.preset==5)
    {
        if(nav&ORBIS_PAD_BUTTON_LEFT)state.sandVariation=(state.sandVariation+3)%4;
        if(nav&ORBIS_PAD_BUTTON_RIGHT)state.sandVariation=(state.sandVariation+1)%4;
        if(pressed&(ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT))state.sandHintUntil=sceKernelGetProcessTime()+2500000;
    }
    if (pressed & ORBIS_PAD_BUTTON_CROSS)
        {state.paused=!state.paused;if(state.paused)state.hudSelection=3;}
    if (pressed & ORBIS_PAD_BUTTON_SQUARE)
    {
        if(state.preset==4) { state.matrixSettingsOpen=true; state.matrixSettingsReturn=false; return; }
        else if(state.preset==8)
        { state.stormRain=!state.stormRain;state.stormHintUntil=sceKernelGetProcessTime()+2500000; }
        else state.palette = (state.palette + 1) % 4;
    }
    if ((nav & ORBIS_PAD_BUTTON_UP) && state.sensitivity < 6)
        ++state.sensitivity;
    if ((nav & ORBIS_PAD_BUTTON_DOWN) && state.sensitivity > 0)
        --state.sensitivity;
}

// Render a snapshot of the PCM samples actually submitted for playback.
void drawVisualizer(GpuScene *scene, const VisualizerState &state)
{
    if(state.preset==11){if(milkdropTexture){const GpuScene::Vertex quad[]={{0,0,0,0},{1920,0,1,0},{0,1080,0,1},{1920,0,1,0},{1920,1080,1,1},{0,1080,0,1}};scene->drawMesh(milkdropTexture,quad,6);}return;}
    static AudioWaveform target = {}, displayed = {};
    static AudioWaveform micWave;
    static AudioSpectrum micSpectrum;
    static AudioStereoScope micScope;
    if(state.microphone)copyMicrophone(micWave,micSpectrum,micScope);
    static RadialPreset radial;
    static uint64_t lastTime=0;
    const uint64_t now=sceKernelGetProcessTime();
    const float dt=lastTime ? (now-lastTime)/1000000.0f : 0.0f;
    lastTime=now;
    if (!state.paused)
    {
        if(state.microphone)target=micWave;else copyAudioWaveform(target);
        for (unsigned i = 0; i < AUDIO_WAVE_POINTS; ++i)
            displayed.points[i] += 0.69f * (target.points[i] - displayed.points[i]);
    }
    static const Color palettes[4][4] = {
        {{150,255,255}, {0,170,210}, {0,65,100}, {0,20,40}},
        {{255,150,255}, {210,0,170}, {100,0,65}, {40,0,20}},
        {{180,255,150}, {65,210,0}, {25,100,0}, {8,40,0}},
        {{255,225,150}, {210,120,0}, {100,45,0}, {40,15,0}}
    };
    const Color *palette = palettes[state.palette];
    const float gain = 0.5f + state.sensitivity * 0.25f;
    if(state.preset==9){static AudioSpectrum spectrum;if(state.microphone)spectrum=micSpectrum;else copyAudioSpectrum(spectrum);gpuCubeOrbit.draw(scene,spectrum,cubeOrbitSettings,gain,state.palette,state.paused,dt);return;}
    if(state.preset==8)
    {
        static AudioSpectrum spectrum;
        if(state.microphone)spectrum=micSpectrum;else copyAudioSpectrum(spectrum);
        if(gpuStormReady)gpuStorm.draw(scene,spectrum,gain,state.paused,dt,state.stormRain);
        else drawStatusText(scene,"STORM SKY INIT FAILED - \x84 OPENS VISUALIZERS",500);
        return;
    }
    if(state.preset==7)
    {
        static AudioSpectrum spectrum;
        if(state.microphone)spectrum=micSpectrum;else copyAudioSpectrum(spectrum);
        if(gpuLiquidReady)gpuLiquidAura.draw(scene,spectrum,gain,state.palette,state.paused,dt);
        else drawStatusText(scene,"LIQUID AURA INIT FAILED - \x84 OPENS VISUALIZERS",500);
        return;
    }
    if(state.preset==6)
    {
        static AudioStereoScope stereo;
        if(state.microphone)stereo=micScope;else copyAudioStereoScope(stereo);
        const float trails[]={.2f,.5f,.8f,1.2f};
        gpuXyScope.draw(scene,stereo,gain,state.palette,state.paused,dt,state.scopeMidSide,trails[state.scopeTrail]);
        return;
    }
    if(state.preset==5)
    {
        static AudioSpectrum spectrum;
        if(state.microphone)spectrum=micSpectrum;else copyAudioSpectrum(spectrum);
        if(gpuCymaticsReady)gpuCymatics.draw(scene,spectrum,gain,state.palette,state.paused,dt,state.sandVariation);
        else drawStatusText(scene,"CYMATICS INIT FAILED - \x84 OPENS VISUALIZERS",500);
        return;
    }
    if(state.preset==4)
    {
        static AudioSpectrum spectrum;
        if(state.microphone)spectrum=micSpectrum;else copyAudioSpectrum(spectrum);
        if(gpuMatrixReady) gpuMatrixEq.draw(scene,spectrum,matrixSettings,dt,state.paused,gain);
        else drawStatusText(scene,"MATRIX EQ INIT FAILED - \x84 OPENS VISUALIZERS",500);
        return;
    }
    if(state.preset==3)
    {
        if(gpuKaleidoscopeReady) gpuKaleidoscope.draw(scene,displayed,gain,state.palette,state.paused,dt);
        else drawStatusText(scene,"KALEIDOSCOPE INIT FAILED - \x84 OPENS VISUALIZERS",500);
        return;
    }
    if (state.preset == 2)
    {
        if (gpuWormholeReady) gpuWormhole.draw(scene, displayed, gain, state.palette, state.paused, dt);
        else drawStatusText(scene,"TUNNEL INIT FAILED - \x84 OPENS VISUALIZERS",500);
        return;
    }
    const Color black = { 0, 0, 0 };
    scene->FrameBufferFill(black);
    if (state.preset == 1)
    {
        radial.draw(scene, displayed, gain, state.palette, state.paused, dt);
        return;
    }
    int previousY = FRAME_HEIGHT / 2;
    static int tops[FRAME_WIDTH],bottoms[FRAME_WIDTH];
    for (int x = 0; x < FRAME_WIDTH; ++x)
    {
        const float position = (float)x * (AUDIO_WAVE_POINTS - 1) / (FRAME_WIDTH - 1);
        const unsigned index = (unsigned)position;
        const unsigned next = index + 1 < AUDIO_WAVE_POINTS ? index + 1 : index;
        const float fraction = position - index;
        const float sample = displayed.points[index]
                           + fraction * (displayed.points[next] - displayed.points[index]);
        float displacement = sample * gain * 360.0f;
        if (displacement > 360.0f) displacement = 360.0f;
        if (displacement < -360.0f) displacement = -360.0f;
        const int y = FRAME_HEIGHT / 2 - (int)displacement;
        // Join adjacent samples so steep sections remain continuous.
        const int top = (x > 0 && previousY < y) ? previousY : y;
        const int bottom = (x > 0 && previousY > y) ? previousY : y;
        tops[x]=top; bottoms[x]=bottom;
        previousY = y;
    }
    // Paint whole glow layers together so the atlas batches four colors,
    // rather than changing color several times at every screen column.
    const int widths[4]={1,3,5,8};
    for(int layer=3;layer>=0;--layer)
        for(int x=0;x<FRAME_WIDTH;++x)
            scene->DrawRectangle(x,tops[x]-widths[layer],1,
                bottoms[x]-tops[x]+1+widths[layer]*2,palette[layer]);


}

void drawMusicMenu(UiScene *scene, const VisualizerState &state)
{
    if(musicSearch.open)scene->DrawRectangle(48,164,1824,636,Color{4,10,16});
    else if(state.microphone)scene->DrawRectangle(48,272,1824,574,Color{4,10,16});
    if(musicSearch.open){
        drawStatusText(scene,presetSearchTarget?"SEARCH VISUALIZERS":"SEARCH USB MUSIC",194);char query[96];snprintf(query,sizeof(query),"> %s_",musicSearch.text);drawStatusText(scene,query,254);
        drawStatusText(scene,presetSearchTarget?"BUILT-IN / MILKDROP / IMPORTED":"FIND FILE NAMES ACROSS ALL USB FOLDERS",310);
        for(int i=0;i<40;++i){int x=64+i%10*178,y=382+i/10*60;if(i==musicSearch.selected)scene->DrawRectangle(x,y-5,164,42,Color{0,55,72});char key[8]={MusicSearchInput::keys()[i],0};if(i==36)snprintf(key,sizeof(key),"SPACE");if(i==38)snprintf(key,sizeof(key),"UNDER");if(i==39)snprintf(key,sizeof(key),"DOT");drawStatusText(scene,key,y,x+12,7);}
        for(int i=0;i<2;++i){int x=64+i*890;if(musicSearch.selected==40+i)scene->DrawRectangle(x,648,868,44,Color{0,55,72});drawStatusText(scene,i?"CANCEL":"SEARCH",654,x+12,12);}
        drawStatusText(scene,"X TYPE / APPLY - \x85 DELETE - \x84 CLEAR - \x86 CANCEL - X SEARCH",768);return;
    }
    if(state.microphone)
    {
        drawStatusText(scene,"MIC MODE - HEADSET MICROPHONE",292);
        drawStatusText(scene,microphoneStatus(),358);
        drawStatusText(scene,"MICROPHONE INPUT LEVEL",430);
        scene->DrawRectangle(64,490,1776,24,Color{12,25,30});
        const int fill=microphonePeak()*1776/32768;
        if(fill)scene->DrawRectangle(64,490,fill,24,Color{30,210,100});
        if(state.diagnostics)drawStatusText(scene,microphoneDetails(),550);
        else drawStatusText(scene,"16000 HZ MONO - LIVE AUDIO INPUT",550);
        drawStatusText(scene,"VISUALIZERS REACT TO YOUR MICROPHONE",626);
        drawStatusText(scene,"\x85 USB MUSIC - X VISUALIZE - \x86 CLOSE",774);
        return;
    }
    drawMusicLibrary(scene,musicNavigation,musicBrowser,state.paused);
}

void drawVisualizerMenu(UiScene *scene,const VisualizerState &state)
{
    if(musicSearch.open&&presetSearchTarget){drawMusicMenu(scene,state);return;}
    const int active=state.preset==11?visualizerLibrary.resolve(milkdropBrowser.activePath):-state.preset-1;
    drawVisualizerLibrary(scene,visualizerLibrary,active,milkdropBrowser.error.c_str());
    if(state.presetPreparing){scene->DrawRectangle(64,690,1792,42,Color{4,10,16});drawStatusText(scene,"PREPARING PRESET - CURRENT VISUALIZER CONTINUES",700,80,96);}
}

void drawMatrixSettings(UiScene *scene,const VisualizerState &state)
{
    scene->DrawRectangle(48,272,1824,574,Color{4,10,16});
    drawStatusText(scene,"MATRIX EQ SETTINGS",292);
    for(int row=0;row<9;++row)
    {
        const int y=346+row*48;
        if(row==state.matrixSettingSelection)scene->DrawRectangle(56,y-5,1800,34,Color{0,55,72});
        char text[96];
        switch(row)
        {
            case 0:snprintf(text,sizeof(text),"SPECTRUM LAYOUT         %s",matrixSettings.layoutName());break;
            case 1:snprintf(text,sizeof(text),"VERTICAL FLIP           %s",matrixSettings.flipped?"ON":"OFF");break;
            case 2:snprintf(text,sizeof(text),"EQ HEIGHT               %d",matrixSettings.height);break;
            case 3:snprintf(text,sizeof(text),"CHARACTER SIZE          %d",matrixSettings.size);break;
            case 4:snprintf(text,sizeof(text),"RAIN DENSITY            %d PERCENT",matrixSettings.density);break;
            case 5:snprintf(text,sizeof(text),"FALLING SPEED           %d PX/S",matrixSettings.speed);break;
            case 6:snprintf(text,sizeof(text),"FADE DURATION           %.1f S",matrixSettings.fadeHalfSeconds*0.5f);break;
            case 7:snprintf(text,sizeof(text),"GLOW INTENSITY          %d PERCENT",matrixSettings.glow);break;
            default:snprintf(text,sizeof(text),"DEMO WITHOUT AUDIO      %s",matrixSettings.demo?"ON":"OFF");break;
        }
        drawStatusText(scene,text,y);
    }
    drawStatusText(scene,"\x80\x81 SELECT - \x82\x83 ADJUST - \x86 BACK",774);
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    reportStartup("PLAYER 1 - MAIN REACHED");
    VisualizerState state;activeUiSettings=&visualizerLibrary.appearance;
    uint32_t previousButtons = 0;

    // No buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    
    // Create a 2D scene
    DEBUGLOG << "Creating a scene";
    
    auto scene = new GpuScene;
    if (!scene->Init(false,reportStartup))
    {
        // Do not introduce a second display owner while diagnosing EGL failure.
        holdStartupFailure(scene->error());
    }

    UiScene uiScene(scene);
    scene->Begin();
    drawStatusText(&uiScene,"HYPNOTIC",280,80,90);
    drawStatusText(&uiScene,"MUSIC / VISUALS",328,80,90);
    drawStatusText(&uiScene,"BUILT WITH OPENORBIS AND THE PS4 HOMEBREW COMMUNITY",430,80,96);
    drawStatusText(&uiScene,"MILKDROP: RYAN GEISS / PROJECTM CONTRIBUTORS",478,80,96);
    drawStatusText(&uiScene,"PRESETS: CREAM OF THE CROP / ORIGINAL COMMUNITY ARTISTS",526,80,96);
    drawStatusText(&uiScene,"SHADERS: OPEN GNM / MESA / GLSLANG / HLSLPARSER",574,80,96);
    drawStatusText(&uiScene,"AUDIO: DR_LIBS / DAVID REID - STB / SEAN BARRETT",622,80,96);
    drawStatusText(&uiScene,"USB ACCESS: LIBJBC - THANK YOU TO ALL CONTRIBUTORS",670,80,96);
    drawStatusText(&uiScene,"LOADING YOUR MUSIC...",780,80,96);
    if(!scene->Present()) holdStartupFailure(scene->error());
    loadPlayerModules();
    if(!milkdropPlayerPreloadDriver())printf("HYPNOTIC MILKDROP PRELOAD DEFERRED %s\n",milkdropPlayerError());
    const int pad=openController();
    reportStartup("PLAYER 9 - STARTING USB AUDIO THREAD");
    startUsbAudio();
    reportStartup("PLAYER 10 - INITIALIZING PERFORMANCE STRIP");
    PerformanceOverlay performance;
    performance.Init();
    reportStartup("PLAYER 11 - ENTERING PLAYER LOOP");
    state.hud.reveal(sceKernelGetProcessTime());
    bool preferencesLoaded=false;
    std::string pendingPresetPath;
    bool pendingPresetManual=false;
    DEBUGLOG << "Entering draw loop...";

    // Draw loop
    for (;;)
    {
        uint64_t milkdropStage[6]={};bool milkdropMeasured=false;
        // Draw the animated neon waveform
        const uint64_t uiNow=sceKernelGetProcessTime();
        if(!preferencesLoaded&&usbLibraryReady()){
            preferencesLoaded=true;if(!milkdropBrowser.scanned)milkdropBrowser.rescan();
            if(!visualizerLibrary.dirty)loadUiPreferences(visualizerLibrary);visualizerLibrary.rebuild();state.hud.delay=(uint64_t)visualizerLibrary.appearance.hudSeconds*1000000;state.sensitivity=visualizerLibrary.appearance.sensitivity;milkdropPlayerRequestHD(visualizerLibrary.renderHD);
        }
        updateControls(pad, state, previousButtons);
        setAudioOutputSettings(visualizerLibrary.appearance.audio);
        if(preferencesLoaded&&visualizerLibrary.dirty&&uiNow>=visualizerLibrary.changed&&uiNow-visualizerLibrary.changed>=500000){
            if(!saveUiPreferences(visualizerLibrary))printf("HYPNOTIC UI PREFERENCES SAVE FAILED - SESSION STATE RETAINED\n");visualizerLibrary.dirty=false;
        }
        if(state.cancelMilkdrop){pendingPresetPath.clear();pendingPresetManual=false;state.cancelMilkdrop=false;}
        if(state.pendingMilkdrop>=0){pendingPresetPath=milkdropBrowser.entries[state.pendingMilkdrop].path;pendingPresetManual=true;state.pendingMilkdrop=-1;}
        // Disabling Auto cancels a pending automatic switch, never a manual selection.
        if(!presetCycle.enabled&&!pendingPresetManual)pendingPresetPath.clear();
        warmNextBuiltin(scene,state);
        const bool cycleAllowed=!state.volumeOpen&&pendingPresetPath.empty()&&!state.paused&&!state.microphone&&!state.menuOpen&&!state.visualizerMenuOpen&&!state.matrixSettingsOpen&&!state.builtinSettingsOpen&&!state.appSettingsOpen&&(state.preset!=11||!state.milkdropFault);
        int next=state.preset==11?milkdropBrowser.peekNext():-1;
        const std::string automaticPath=presetCycle.enabled&&next>=0?milkdropBrowser.entries[next].path:std::string();
        if(presetCycle.tick(sceKernelGetProcessTime(),cycleAllowed)){if(state.preset==11){pendingPresetPath=automaticPath;pendingPresetManual=false;}else cycleVisualizer(state,1);}
        const std::string wanted=!pendingPresetPath.empty()?pendingPresetPath:automaticPath;
        pauseUsbAudio(state.paused||state.microphone);pauseMicrophone(state.paused||!state.microphone);
        if(!wanted.empty()){
            int prep=milkdropPlayerPrepareState(wanted.c_str());
            if(prep==0&&!milkdropPlayerPrepareBusy()){
                if(!scene->SuspendForNativeGpu())return 1;
                const bool begun=milkdropPlayerBeginPrepare(wanted.c_str());
                if(!scene->ResumeAfterNativeGpu())return 1;
                if(!begun)printf("HYPNOTIC PREFETCH DEFERRED %s\n",wanted.c_str());
                prep=milkdropPlayerPrepareState(wanted.c_str());
            }
            if(prep==2){if(!scene->SuspendForNativeGpu())return 1;milkdropPlayerFinishPrepare(wanted.c_str());if(!scene->ResumeAfterNativeGpu())return 1;prep=milkdropPlayerPrepareState(wanted.c_str());}
            if(!pendingPresetPath.empty()&&(prep==3||prep<0)){
                bool applied=false;if(prep==3){if(!scene->SuspendForNativeGpu())return 1;applied=milkdropPlayerApply(wanted.c_str());if(!scene->ResumeAfterNativeGpu())return 1;}
                else milkdropPlayerFinishPrepare(wanted.c_str());
                if(applied){milkdropBrowser.activePath=wanted;milkdropBrowser.error.clear();state.milkdropFault=false;presetCycle.reset();milkdropBridgeTiming.reset(sceKernelGetProcessTime());state.preset=11;recordVisualizer(state);performance.ResetAveraging();state.visualizerMenuOpen=false;if(pendingPresetManual)state.hud.reveal(sceKernelGetProcessTime());}
                else{milkdropBrowser.error=milkdropPlayerError();state.visualizerMenuOpen=true;state.milkdropLibrary=true;}
                pendingPresetPath.clear();pendingPresetManual=false;
            }
        }
        state.presetPreparing=!pendingPresetPath.empty();
        milkdropBridgeTiming.tick(sceKernelGetProcessTime());
        static unsigned measuredMode=1;if(measuredMode!=milkdropBridgeTiming.mode){performance.ResetAveraging();measuredMode=milkdropBridgeTiming.mode;}
        milkdropPlayerSetBoosted(milkdropBridgeTiming.mode!=0);
        if(state.preset==11 && !state.milkdropFault){
            static AudioStereoScope scope;static AudioWaveform micWave;static AudioSpectrum micSpectrum;static uint64_t lastRender=0;const uint64_t now=sceKernelGetProcessTime();float dt=lastRender?(now-lastRender)/1000000.f:1.f/60;lastRender=now;
            if(state.microphone)copyMicrophone(micWave,micSpectrum,scope);else copyAudioStereoScope(scope);
            MilkdropPixels image={};milkdropStage[0]=sceKernelGetProcessTime();
            if(!scene->SuspendForNativeGpu(milkdropBridgeTiming.releaseContext()))return 1;
            milkdropStage[1]=sceKernelGetProcessTime();
            milkdropPlayerSetReadback(milkdropBridgeTiming.mode!=0);
            bool rendered=milkdropPlayerRender(scope.left,scope.right,scope.count,scope.sequence,scope.epoch,scope.available,state.paused,dt,image);
            milkdropStage[2]=sceKernelGetProcessTime();
            if(!scene->ResumeAfterNativeGpu())return 1;
            milkdropStage[3]=sceKernelGetProcessTime();
            bool uploaded=false;
            if(rendered && copyMilkdropPixels(image,milkdropUpload,sizeof(milkdropUpload))){
                milkdropStage[4]=sceKernelGetProcessTime();
                if(!milkdropTexture){milkdropTexture=scene->createTexture(image.width,image.height,milkdropUpload,false);uploaded=milkdropTexture!=0;}
                else uploaded=scene->updateTextureRegion(milkdropTexture,0,0,image.width,image.height,milkdropUpload);
            }
            milkdropStage[5]=sceKernelGetProcessTime();milkdropMeasured=uploaded;
            if(!uploaded){state.milkdropFault=true;milkdropBrowser.error=rendered?"MilkDrop image transfer failed":milkdropPlayerError();state.visualizerMenuOpen=true;state.milkdropLibrary=true;printf("HYPNOTIC MILKDROP FRAME ERROR %s\n",milkdropBrowser.error.c_str());}
        }
        if(state.preset==2 && !gpuWormholeAttempted)
        {
            gpuWormholeAttempted=true;
            showPresetLoading(scene,"WORMHOLE");
            const uint64_t started=sceKernelGetProcessTime();
            gpuWormholeReady=gpuWormhole.Init(scene);
            printf("HYPNOTIC WORMHOLE LOAD %llu US - READY %d\n",
                (unsigned long long)(sceKernelGetProcessTime()-started),(int)gpuWormholeReady);
            if(!gpuWormholeReady) reportFailure("TUNNEL INIT FAILED - OTHER PRESETS AVAILABLE");
        }
        if(state.preset==3 && !gpuKaleidoscopeAttempted)
        {
            gpuKaleidoscopeAttempted=true;
            showPresetLoading(scene,"KALEIDOSCOPE");
            const uint64_t started=sceKernelGetProcessTime();
            gpuKaleidoscopeReady=gpuKaleidoscope.Init(scene);
            printf("HYPNOTIC KALEIDOSCOPE LOAD %llu US - READY %d\n",
                (unsigned long long)(sceKernelGetProcessTime()-started),(int)gpuKaleidoscopeReady);
            if(!gpuKaleidoscopeReady) reportFailure("KALEIDOSCOPE INIT FAILED - OTHER PRESETS AVAILABLE");
        }
        if(state.preset==4 && !gpuMatrixAttempted)
        {
            gpuMatrixAttempted=true;
            showPresetLoading(scene,"MATRIX EQ");
            gpuMatrixReady=gpuMatrixEq.Init(scene);
            if(!gpuMatrixReady)reportFailure("MATRIX EQ INIT FAILED - OTHER PRESETS AVAILABLE");
        }
        if(state.preset==5 && !gpuCymaticsAttempted)
        {
            gpuCymaticsAttempted=true;
            showPresetLoading(scene,"CYMATICS SAND");
            gpuCymaticsReady=gpuCymatics.Init(scene);
            if(!gpuCymaticsReady)reportFailure("CYMATICS INIT FAILED - OTHER PRESETS AVAILABLE");
        }
        if(state.preset==7 && !gpuLiquidAttempted)
        {
            gpuLiquidAttempted=true;
            showPresetLoading(scene,"LIQUID AURA");
            gpuLiquidReady=gpuLiquidAura.Init(scene);
            if(!gpuLiquidReady)reportFailure("LIQUID AURA INIT FAILED - OTHER PRESETS AVAILABLE");
        }
        if(state.preset==8 && !gpuStormAttempted)
        {
            gpuStormAttempted=true;
            showPresetLoading(scene,"STORM SKY");
            gpuStormReady=gpuStorm.Init(scene);
            if(!gpuStormReady)reportFailure("STORM SKY INIT FAILED - OTHER PRESETS AVAILABLE");
        }
        scene->Begin();
        drawVisualizer(scene, state);
        if(state.preset==8 && !state.menuOpen && !state.visualizerMenuOpen && sceKernelGetProcessTime()<state.stormHintUntil)
            drawStatusText(scene,state.stormRain?"\x85 RAIN ON - X PAUSE - \x80\x81 SENSITIVITY"
                :"\x85 RAIN OFF - X PAUSE - \x80\x81 SENSITIVITY",980);
        if(state.preset==6 && !state.menuOpen && !state.visualizerMenuOpen && sceKernelGetProcessTime()<state.scopeHintUntil)
        {
            const char *trails[]={"0.2","0.5","0.8","1.2"};
            char hint[128];snprintf(hint,sizeof(hint),"%s - TRAIL %s S - \x82\x83 MODE - \x85 SETTINGS",state.scopeMidSide?"MID/SIDE":"XY",trails[state.scopeTrail]);
            drawStatusText(scene,hint,980);
        }
        if(state.preset==5 && !state.menuOpen && !state.visualizerMenuOpen && sceKernelGetProcessTime()<state.sandHintUntil)
        {
            const char *names[]={"ORIGINAL","ORNATE","CELLS","WEAVE"};
            char hint[96];snprintf(hint,sizeof(hint),"SAND %s - \x82\x83 VARIATION",names[state.sandVariation]);
            drawStatusText(scene,hint,980);
        }
        uiScene.BeginUi();
        // Ordinary UI stays clear of developer credentials and timing details.
        if(state.appSettingsOpen&&state.appMicTestOpen)drawMicTest(&uiScene,microphoneStatus(),microphoneDetails(),microphonePeak());
        else if(state.appSettingsOpen&&state.appEqOpen)drawAudioEqualizer(&uiScene,visualizerLibrary.appearance,state.appEqRow);
        else if(state.appSettingsOpen)drawAppSettings(&uiScene,visualizerLibrary.appearance,state.appSettingsRow,visualizerLibrary.renderHD,state.appCreditsOpen,state.sensitivity,state.appSettingsTab,state.diagnostics);
        else if(state.menuOpen)drawMusicMenu(&uiScene,state);
        else if(state.visualizerMenuOpen)drawVisualizerMenu(&uiScene,state);
        else if(state.matrixSettingsOpen)drawMatrixSettings(&uiScene,state);
        else if(state.builtinSettingsOpen)drawBuiltinSettings(&uiScene,state.builtinSettingsPreset,state.builtinSettingsRow,state.sandVariation,state.sensitivity,state.palette,cubeOrbitSettings,state.scopeMidSide,state.scopeTrail);
        const bool micFullscreen=state.microphone&&!state.menuOpen&&!state.visualizerMenuOpen&&!state.matrixSettingsOpen&&!state.builtinSettingsOpen&&!state.appSettingsOpen&&!musicSearch.open&&!presetCycle.menuOpen;
        if(state.diagnostics&&!micFullscreen){
            char label[112];snprintf(label,sizeof(label),"DEBUG %s - R3 TOGGLE - SAME PRESET STATE",state.preset==11?(milkdropBridgeTiming.mode?(milkdropPlayerReadbackReady()?"BOOSTED":"BOOSTED / DMA FALLBACK"):"REFERENCE"):"BUILT-IN");drawStatusText(scene,label,32);performance.Draw(&uiScene,68);
            snprintf(label,sizeof(label),"RENDER %u X %u - NEXT LAUNCH %s - \x85 QUALITY",milkdropPlayerWidth(),milkdropPlayerHeight(),visualizerLibrary.renderHD?"1080P":"288P");drawStatusText(scene,label,112);
        }
        const bool hudVisible=!state.microphone&&!state.matrixSettingsOpen&&!state.builtinSettingsOpen&&!state.appSettingsOpen&&(state.volumeOpen||presetCycle.menuOpen||state.menuOpen||state.visualizerMenuOpen||state.hud.visible(sceKernelGetProcessTime(),state.paused));
        if(hudVisible){
            const int active=usbActiveTrack();const char *track=active>=0?usbTrackName(active):"NO TRACK SELECTED";
            const char *visualizer=state.preset==11?milkdropBrowser.activeName():presetNames[state.preset];
            const int focus=state.menuOpen||state.visualizerMenuOpen?-1:state.hudSelection;
            drawPlayerHud(&uiScene,PlayerHudData{track,visualizer,usbPlaybackPositionMs(),usbPlaybackDurationMs(),usbShuffle(),usbRepeat(),focus,presetCycle.enabled,presetCycle.seconds,state.paused||usbAudioFinished(),visualizerLibrary.appearance.audio.volume,visualizerLibrary.appearance.audio.muted});
            if(state.volumeOpen)drawVolumePopover(&uiScene,visualizerLibrary.appearance.audio);
            if(presetCycle.menuOpen)drawPresetCycleMenu(&uiScene,presetCycle.draftEnabled,presetCycle.draftSeconds,presetCycle.row);
        }
        if(visualizerLibrary.appearance.stats&&!state.diagnostics&&!micFullscreen)performance.Draw(&uiScene,32);
        if(!scene->Present()) return 1;
        performance.Presented();
        if(milkdropMeasured){const uint64_t done=sceKernelGetProcessTime();uint64_t times[]={milkdropStage[1]-milkdropStage[0],milkdropStage[2]-milkdropStage[1],milkdropStage[3]-milkdropStage[2],milkdropStage[4]-milkdropStage[3],milkdropStage[5]-milkdropStage[4],done-milkdropStage[5],done-milkdropStage[0]};milkdropBridgeTiming.add(times);if(milkdropBridgeTiming.due(done))milkdropBridgeTiming.report(done);}
    }

    return 0;
}

#endif
