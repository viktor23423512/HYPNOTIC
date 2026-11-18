#pragma once
#include <stdint.h>

// Five milliseconds at the fixed 48 kHz output rate. One shared gain for
// both channels; state survives reversals, so rapid toggles do not jump gain.
class PauseFade
{
    unsigned level = 0;
public:
    static constexpr unsigned frames = 240;
    bool silent() const { return level == 0; }
    void apply(int16_t *samples, unsigned count, bool playing)
    {
        for (unsigned frame = 0; frame < count; ++frame)
        {
            if (playing && level < frames) ++level;
            if (!playing && level > 0) --level;
            for (unsigned channel = 0; channel < 2; ++channel)
            {
                // Retain the existing half-scale playback level.
                const int value = samples[frame * 2 + channel];
                samples[frame * 2 + channel] = (int16_t)(value * (int)level / (2 * (int)frames));
            }
        }
    }
};
