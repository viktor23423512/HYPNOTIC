#include "audio.h"
#include "audio_decoder.h"
#include "pcm_resampler.h"
#include "track_library.h"
#include "track_sequence.h"
#include "usb_folder_library.h"
#include <sys/stat.h>
#include "playback_position.h"
#include "pause_fade.h"
#include "audio_output_processor.h"
#include "filesystem_access.h"
#include <atomic>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>


enum AudioStatus { Ready, Scanning, NoUsb, NoWav, Unsupported, Playing, Finished, ReadError, OutputError, ThreadError };
static std::atomic<int> status{Scanning};
static std::atomic<bool> paused{false};
static std::atomic<uint64_t> outputControls{AudioOutputSettings{}.packed()};
void setAudioOutputSettings(const AudioOutputSettings &settings){outputControls.store(settings.packed(),std::memory_order_relaxed);}
static TrackLibrary library;
static UsbFolderLibrary folders;
static std::atomic<bool> libraryReady{false};
static std::atomic<int> activeTrack{-1}, requestedTrack{-1};
static std::atomic<unsigned> trackRate{0};
static std::atomic<int> trackFormat{0};
static const char *formats[]={"AUDIO","WAV","MP3","FLAC","OGG VORBIS"};
static int formatIndex(const char *name){for(int i=1;i<5;++i)if(strcmp(name,formats[i])==0)return i;return 0;}
static std::atomic<int64_t> requestedSeek{0};
static std::atomic<int> requestedStep{0},repeatMode{0};
static std::atomic<bool> shuffleMode{false};
static std::atomic<uint64_t> playbackFrame{0}, playbackLength{0};
static char diagnostic[256] = {};
static std::atomic<int> audioInitResult{0},audioOpenResult{0},audioSystemResult{0},audioOutputResult{0};
static std::atomic<bool> audioSystemTried{false};
static std::atomic_flag waveformLock = ATOMIC_FLAG_INIT;
static AudioWaveform publishedWaveform = {};
static std::atomic_flag spectrumLock=ATOMIC_FLAG_INIT;
static AudioSpectrum publishedSpectrum;
static std::atomic_flag scopeLock=ATOMIC_FLAG_INIT;
static AudioStereoScope publishedScope,workingScope;
static void publishScope(const int16_t *pcm,unsigned count)
{
    if(pcm)scopeFromStereo(pcm,count,workingScope);
    else {workingScope.count=0;workingScope.available=false;++workingScope.epoch;}
    ++workingScope.sequence;
    if(!scopeLock.test_and_set(std::memory_order_acquire))
    {publishedScope=workingScope;scopeLock.clear(std::memory_order_release);}
}
bool copyAudioStereoScope(AudioStereoScope &out)
{
    if(scopeLock.test_and_set(std::memory_order_acquire))return false;
    out=publishedScope;scopeLock.clear(std::memory_order_release);
    out.available=out.available && status.load()==Playing;return true;
}
static void publishSpectrum(const AudioSpectrum &value)
{
    if(!spectrumLock.test_and_set(std::memory_order_acquire))
    { publishedSpectrum=value; spectrumLock.clear(std::memory_order_release); }
}
bool copyAudioSpectrum(AudioSpectrum &out)
{
    if(spectrumLock.test_and_set(std::memory_order_acquire)) return false;
    out=publishedSpectrum; spectrumLock.clear(std::memory_order_release);
    out.available=out.available && status.load()==Playing;
    return true;
}

void publishAudioWaveform(const AudioWaveform &wave)
{
    // Never wait on the renderer in the audio thread. Drop a visual update
    // if it is copying a snapshot; playback always takes priority.
    if (!waveformLock.test_and_set(std::memory_order_acquire))
    {
        publishedWaveform = wave;
        waveformLock.clear(std::memory_order_release);
    }
}

bool copyAudioWaveform(AudioWaveform &out)
{
    if (waveformLock.test_and_set(std::memory_order_acquire)) return false;
    out = publishedWaveform;
    waveformLock.clear(std::memory_order_release);
    return true;
}


#include "usb_music_scan.h"
static bool scanTracks(){
 bool foundAudio=false;int readError=0,firstNative=0,firstAlias=0;
 for(int usb=0;usb<8 && library.count<TrackLibrary::capacity;++usb){
  char root[32];int fd=-1;
  for(int style=0;style<2;++style){snprintf(root,sizeof(root),style?"/usb%d":"/mnt/usb%d",usb);fd=sceKernelOpen(root,0,0);
   if(usb==0){if(!style)firstNative=fd<0?fd:0;else firstAlias=fd<0?fd:0;}if(fd>=0)break;
  }
  if(fd<0)continue;sceKernelClose(fd);int parent=folders.add(root,-1);if(parent>=0)scanMusicFolder(parent,0,foundAudio,readError);
 }
 library.sort();
 snprintf(diagnostic,sizeof(diagnostic),"MNT USB0 %08X - USB0 %08X - READ %08X",(unsigned)firstNative,(unsigned)firstAlias,(unsigned)readError);
 printf("HYPNOTIC USB LIBRARY %d TRACKS %d FOLDERS LIMITED %u READ %08X\n",library.count,folders.count,folders.limited,(unsigned)readError);
 // Even an empty USB has browsable folders. Release-publish both catalogues.
 status.store(library.count?Ready:!folders.count?NoUsb:readError?ReadError:foundAudio?Unsupported:NoWav);
 libraryReady.store(true);return library.count>0;
}

static int openAudioOutput()
{
    // Register/open with the original application credentials, before libjbc
    // changes filesystem access. The GPU mini-app must register its own route.
    const int init=sceAudioOutInit();
    audioInitResult.store(init);
    printf("HYPNOTIC AUDIO INIT %08X - BEFORE USB ACCESS\n",(unsigned)init);
    if(init!=0) return -1;
    int32_t user=ORBIS_USER_SERVICE_USER_ID_SYSTEM;
    int32_t initialUser=ORBIS_USER_SERVICE_USER_ID_INVALID;
    const int userResult=sceUserServiceGetInitialUser(&initialUser);
    if(userResult==0 && initialUser!=ORBIS_USER_SERVICE_USER_ID_INVALID) user=initialUser;
    int port=sceAudioOutOpen(user,ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,0,1024,48000,
        ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    audioOpenResult.store(port);
    printf("HYPNOTIC AUDIO OPEN USER %08X LOOKUP %08X RESULT %08X\n",
        (unsigned)user,(unsigned)userResult,(unsigned)port);
    if(port<0 && user!=ORBIS_USER_SERVICE_USER_ID_SYSTEM)
    {
        // SYSTEM was the working route in the earlier CPU player; retain it
        // as a bounded fallback rather than assuming one user fits all modes.
        audioSystemTried.store(true);
        port=sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM,ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,
            0,1024,48000,ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        audioSystemResult.store(port);
        printf("HYPNOTIC AUDIO SYSTEM FALLBACK %08X\n",(unsigned)port);
    }
    return port;
}

static int outputAudio(int port,const void *samples)
{
    const int result=sceAudioOutOutput(port,samples);
    if(result<0)
    {
        audioOutputResult.store(result);
        printf("HYPNOTIC AUDIO OUTPUT ERROR %08X\n",(unsigned)result);
    }
    return result;
}

static void *audioThread(void *)
{
    static SpectrumAnalyzer spectrumAnalyzer;
    const int port=openAudioOutput();
    prepareUsbAccess();
    printf("Filesystem access: %s\n", usbAccessStatus());
    if (!scanTracks()) { if(port>=0) sceAudioOutClose(port); return NULL; }
    const unsigned framesPerBlock = 1024;
    if (port < 0) { status.store(OutputError); return NULL; }
    alignas(16) int16_t samples[framesPerBlock * 2],outputSamples[framesPerBlock * 2];
    AudioOutputProcessor outputProcessor;
    StereoPcmResampler resampler;
    TrackSequence sequence;sequence.random=(unsigned)sceKernelGetProcessTime()|1u;int automaticTrack=-1;
    for (;;)
    {
        int selected=requestedTrack.exchange(-1);const int step=requestedStep.exchange(0);
        if(selected<0 && step)selected=sequence.advance(library,activeTrack.load(),step,shuffleMode.load(),repeatMode.load(),false);
        if(selected<0)selected=automaticTrack;automaticTrack=-1;
        int64_t initialSeek=0;
        if (selected < 0)
        {
            initialSeek = requestedSeek.exchange(0);
            if (initialSeek && status.load() == Finished) selected = activeTrack.load();
            else { sceKernelUsleep(10000); continue; }
        }
        if (selected >= library.count) continue;
        requestedSeek.store(0);
        AudioDecoder decoder;
        if (!decoder.open(library.paths[selected]))
        { status.store(ReadError); continue; }
        // Revalidate because a USB file could have changed after scanning.
        if(!resampler.begin(AudioDecoder::readFrames,&decoder,decoder.frames,decoder.sampleRate))
        { status.store(ReadError); continue; }
        activeTrack.store(selected);sequence.prepare(library,selected,shuffleMode.load());
        printf("HYPNOTIC TRANSPORT TRACK %d SHUFFLE %u REPEAT %d PAUSED %u\n",selected,shuffleMode.load(),repeatMode.load(),paused.load());
        spectrumAnalyzer.reset(); publishSpectrum(AudioSpectrum{});publishScope(nullptr,0);outputProcessor.resetFilters();
        trackRate.store(decoder.sampleRate);trackFormat.store(formatIndex(decoder.format()));
        const uint64_t length = resampler.length();
        uint64_t cursor = 0;
        uint64_t remaining = length;
        playbackLength.store(length);
        if (initialSeek)
        {
            const uint64_t target = seekTargetFrameMs(playbackFrame.load(), length, initialSeek);
            const uint64_t sourceFrame = scaleAudioFrames(target, 48000, decoder.sampleRate);
            if (target == length) { remaining = 0; cursor = length; }
            else if (!decoder.seek(sourceFrame)
                || !resampler.begin(AudioDecoder::readFrames, &decoder, decoder.frames - sourceFrame, decoder.sampleRate))
            { decoder.close(); status.store(ReadError); continue; }
            else { remaining = resampler.length(); cursor = scaleAudioFrames(sourceFrame, decoder.sampleRate, 48000); }
        }
        playbackFrame.store(cursor);
        status.store(Playing);
        PauseFade fade;
        while (remaining > 0 && requestedTrack.load() < 0 && requestedStep.load()==0)
        {
            const int64_t seekMs = requestedSeek.exchange(0);
            if (seekMs)
            {
                spectrumAnalyzer.reset(); publishSpectrum(AudioSpectrum{});publishScope(nullptr,0);outputProcessor.resetFilters();
                // Drain queued sound before discarding decoder/resampler state.
                if (outputAudio(port, NULL) < 0) { status.store(OutputError); break; }
                const uint64_t target = seekTargetFrameMs(cursor, length, seekMs);
                if (target == length)
                { cursor = length; remaining = 0; playbackFrame.store(cursor); break; }
                const uint64_t sourceFrame = scaleAudioFrames(target, 48000, decoder.sampleRate);
                if (!decoder.seek(sourceFrame)
                    || !resampler.begin(AudioDecoder::readFrames, &decoder, decoder.frames - sourceFrame, decoder.sampleRate))
                { status.store(ReadError); break; }
                remaining = resampler.length();
                cursor = scaleAudioFrames(sourceFrame, decoder.sampleRate, 48000);
                playbackFrame.store(cursor);
                publishAudioWaveform(AudioWaveform{});fade=PauseFade{};
            }
            const bool wantsPause = paused.load();
            if (wantsPause && fade.silent()) { sceKernelUsleep(10000); continue; }
            if (outputAudio(port, NULL) < 0)
            { status.store(OutputError); break; }
            memset(samples, 0, sizeof(samples));
            // Decode only the short fade tail when pausing. The rest of the
            // output block stays zero; the paused song does not keep advancing.
            const unsigned limit = wantsPause ? PauseFade::frames : framesPerBlock;
            const unsigned requested = remaining < limit ? (unsigned)remaining : limit;
            const unsigned count = resampler.render(samples, requested);
            if (count != requested || resampler.error())
            { status.store(ReadError); break; }
            remaining -= count;
            fade.apply(samples, count, !wantsPause);
            memcpy(outputSamples,samples,sizeof(samples));
            outputProcessor.configure(AudioOutputSettings::unpack(outputControls.load(std::memory_order_relaxed)));
            outputProcessor.process(outputSamples,framesPerBlock);
            if (outputAudio(port, outputSamples) < 0)
            { status.store(OutputError); break; }
            AudioWaveform wave;
            waveformFromStereo(samples, count, wave);
            publishAudioWaveform(wave);
            publishScope(samples,count);
            AudioSpectrum spectrum;
            if(spectrumAnalyzer.process(samples,count,spectrum)) publishSpectrum(spectrum);
            cursor += count;
            playbackFrame.store(cursor < length ? cursor : length);
        }
        outputAudio(port, NULL);
        decoder.close();
        publishAudioWaveform(AudioWaveform{});
        publishSpectrum(AudioSpectrum{});
        publishScope(nullptr,0);
        if (status.load() == OutputError) { sceAudioOutClose(port); return NULL; }
        if (remaining == 0 && status.load() == Playing) { playbackFrame.store(length); status.store(Finished);
            automaticTrack=sequence.advance(library,selected,1,shuffleMode.load(),repeatMode.load(),true);
        }
        // Stay alive after completion/errors: another menu choice starts again.
    }
}

void startUsbAudio()
{
    OrbisPthread thread;
    const int result = scePthreadCreate(&thread, NULL, audioThread, NULL, "hypnotic-audio");
    if (result != 0)
    {
        printf("Audio thread error: %x\n", result);
        status.store(ThreadError);
    }
    else scePthreadDetach(thread);
}

void pauseUsbAudio(bool value) { paused.store(value); }
int usbTrackCount() { return libraryReady.load() ? library.count : 0; }
const char *usbTrackName(int index)
{
    return index >= 0 && index < usbTrackCount() ? TrackLibrary::name(library.paths[index]) : "";
}
int usbActiveTrack() { return activeTrack.load(); }
void playUsbTrack(int index)
{
    if (index >= 0 && index < usbTrackCount())
    { requestedSeek.store(0);requestedStep.store(0);requestedTrack.store(index); }
}
const char *usbAudioTrack()
{
    if(status.load()==OutputError)
    {
        static char errorText[192];
        if(audioSystemTried.load())
            snprintf(errorText,sizeof(errorText),"AUDIO INIT %08X OPEN %08X SYSTEM %08X OUTPUT %08X",
                (unsigned)audioInitResult.load(),(unsigned)audioOpenResult.load(),
                (unsigned)audioSystemResult.load(),(unsigned)audioOutputResult.load());
        else
            snprintf(errorText,sizeof(errorText),"AUDIO INIT %08X OPEN %08X OUTPUT %08X",
                (unsigned)audioInitResult.load(),(unsigned)audioOpenResult.load(),(unsigned)audioOutputResult.load());
        return errorText;
    }
    const int selected = activeTrack.load();
    if (selected >= 0) return library.paths[selected];
    return status.load() == Scanning ? "" : diagnostic;
}
const char *usbAudioStatus()
{
    switch (status.load())
    {
        case Ready: return "SELECT A TRACK IN THE MUSIC MENU";
        case Scanning: return "SCANNING USB";
        case NoUsb: return "USB NOT ACCESSIBLE - SEE ERROR CODES BELOW";
        case NoWav: return "NO WAV MP3 FLAC OR OGG IN USB MUSIC FOLDER OR ROOT";
        case Unsupported: return "SUPPORTED AUDIO: WAV MP3 FLAC OGG VORBIS - MONO/STEREO 8-48 KHZ";
        case Playing: return paused.load() ? "PAUSED" : "PLAYING AUDIO";
        case Finished: return "FINISHED - SELECT A TRACK TO PLAY AGAIN";
        case ReadError: return "USB READ ERROR - RECONNECT AND RELAUNCH";
        case OutputError: return "AUDIO OUTPUT ERROR";
        default: return "AUDIO THREAD ERROR";
    }
}

const char *usbAudioFormat()
{
    static char text[128];
    snprintf(text,sizeof(text),"SOURCE %s %u HZ - OUTPUT 48000 HZ - HYPNOTIC 0.58",formats[trackFormat.load()],trackRate.load());
    return text;
}

uint64_t usbTrackDurationMs(int index)
{
    return index >= 0 && index < usbTrackCount() ? library.durationMs[index] : 0;
}
uint64_t usbPlaybackPositionMs() { return playbackFrame.load() / 48; }
uint64_t usbPlaybackDurationMs() { return playbackLength.load() / 48; }
void seekUsbAudioMs(int64_t milliseconds)
{
    const int currentStatus = status.load();
    if (activeTrack.load() >= 0 && requestedTrack.load() < 0
        && (currentStatus == Playing || currentStatus == Finished))
        requestedSeek.fetch_add(milliseconds);
}

bool usbLibraryReady(){return libraryReady.load();}
bool usbLibraryLimited(){return libraryReady.load() && folders.limited;}
int usbFolderCount(){return libraryReady.load()?folders.count:0;}
const char *usbFolderPath(int i){return i>=0 && i<usbFolderCount()?folders.paths[i]:"";}
int usbFolderParent(int i){return i>=0 && i<usbFolderCount()?folders.parent[i]:-1;}
int usbTrackFolder(int i){return i>=0 && i<usbTrackCount()?library.folder[i]:-1;}

void seekUsbAudio(int seconds){seekUsbAudioMs((int64_t)seconds*1000);}
void stepUsbTrack(int direction){if(direction && usbActiveTrack()>=0){requestedSeek.store(0);requestedStep.fetch_add(direction<0?-1:1);}}
bool usbShuffle(){return shuffleMode.load();}
void toggleUsbShuffle(){shuffleMode.store(!shuffleMode.load());printf("HYPNOTIC TRANSPORT SHUFFLE %u\n",shuffleMode.load());}
int usbRepeat(){return repeatMode.load();}
void cycleUsbRepeat(){repeatMode.store((repeatMode.load()+1)%3);printf("HYPNOTIC TRANSPORT REPEAT %d\n",repeatMode.load());}
bool usbAudioFinished(){return status.load()==Finished;}
