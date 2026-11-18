#include "microphone.h"
#include "audio_input_api.h"
#include "mic_open_policy.h"
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <orbis/UserService.h>
#include <orbis/Sysmodule.h>

enum MicState { Starting, Active, InitFailed, UserFailed, OpenFailed, InputFailed, ThreadFailed, ModuleFailed };
static std::atomic<int> micState{Starting};
static std::atomic<bool> micPaused{false};
static std::atomic<int> initCode{0}, openCode{0}, readCode{0}, silentCode{0}, peak{0};
static std::atomic<unsigned> blockFrames{0},openAttempts{0};
static std::atomic<bool> started{false};
static std::atomic_flag snapshotLock=ATOMIC_FLAG_INIT;
static AudioWaveform micWave;
static AudioSpectrum micSpectrum;
static AudioStereoScope micScope;

bool copyMicrophone(AudioWaveform &wave,AudioSpectrum &spectrum,AudioStereoScope &scope)
{
    if(snapshotLock.test_and_set(std::memory_order_acquire))return false;
    wave=micWave;spectrum=micSpectrum;scope=micScope;snapshotLock.clear(std::memory_order_release);
    if(micState.load()!=Active){spectrum.available=false;scope.available=false;}
    return true;
}
int microphonePeak(){return peak.load();}

static bool loadInputModule()
{
    const int result=sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOIN);
    initCode.store(result);
    printf("HYPNOTIC MIC MODULE LOAD %08X\n",(unsigned)result);
    OrbisKernelModule handles[128];size_t count=0;
    if(sceKernelGetModuleList(handles,sizeof(handles),&count)!=0)return false;
    if(count>128)count=128;
    const char *names[]={"sceAudioInInit","sceAudioInOpen","sceAudioInInput","sceAudioInGetSilentState","sceAudioInClose"};
    for(size_t i=0;i<count;++i)
    {
        OrbisKernelModuleInfo info={};info.size=sizeof(info);
        if(sceKernelGetModuleInfo(handles[i],&info)!=0 || !strstr(info.name,"libSceAudioIn"))continue;
        for(const char *name:names){void *address=nullptr;if(sceKernelDlsym(handles[i],name,&address)!=0 || !address){printf("HYPNOTIC MIC MISSING EXPORT %s\n",name);return false;}}
        return true;
    }
    return false;
}

static void *captureThread(void *)
{
    if(!loadInputModule()){micState.store(ModuleFailed);return NULL;}
    static SpectrumAnalyzer analyzer;
    static AudioSpectrum spectrum;
    static AudioStereoScope scope;
    scope.epoch=1ULL<<63; // Keep microphone history distinct from USB epochs.
    // UserService is initialized by openController() in main; use that user.
    int32_t user = ORBIS_USER_SERVICE_USER_ID_INVALID;
    const int userResult = sceUserServiceGetInitialUser(&user);
    if (userResult < 0 || user==ORBIS_USER_SERVICE_USER_ID_INVALID)
    {
        openCode.store(userResult);
        micState.store(UserFailed);
        return NULL;
    }
    const int result = sceAudioInInit();
    initCode.store(result);
    if (result < 0) { micState.store(InitFailed); return NULL; }
    // Voice-chat type (0), signed 16-bit mono format (0). The system selects
    // this user's input device. A DS4 headset mic is the intended first test;
    // this API does not force a specific physical microphone.
    const MicOpenResult opened=openMicrophoneBlock([user](unsigned frames){
        const int code=sceAudioInOpen(user,0,0,frames,16000,0);
        printf("HYPNOTIC MIC TRY FRAMES %u RATE 16000 RESULT %08X\n",frames,(unsigned)code);
        return code;
    });
    const unsigned frames=opened.frames;
    const int port=opened.port;
    blockFrames.store(frames);openAttempts.store(opened.attempts);
    openCode.store(port);
    printf("HYPNOTIC MIC OPEN USER %08X INIT %08X PORT %08X\n",(unsigned)user,(unsigned)result,(unsigned)port);
    if (port < 0) { micState.store(OpenFailed); return NULL; }
    alignas(16) static int16_t samples[2048];
    alignas(16) static int16_t stereo[2048*2];
    micState.store(Active);
    for (;;)
    {
        if (micPaused.load()) { sceKernelUsleep(10000); continue; }
        // Clear first so a short capture cannot publish stale samples.
        memset(samples, 0, frames*sizeof(samples[0]));
        const int inputResult = sceAudioInInput(port, samples);
        readCode.store(inputResult);
        if (inputResult < 0)
        {
            printf("HYPNOTIC MIC INPUT FAILED %08X\n",(unsigned)inputResult);
            micState.store(InputFailed);
            break;
        }
        silentCode.store(sceAudioInGetSilentState(port));
        int maximum = 0;
        for (unsigned i = 0; i < frames; ++i)
        {
            const int value = samples[i] < 0 ? -(int)samples[i] : (int)samples[i];
            if (value > maximum) maximum = value;
        }
        peak.store(maximum);
        AudioWaveform wave;
        waveformFromMono(samples, frames, wave);
        for(unsigned i=0;i<frames;++i)stereo[i*2]=stereo[i*2+1]=samples[i];
        analyzer.process(stereo,frames,spectrum,16000);
        scopeFromStereo(stereo,frames,scope);++scope.sequence;
        if(!micPaused.load() && !snapshotLock.test_and_set(std::memory_order_acquire))
        {micWave=wave;micSpectrum=spectrum;micScope=scope;snapshotLock.clear(std::memory_order_release);}
        // Input is expected to pace capture; yield even if an implementation
        // returns immediately with silence or no connected device.
        sceKernelUsleep(1000);
    }
    sceAudioInClose(port);
    peak.store(0);
    return NULL;
}

void startMicrophone()
{
    if(started.exchange(true))return;
    OrbisPthread thread;
    const int result = scePthreadCreate(&thread, NULL, captureThread, NULL, "hypnotic-mic");
    if (result != 0) { openCode.store(result); micState.store(ThreadFailed); }
    else scePthreadDetach(thread);
}
void pauseMicrophone(bool value) { micPaused.store(value); }
const char *microphoneStatus()
{
    switch (micState.load())
    {
        case Starting: return "OPENING MICROPHONE";
        case Active: return micPaused.load() ? "MIC MODE PAUSED" : "MIC MODE ACTIVE";
        case InitFailed: return "MIC INITIALIZATION FAILED";
        case UserFailed: return "MIC USER LOOKUP FAILED";
        case OpenFailed: return (uint32_t)openCode.load()==0x80260102u
            ?"MIC FORMAT UNAVAILABLE - CHECK PS4 AUDIO DEVICES":"MIC OPEN FAILED - CHECK PS4 AUDIO DEVICES";
        case InputFailed: return "MIC READ FAILED - RECONNECT AND RELAUNCH";
        case ModuleFailed: return "MIC MODULE UNAVAILABLE - USB PLAYER STILL AVAILABLE";
        default: return "MIC THREAD CREATION FAILED";
    }
}
const char *microphoneDetails()
{
    // Render-thread-owned text; the capture thread publishes scalar fields.
    static char text[192];
    snprintf(text, sizeof(text), "INIT %08X - OPEN %08X - READ %08X - SILENT %08X - PEAK %d - BLOCK %u TRY %u",
        (unsigned)initCode.load(), (unsigned)openCode.load(), (unsigned)readCode.load(),
        (unsigned)silentCode.load(), peak.load(),blockFrames.load(),openAttempts.load());
    return text;
}
