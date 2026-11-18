#include <cstdint>
#include <cstdio>
#include <cassert>
#include "music_search_input.h"
#include "touchpad_routing.h"
static TouchpadRouting touchpadRouting;
#include "cube_orbit.h"
static CubeOrbitSettings cubeOrbitSettings;
#include "music_navigation.h"
#include "visualizer_library.h"
#include "menu_repeat.h"
#include "trigger_scrub.h"
#include "preset_cycle.h"
enum {ORBIS_PAD_BUTTON_UP=1,ORBIS_PAD_BUTTON_DOWN=2,ORBIS_PAD_BUTTON_LEFT=4,ORBIS_PAD_BUTTON_RIGHT=8,ORBIS_PAD_BUTTON_CROSS=16,ORBIS_PAD_BUTTON_CIRCLE=32,ORBIS_PAD_BUTTON_SQUARE=64,ORBIS_PAD_BUTTON_TRIANGLE=128,ORBIS_PAD_BUTTON_OPTIONS=256,ORBIS_PAD_BUTTON_TOUCH_PAD=512,ORBIS_PAD_BUTTON_L1=1024,ORBIS_PAD_BUTTON_R1=2048,ORBIS_PAD_BUTTON_L3=4096,ORBIS_PAD_BUTTON_R3=8192};
struct OrbisPadData{bool connected=true;uint32_t buttons=0;struct{unsigned char l2=0,r2=0;}analogButtons;struct{unsigned char fingers=0;struct{unsigned short x=0,y=0;}touch[2];}touch;};
static OrbisPadData fake;static uint64_t clockUs=10000000;static int steps=0,plays=0;static bool finished=false;
uint64_t sceKernelGetProcessTime(){return clockUs;}
int scePadReadState(int,OrbisPadData *out){*out=fake;return 0;}
static MusicSearchInput musicSearch;static MusicFolderBrowser musicBrowser;static MusicNavigation musicNavigation;static bool presetSearchTarget=false;static PresetCycleTimer presetCycle;
static MilkdropBrowser milkdropBrowser;
struct {void adjust(int,int){}}matrixSettings;
#include "milkdrop_bridge_timing.h"
static MilkdropBridgeTiming milkdropBridgeTiming;
bool milkdropPlayerRequestHD(bool){return true;}
static bool micPausedTest=true;static int micStartsTest=0;void pauseUsbAudio(bool){}void pauseMicrophone(bool p){micPausedTest=p;}void startMicrophone(){++micStartsTest;}void seekUsbAudio(int){}void seekUsbAudioMs(int64_t){}
void stepUsbTrack(int d){steps+=d;}void toggleUsbShuffle(){}void cycleUsbRepeat(){}void playUsbTrack(int){++plays;finished=false;}bool usbAudioFinished(){return finished;}
int usbActiveTrack(){return 0;}bool usbLibraryReady(){return true;}int usbFolderCount(){return 0;}int usbTrackCount(){return 0;}int usbFolderParent(int){return -1;}int usbTrackFolder(int){return -1;}const char *usbFolderPath(int){return "";}const char *usbTrackName(int){return "track";}
struct VisualizerState
{
    bool paused = false;
    int hudSelection=3;
    HudVisibility hud;
    bool diagnostics=false;
    bool microphone=false;
    bool menuOpen = true;
    bool visualizerMenuOpen = false;
    bool appSettingsOpen=false,appCreditsOpen=false,appMicTestOpen=false,settingsReturnMusic=false,settingsReturnVisualizers=false;int appSettingsRow=0;
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
    int context=musicSearch.open?1:presetCycle.menuOpen?2:(state.matrixSettingsOpen||state.builtinSettingsOpen||state.appSettingsOpen)?3:state.visualizerMenuOpen?4:state.menuOpen?5:6;
    const uint32_t nav=pressed|repeat.update(context<6?data.buttons:0,directions,ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT,now,context);
    const bool noMenu=!state.menuOpen&&!state.visualizerMenuOpen&&!state.matrixSettingsOpen&&!state.builtinSettingsOpen&&!state.appSettingsOpen&&!musicSearch.open&&!presetCycle.menuOpen;
    const bool wasHud=state.hud.visible(now,state.paused);
    if(noMenu && (pressed||data.analogButtons.l2>24||data.analogButtons.r2>24)){
        if(!wasHud)state.hudSelection=3;state.hud.reveal(now);
    }
    if((data.buttons&(ORBIS_PAD_BUTTON_L3|ORBIS_PAD_BUTTON_R3))==(ORBIS_PAD_BUTTON_L3|ORBIS_PAD_BUTTON_R3) && (pressed&(ORBIS_PAD_BUTTON_L3|ORBIS_PAD_BUTTON_R3))){state.diagnostics=!state.diagnostics;milkdropBridgeTiming.debug(state.diagnostics,now);scrub.reset();return;}
    if(noMenu && (pressed&ORBIS_PAD_BUTTON_L3)){state.hud.toggle(now);scrub.reset();return;}
    if(noMenu&&state.diagnostics&&(pressed&ORBIS_PAD_BUTTON_SQUARE)){visualizerLibrary.renderHD=!visualizerLibrary.renderHD;visualizerLibrary.touch(now);milkdropPlayerRequestHD(visualizerLibrary.renderHD);return;}
    if(pressed&ORBIS_PAD_BUTTON_OPTIONS){
        if(state.appMicTestOpen){pauseMicrophone(!state.microphone);state.appMicTestOpen=false;}
        if(state.appSettingsOpen){state.appSettingsOpen=false;state.appCreditsOpen=false;state.menuOpen=state.settingsReturnMusic;state.visualizerMenuOpen=state.settingsReturnVisualizers;}
        else{state.settingsReturnMusic=state.menuOpen;state.settingsReturnVisualizers=state.visualizerMenuOpen;state.appSettingsOpen=true;state.appCreditsOpen=false;state.menuOpen=state.visualizerMenuOpen=false;state.matrixSettingsOpen=state.builtinSettingsOpen=false;musicSearch.open=false;presetCycle.menuOpen=false;}
        scrub.reset();return;
    }
    if((pressed&ORBIS_PAD_BUTTON_TOUCH_PAD)||((pressed&ORBIS_PAD_BUTTON_L3)&&(state.menuOpen||state.visualizerMenuOpen))){
        const bool search=(pressed&ORBIS_PAD_BUTTON_L3)||touchpadRouting.searchSide(now);
        const bool visualSearch=state.visualizerMenuOpen;
        if(state.appMicTestOpen){pauseMicrophone(!state.microphone);state.appMicTestOpen=false;}
        state.appSettingsOpen=state.appCreditsOpen=false;state.matrixSettingsOpen=state.builtinSettingsOpen=false;presetCycle.menuOpen=false;
        if(search){presetSearchTarget=visualSearch;if(!visualSearch){state.menuOpen=true;state.visualizerMenuOpen=false;}musicSearch.start(presetSearchTarget?visualizerLibrary.query.c_str():musicNavigation.searchResults?musicBrowser.query:"");}
        else{state.menuOpen=true;state.visualizerMenuOpen=false;musicSearch.open=false;}
        scrub.reset();return;
    }
    if(state.appSettingsOpen){
        scrub.reset();
        if(state.appMicTestOpen){if(pressed&(ORBIS_PAD_BUTTON_CIRCLE|ORBIS_PAD_BUTTON_CROSS)){pauseMicrophone(!state.microphone);state.appMicTestOpen=false;}return;}
        if(state.appCreditsOpen){if(pressed&(ORBIS_PAD_BUTTON_CIRCLE|ORBIS_PAD_BUTTON_CROSS))state.appCreditsOpen=false;return;}
        if((nav&ORBIS_PAD_BUTTON_UP)&&state.appSettingsRow>0)--state.appSettingsRow;
        if((nav&ORBIS_PAD_BUTTON_DOWN)&&state.appSettingsRow<7)++state.appSettingsRow;
        const int d=(nav&ORBIS_PAD_BUTTON_RIGHT)?1:(nav&ORBIS_PAD_BUTTON_LEFT)?-1:0;
        if(d){
            const int dir=d?d:1,row=state.appSettingsRow;auto &ui=visualizerLibrary.appearance;
            if(row==0)ui.theme=(ui.theme+UiSettings::themeCount+dir)%UiSettings::themeCount;
            else if(row==1)ui.font=(ui.font+UiSettings::fontCount+dir)%UiSettings::fontCount;
            else if(row==2){ui.hudSeconds=UiSettings::bound(ui.hudSeconds+dir,3,15);state.hud.delay=(uint64_t)ui.hudSeconds*1000000;}
            else if(row==3){state.sensitivity=UiSettings::bound(state.sensitivity+dir,0,6);ui.sensitivity=state.sensitivity;}
            else if(row==4){visualizerLibrary.renderHD=!visualizerLibrary.renderHD;milkdropPlayerRequestHD(visualizerLibrary.renderHD);}
            else if(row==5)ui.stats=!ui.stats;

            if(row<6)visualizerLibrary.touch(now);
        }
        if(pressed&ORBIS_PAD_BUTTON_CROSS){if(state.appSettingsRow==7)state.appCreditsOpen=true;else if(state.appSettingsRow==6){state.appMicTestOpen=true;pauseMicrophone(false);startMicrophone();}else visualizerLibrary.touch(now);}
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
        if(wasHud&&(nav&ORBIS_PAD_BUTTON_LEFT))state.hudSelection=(state.hudSelection+5)%6;
        if(wasHud&&(nav&ORBIS_PAD_BUTTON_RIGHT))state.hudSelection=(state.hudSelection+1)%6;
        if(pressed&ORBIS_PAD_BUTTON_CROSS){switch(state.hudSelection){
          case 0:toggleUsbShuffle();break;
          case 1:cycleUsbRepeat();break;
          case 2:stepUsbTrack(-1);break;
          case 3:if(usbAudioFinished() && usbActiveTrack()>=0){playUsbTrack(usbActiveTrack());state.paused=false;}else state.paused=!state.paused;state.hud.reveal(now);break;
          case 4:stepUsbTrack(1);break;
          case 5:presetCycle.begin();break;
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

int main(){VisualizerState state;state.menuOpen=false;milkdropBrowser.scanned=true;uint32_t previous=0;
 auto frame=[&](uint32_t buttons,uint64_t dt=16667){clockUs+=dt;fake.buttons=buttons;updateControls(0,state,previous);};
 auto press=[&](uint32_t buttons){frame(0);frame(buttons);};
 // A right physical click opens Music; contact alone does nothing.
 fake.touch.fingers=1;fake.touch.touch[0].x=1500;frame(0);assert(!state.menuOpen);press(ORBIS_PAD_BUTTON_TOUCH_PAD);assert(state.menuOpen&&!musicSearch.open);press(ORBIS_PAD_BUTTON_CIRCLE);assert(!state.menuOpen);
 fake.touch.touch[0].x=100;press(ORBIS_PAD_BUTTON_TOUCH_PAD);assert(state.menuOpen&&musicSearch.open&&!presetSearchTarget);musicSearch.start("test");musicSearch.selected=40;press(ORBIS_PAD_BUTTON_CROSS);assert(!musicSearch.open&&musicNavigation.searchResults);press(ORBIS_PAD_BUTTON_CIRCLE);assert(!musicNavigation.searchResults);
 press(ORBIS_PAD_BUTTON_L3);assert(musicSearch.open);press(ORBIS_PAD_BUTTON_CIRCLE);assert(!musicSearch.open);press(ORBIS_PAD_BUTTON_CIRCLE);assert(!state.menuOpen);
 // A remote click with no current/recent position falls back to Music.
 fake.touch.fingers=0;frame(0,300000);press(ORBIS_PAD_BUTTON_TOUCH_PAD);assert(state.menuOpen&&!musicSearch.open);
 press(ORBIS_PAD_BUTTON_OPTIONS);assert(state.appSettingsOpen&&!state.menuOpen);int theme=visualizerLibrary.appearance.theme;press(ORBIS_PAD_BUTTON_RIGHT);assert(visualizerLibrary.appearance.theme==(theme+1)%5&&visualizerLibrary.dirty);press(ORBIS_PAD_BUTTON_DOWN);press(ORBIS_PAD_BUTTON_RIGHT);assert(visualizerLibrary.appearance.font==1);
 frame(ORBIS_PAD_BUTTON_DOWN,400000);assert(state.appSettingsRow>=2);state.appSettingsRow=2;int delay=visualizerLibrary.appearance.hudSeconds;press(ORBIS_PAD_BUTTON_RIGHT);assert(state.hud.delay==(uint64_t)(delay+1)*1000000);
 state.appSettingsRow=7;press(ORBIS_PAD_BUTTON_CROSS);assert(state.appCreditsOpen);press(ORBIS_PAD_BUTTON_CIRCLE);assert(!state.appCreditsOpen&&state.appSettingsOpen);press(ORBIS_PAD_BUTTON_OPTIONS);assert(!state.appSettingsOpen&&state.menuOpen);
 press(ORBIS_PAD_BUTTON_TRIANGLE);assert(state.visualizerMenuOpen&&!state.menuOpen);fake.touch.fingers=1;fake.touch.touch[0].x=100;press(ORBIS_PAD_BUTTON_TOUCH_PAD);assert(musicSearch.open&&presetSearchTarget);press(ORBIS_PAD_BUTTON_CIRCLE);press(ORBIS_PAD_BUTTON_CIRCLE);assert(!state.visualizerMenuOpen);
 // Settings owns all input while paused, and returns to the previous menu.
 state.paused=true;press(ORBIS_PAD_BUTTON_OPTIONS);assert(state.appSettingsOpen);state.appSettingsRow=5;press(ORBIS_PAD_BUTTON_RIGHT);press(ORBIS_PAD_BUTTON_CROSS);assert(visualizerLibrary.appearance.stats&&state.paused);press(ORBIS_PAD_BUTTON_CIRCLE);assert(!state.appSettingsOpen&&state.paused);
 state.preset=5;press(ORBIS_PAD_BUTTON_SQUARE);assert(state.builtinSettingsOpen);int variation=state.sandVariation;press(ORBIS_PAD_BUTTON_RIGHT);assert(state.sandVariation==(variation+1)%4);press(ORBIS_PAD_BUTTON_CIRCLE);
 state.preset=6;press(ORBIS_PAD_BUTTON_SQUARE);assert(state.builtinSettingsOpen&&state.builtinSettingsPreset==6);state.builtinSettingsRow=1;int trail=state.scopeTrail;press(ORBIS_PAD_BUTTON_RIGHT);assert(state.scopeTrail==(trail+1)%4);press(ORBIS_PAD_BUTTON_CIRCLE);
 state.preset=9;press(ORBIS_PAD_BUTTON_SQUARE);assert(state.builtinSettingsOpen&&state.builtinSettingsPreset==9);int rotation=cubeOrbitSettings.rotation;press(ORBIS_PAD_BUTTON_RIGHT);assert(cubeOrbitSettings.rotation==rotation+3);press(ORBIS_PAD_BUTTON_CIRCLE);
 state.preset=0;state.paused=false;state.hudSelection=3;press(ORBIS_PAD_BUTTON_CROSS);assert(state.paused);press(ORBIS_PAD_BUTTON_CROSS);assert(!state.paused);state.hudSelection=4;int oldSteps=steps;press(ORBIS_PAD_BUTTON_CROSS);assert(steps==oldSteps+1);
 press(ORBIS_PAD_BUTTON_TRIANGLE);assert(visualizerLibrary.count==10);visualizerLibrary.selection=5;press(ORBIS_PAD_BUTTON_SQUARE);assert(state.builtinSettingsReturn&&!state.visualizerMenuOpen);press(ORBIS_PAD_BUTTON_CIRCLE);assert(state.visualizerMenuOpen);press(ORBIS_PAD_BUTTON_CIRCLE);
 // Disconnect discards remembered touch position.
 fake.connected=false;frame(0);fake.connected=true;fake.touch.fingers=0;press(ORBIS_PAD_BUTTON_TOUCH_PAD);assert(state.menuOpen&&!musicSearch.open);
 // Mic Test temporarily captures input without changing the USB source.
 press(ORBIS_PAD_BUTTON_OPTIONS);state.appSettingsRow=6;state.microphone=false;int starts=micStartsTest;press(ORBIS_PAD_BUTTON_CROSS);assert(state.appMicTestOpen&&micStartsTest==starts+1&&!micPausedTest&&!state.microphone);press(ORBIS_PAD_BUTTON_CIRCLE);assert(!state.appMicTestOpen&&state.appSettingsOpen&&micPausedTest);
 state.microphone=true;press(ORBIS_PAD_BUTTON_CROSS);assert(state.appMicTestOpen&&!micPausedTest);press(ORBIS_PAD_BUTTON_OPTIONS);assert(!state.appMicTestOpen&&!state.appSettingsOpen&&!micPausedTest&&state.microphone);
 press(ORBIS_PAD_BUTTON_OPTIONS);state.appSettingsRow=6;press(ORBIS_PAD_BUTTON_CROSS);fake.touch.fingers=1;fake.touch.touch[0].x=1500;press(ORBIS_PAD_BUTTON_TOUCH_PAD);assert(!state.appMicTestOpen&&!state.appSettingsOpen&&state.menuOpen&&state.microphone&&!micPausedTest);
 puts("PASS actual controls: split touch clicks, contact ignored, recent/missing coordinates, L3 fallback, contextual search/apply, OPTIONS settings/credits/return, themes/fonts/HUD/stats, paused builtin menus, scope trail, playback and disconnect");}
