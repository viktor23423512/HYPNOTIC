#pragma once
#include <stdint.h>

// Streaming, audio-thread-owned decoder. Output is always stereo signed-16 PCM.
class AudioDecoder
{
    enum Kind {None,Wav,Mp3,Flac,Vorbis} kind=None;
    void *handle=nullptr;
public:
    unsigned sampleRate=0,channels=0;
    uint64_t frames=0;
    AudioDecoder()=default;
    ~AudioDecoder(){close();}
    AudioDecoder(const AudioDecoder&)=delete;
    AudioDecoder &operator=(const AudioDecoder&)=delete;
    static bool supportedExtension(const char *path);
    bool open(const char *path);
    void close();
    unsigned read(unsigned count,int16_t *stereo);
    bool seek(uint64_t frame);
    const char *format() const;
    static unsigned readFrames(void *context,unsigned count,int16_t *out)
    {return static_cast<AudioDecoder *>(context)->read(count,out);}
};
