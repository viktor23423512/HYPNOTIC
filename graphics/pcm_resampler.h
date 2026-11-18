#pragma once
#include <stdint.h>

// Stateful, allocation-free stereo S16 conversion to 48 kHz. Linear
// interpolation is deliberately lightweight for this initial playback test.
// Integer phase carries across blocks, avoiding timing drift and seams.
class StereoPcmResampler
{
public:
    typedef unsigned (*Reader)(void *, unsigned, int16_t *);
    bool begin(Reader reader, void *context, uint64_t frames, unsigned rate)
    {
        read = reader; source = context; inputFrames = frames; inputRate = rate;
        fetched = generated = 0; position = buffered = phase = 0; failed = false;
        if (!reader || !frames || rate<8000 || rate>48000) return false;
        outputFrames = (frames / rate) * 48000 + ((frames % rate) * 48000 + rate - 1) / rate;
        if (!pull(left)) return false;
        sourceIndex = 0;
        if (frames > 1) { if (!pull(right)) return false; }
        else { right[0] = left[0]; right[1] = left[1]; }
        return true;
    }
    unsigned render(int16_t *output, unsigned capacity)
    {
        unsigned count = 0;
        while (count < capacity && generated < outputFrames && !failed)
        {
            // Advance source positions only when another output is requested.
            if (phase >= 48000)
            {
                phase -= 48000;
                ++sourceIndex;
                left[0] = right[0]; left[1] = right[1];
                if (sourceIndex + 1 < inputFrames)
                { if (!pull(right)) break; }
                else { right[0] = left[0]; right[1] = left[1]; }
            }
            for (unsigned channel = 0; channel < 2; ++channel)
            {
                // 64-bit product covers the full signed-16 difference safely.
                const int64_t delta = (int)right[channel] - left[channel];
                output[count * 2 + channel] = (int16_t)(left[channel] + delta * phase / 48000);
            }
            phase += inputRate;
            ++generated;
            ++count;
        }
        return count;
    }
    bool error() const { return failed; }
    uint64_t length() const { return outputFrames; }
private:
    bool pull(int16_t *frame)
    {
        if (position == buffered)
        {
            const uint64_t remaining = inputFrames - fetched;
            unsigned request = remaining < 1024 ? (unsigned)remaining : 1024;
            if (!request) { failed = true; return false; }
            buffered = read(source, request, cache);
            position = 0;
            if (buffered != request) { failed = true; return false; }
            fetched += buffered;
        }
        frame[0] = cache[position * 2]; frame[1] = cache[position * 2 + 1];
        ++position;
        return true;
    }
    Reader read = nullptr;
    void *source = nullptr;
    uint64_t inputFrames = 0, outputFrames = 0, fetched = 0, generated = 0, sourceIndex = 0;
    unsigned inputRate = 0, phase = 0, position = 0, buffered = 0;
    bool failed = false;
    int16_t left[2] = {}, right[2] = {}, cache[2048] = {};
};
