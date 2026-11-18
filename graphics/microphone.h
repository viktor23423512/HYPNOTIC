#pragma once
#include "audio_waveform.h"
#include "audio_spectrum.h"
#include "audio_stereo_scope.h"
void startMicrophone();
void pauseMicrophone(bool paused);
const char *microphoneStatus();
const char *microphoneDetails();
bool copyMicrophone(AudioWaveform &wave,AudioSpectrum &spectrum,AudioStereoScope &scope);
int microphonePeak();
