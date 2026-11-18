#include "audio_decoder.h"
#include <stdlib.h>
#include <string.h>
#define DR_WAV_IMPLEMENTATION
#include "third_party/dr_wav.h"
#define DR_MP3_NO_WCHAR
#define DR_MP3_IMPLEMENTATION
#include "third_party/dr_mp3.h"
#define DR_FLAC_NO_WCHAR
#define DR_FLAC_IMPLEMENTATION
#include "third_party/dr_flac.h"
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"

static bool extension(const char *path,const char *suffix)
{
    const char *dot=nullptr;for(const char *p=path;*p;++p){if(*p=='/' || *p=='\\')dot=nullptr;else if(*p=='.')dot=p;}
    if(!dot)return false;++dot;
    while(*dot && *suffix){char c=*dot++;if(c>='A' && c<='Z')c+='a'-'A';if(c!=*suffix++)return false;}
    return !*dot && !*suffix;
}
bool AudioDecoder::supportedExtension(const char *path)
{return path && (extension(path,"wav") || extension(path,"mp3") || extension(path,"flac") || extension(path,"ogg") || extension(path,"oga"));}
void AudioDecoder::close()
{
    if(handle)switch(kind){
        case Wav:drwav_uninit((drwav*)handle);free(handle);break;
        case Mp3:drmp3_uninit((drmp3*)handle);free(handle);break;
        case Flac:drflac_close((drflac*)handle);break;
        case Vorbis:stb_vorbis_close((stb_vorbis*)handle);break;
        default:break;
    }
    handle=nullptr;kind=None;sampleRate=channels=0;frames=0;
}
bool AudioDecoder::open(const char *path)
{
    close();
    if(!path)return false;
    if(extension(path,"wav")){
        auto p=(drwav*)malloc(sizeof(drwav));if(!p)return false;
        if(!drwav_init_file(p,path,nullptr)){free(p);return false;}
        handle=p;kind=Wav;sampleRate=p->sampleRate;channels=p->channels;frames=p->totalPCMFrameCount;
        if(p->translatedFormatTag!=DR_WAVE_FORMAT_PCM && p->translatedFormatTag!=DR_WAVE_FORMAT_IEEE_FLOAT){close();return false;}
    }else if(extension(path,"mp3")){
        auto p=(drmp3*)malloc(sizeof(drmp3));if(!p)return false;
        if(!drmp3_init_file(p,path,nullptr)){free(p);return false;}
        handle=p;kind=Mp3;sampleRate=p->sampleRate;channels=p->channels;
        frames=drmp3_get_pcm_frame_count(p);
        if(!drmp3_seek_to_pcm_frame(p,0)){close();return false;}
    }else if(extension(path,"flac")){
        auto p=drflac_open_file(path,nullptr);if(!p)return false;
        handle=p;kind=Flac;sampleRate=p->sampleRate;channels=p->channels;frames=p->totalPCMFrameCount;
    }else if(extension(path,"ogg") || extension(path,"oga")){
        int error=0;auto p=stb_vorbis_open_filename(path,&error,nullptr);if(!p)return false;
        handle=p;kind=Vorbis;auto info=stb_vorbis_get_info(p);sampleRate=info.sample_rate;channels=info.channels;
        frames=stb_vorbis_stream_length_in_samples(p);
        if(!stb_vorbis_seek(p,0)){close();return false;}
    }
    // First release keeps the existing lightweight resampler at <=48 kHz.
    if(!handle || !frames || (channels!=1 && channels!=2) || sampleRate<8000 || sampleRate>48000){close();return false;}
    return true;
}
unsigned AudioDecoder::read(unsigned count,int16_t *out)
{
    if(!handle || !out || !count)return 0;
    // The resampler requests at most 1024 frames; bound external callers too.
    if(count>1024)count=1024;
    unsigned got=0;
    switch(kind){
        case Wav:got=(unsigned)drwav_read_pcm_frames_s16((drwav*)handle,count,out);break;
        case Mp3:got=(unsigned)drmp3_read_pcm_frames_s16((drmp3*)handle,count,out);break;
        case Flac:got=(unsigned)drflac_read_pcm_frames_s16((drflac*)handle,count,out);break;
        case Vorbis:got=(unsigned)stb_vorbis_get_samples_short_interleaved((stb_vorbis*)handle,channels,out,count*channels);break;
        default:break;
    }
    if(channels==1)for(unsigned i=got;i>0;--i)out[(i-1)*2]=out[(i-1)*2+1]=out[i-1];
    return got;
}
bool AudioDecoder::seek(uint64_t frame)
{
    if(!handle || frame>frames)return false;
    switch(kind){
        case Wav:return drwav_seek_to_pcm_frame((drwav*)handle,frame)!=0;
        case Mp3:return drmp3_seek_to_pcm_frame((drmp3*)handle,frame)!=0;
        case Flac:return drflac_seek_to_pcm_frame((drflac*)handle,frame)!=0;
        case Vorbis:return frame<=0xffffffffu && stb_vorbis_seek((stb_vorbis*)handle,(unsigned)frame)!=0;
        default:return false;
    }
}
const char *AudioDecoder::format() const
{switch(kind){case Wav:return "WAV";case Mp3:return "MP3";case Flac:return "FLAC";case Vorbis:return "OGG VORBIS";default:return "AUDIO";}}
