#pragma once
#include <stdint.h>

constexpr unsigned AUDIO_WAVE_POINTS = 256;
struct AudioWaveform { float points[AUDIO_WAVE_POINTS]; };

// 1024 stereo frames -> 256 mono points, averaging four frames per point.
// A short final block is padded with silence without reading beyond its end.
inline void waveformFromStereo(const int16_t *samples, unsigned frames, AudioWaveform &out)
{
    if (frames > 1024) frames = 1024;
    for (unsigned point = 0; point < AUDIO_WAVE_POINTS; ++point)
    {
        int sum = 0;
        for (unsigned n = 0; n < 4; ++n)
        {
            unsigned frame = point * 4 + n;
            if (frame < frames) sum += (int)samples[frame * 2] + samples[frame * 2 + 1];
        }
        out.points[point] = sum / (8.0f * 32768.0f);
    }
}

// Mono input is spread across the same 256-point visualization snapshot.
inline void waveformFromMono(const int16_t *samples, unsigned frames, AudioWaveform &out)
{
    for (unsigned i = 0; i < AUDIO_WAVE_POINTS; ++i)
    {
        unsigned start = i * frames / AUDIO_WAVE_POINTS;
        unsigned end = (i + 1) * frames / AUDIO_WAVE_POINTS;
        if (end <= start) end = start + 1;
        int sum = 0;
        unsigned count = 0;
        for (unsigned n = start; n < end && n < frames; ++n)
        { sum += samples[n]; ++count; }
        out.points[i] = count ? sum / (count * 32768.0f) : 0.0f;
    }
}
