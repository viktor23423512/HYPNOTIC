#pragma once
#include <stdint.h>

// Local declarations: the installed OpenOrbis AudioIn.h has void placeholders.
// Signature/type reference (not a claim of verified console support):
// https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/audio/audioin.h
// https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/audio/audioin.cpp
// These declarations do not modify the SDK or copy the emulator implementation.
extern "C" {
int32_t sceAudioInInit(void);
int32_t sceAudioInOpen(int32_t user, uint32_t type, uint32_t index,
                       uint32_t frames, uint32_t rate, uint32_t format);
int32_t sceAudioInInput(int32_t handle, void *samples);
int32_t sceAudioInGetSilentState(int32_t handle);
int32_t sceAudioInClose(int32_t handle);
}
