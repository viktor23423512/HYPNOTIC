#pragma once
#include "audio_reactivity.hpp"
#include "../../../graphics/audio_decoder.h"
#include "../../../graphics/pcm_resampler.h"
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>
#include <atomic>

enum TestAudioState {AudioStarting,AudioPlaying,AudioFailed};
static std::atomic<int> testAudioState{AudioStarting},testAudioCode{0};
static std::atomic<uint64_t> testAudioPublishedAt{0};
static std::atomic<const char *> testAudioFailureStep{"STARTUP"};
static std::atomic_flag testAudioLock=ATOMIC_FLAG_INIT;
static TestAudioFrame testAudioPublished;
static float testAudioUiPeak=0;
static bool testAudioUiAvailable=false;
static void audioFailure(const char *step,int code)
{
    printf("MILKDROP AUDIO FAILED %s %08X\n",step,(unsigned)code);
    testAudioFailureStep.store(step);testAudioCode.store(code);testAudioState.store(AudioFailed);
}
static void *testAudioWorker(void *)
{
    OrbisUserServiceInitializeParams parameters={};parameters.priority=ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    int rc=sceUserServiceInitialize(&parameters);
    printf("MILKDROP AUDIO USER INIT %08X\n",(unsigned)rc);
    rc=sceAudioOutInit();printf("MILKDROP AUDIO INIT %08X\n",(unsigned)rc);
    if(rc!=0){audioFailure("INIT",rc);return nullptr;}
    int32_t user=ORBIS_USER_SERVICE_USER_ID_SYSTEM,initial=ORBIS_USER_SERVICE_USER_ID_INVALID;
    const int lookup=sceUserServiceGetInitialUser(&initial);
    if(lookup==0 && initial!=ORBIS_USER_SERVICE_USER_ID_INVALID)user=initial;
    int port=sceAudioOutOpen(user,ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,0,1024,48000,ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    printf("MILKDROP AUDIO OPEN USER %08X LOOKUP %08X PORT %08X\n",(unsigned)user,(unsigned)lookup,(unsigned)port);
    if(port<0 && user!=ORBIS_USER_SERVICE_USER_ID_SYSTEM) {
        port=sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM,ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,0,1024,48000,ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        printf("MILKDROP AUDIO SYSTEM FALLBACK %08X\n",(unsigned)port);
    }
    if(port<0){audioFailure("OPEN",port);return nullptr;}
    AudioDecoder decoder;
    if(!decoder.open("/app0/assets/audio/track09.ogg")) {
        audioFailure("BUNDLED FILE",-1);sceAudioOutClose(port);return nullptr;
    }
    printf("MILKDROP AUDIO SOURCE %s RATE %u FRAMES %llu OUTPUT 48000\n",decoder.format(),decoder.sampleRate,(unsigned long long)decoder.frames);
    static SpectrumAnalyzer analyzer;
    AudioSpectrum spectrum;
    StereoPcmResampler resampler;
    alignas(16) int16_t samples[2048]={};
    TestAudioFrame snapshot;
    unsigned loops=0;
    for(;;) {
        if(!decoder.seek(0) || !resampler.begin(AudioDecoder::readFrames,&decoder,decoder.frames,decoder.sampleRate)) {
            audioFailure("RESAMPLER",-1);break;
        }
        analyzer.reset();spectrum=AudioSpectrum{};
        const uint64_t length=resampler.length();uint64_t cursor=0;
        printf("MILKDROP AUDIO LOOP %u OUTPUT FRAMES %llu\n",++loops,(unsigned long long)length);
        while(cursor<length) {
            const unsigned request=length-cursor<1024?(unsigned)(length-cursor):1024;
            const unsigned got=resampler.render(samples,request);
            if(got!=request || resampler.error()){audioFailure("DECODE",-1);break;}
            // Ten-ms loop-edge fades and the player's half-scale loudness.
            for(unsigned i=0;i<got;++i) {
                const uint64_t position=cursor+i;
                unsigned fade=position<480?(unsigned)position:480;
                const uint64_t tail=length-1-position;if(tail<fade)fade=(unsigned)tail;
                for(unsigned c=0;c<2;++c)samples[i*2+c]=(int16_t)((int)samples[i*2+c]*(int)fade/960);
            }
            memset(samples+got*2,0,(1024-got)*sizeof(int16_t)*2);
            rc=sceAudioOutOutput(port,samples);
            if(rc<0){audioFailure("OUTPUT",rc);break;}
            analyzer.process(samples,1024,spectrum,48000);
            makeAudioFrame(samples,1024,spectrum,snapshot);
            if(!testAudioLock.test_and_set(std::memory_order_acquire)) {
                testAudioPublished=snapshot;
                testAudioPublishedAt.store(sceKernelGetProcessTime());
                testAudioLock.clear(std::memory_order_release);
            }
            testAudioState.store(AudioPlaying);cursor+=got;
        }
        if(testAudioState.load()==AudioFailed)break;
    }
    sceAudioOutOutput(port,nullptr);sceAudioOutClose(port);
    return nullptr;
}
static void startTestAudio()
{
    OrbisPthread thread;
    const int rc=scePthreadCreate(&thread,nullptr,testAudioWorker,nullptr,"milkdrop-audio");
    if(rc!=0)audioFailure("THREAD",rc);
}
static void readTestAudio(TestAudioFrame &frame)
{
    if(!testAudioLock.test_and_set(std::memory_order_acquire)) {
        frame=testAudioPublished;testAudioLock.clear(std::memory_order_release);
    }
    const uint64_t now=sceKernelGetProcessTime(),last=testAudioPublishedAt.load();
    if(testAudioState.load()!=AudioPlaying || !last || now-last>250000)frame=TestAudioFrame{};
    testAudioUiPeak=frame.peak;testAudioUiAvailable=frame.available;
}
static const char *testAudioStatus()
{
    static char text[100];
    if(testAudioState.load()==AudioFailed)
        snprintf(text,sizeof(text),"AUDIO %s ERROR %08X - SEND KLOG",testAudioFailureStep.load(),(unsigned)testAudioCode.load());
    else if(testAudioState.load()==AudioPlaying && !testAudioUiAvailable)
        snprintf(text,sizeof(text),"WAITING FOR FRESH PCM - NO SYNTHETIC FALLBACK");
    else if(testAudioState.load()==AudioPlaying)
        snprintf(text,sizeof(text),"TRACK09 OGG LOOP - REAL PCM - PEAK %.3f - OUTPUT 48000 HZ",testAudioUiPeak);
    else snprintf(text,sizeof(text),"STARTING BUNDLED MUSIC CLIP - NO USB ACCESS REQUIRED");
    return text;
}
