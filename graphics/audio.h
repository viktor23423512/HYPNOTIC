#pragma once

// Playback runs independently of the framebuffer loop. USB files are read-only.
#include "audio_output_settings.h"
void setAudioOutputSettings(const AudioOutputSettings &settings);
void startUsbAudio();
void pauseUsbAudio(bool paused);
const char *usbAudioStatus();
const char *usbAudioTrack();

#include "audio_waveform.h"
#include "audio_spectrum.h"
#include "audio_stereo_scope.h"
bool copyAudioStereoScope(AudioStereoScope &out);
bool copyAudioSpectrum(AudioSpectrum &out);
// Nonblocking snapshot: false means keep the previous frame.
bool copyAudioWaveform(AudioWaveform &out);

// Shared visualization sink; called by one active audio source.
void publishAudioWaveform(const AudioWaveform &wave);

const char *usbAudioFormat();

int usbTrackCount();
const char *usbTrackName(int index);
int usbActiveTrack();
void playUsbTrack(int index);

uint64_t usbTrackDurationMs(int index);
uint64_t usbPlaybackPositionMs();
uint64_t usbPlaybackDurationMs();
void seekUsbAudio(int seconds);

// Read-only, immutable startup catalogue. Folder -1 is the USB drive list.
bool usbLibraryReady();
bool usbLibraryLimited();
int usbFolderCount();
const char *usbFolderPath(int index);
int usbFolderParent(int index);
int usbTrackFolder(int index);

void seekUsbAudioMs(int64_t milliseconds);
void stepUsbTrack(int direction);
bool usbShuffle();void toggleUsbShuffle();
int usbRepeat();void cycleUsbRepeat();
bool usbAudioFinished();
