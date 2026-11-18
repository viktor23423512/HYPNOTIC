#pragma once
#include <array>
namespace libprojectM { namespace MilkdropPreset {struct PresetState {struct {std::array<float,480> waveformLeft{},waveformRight{};std::array<float,512> spectrumLeft{},spectrumRight{};} audioData;float waveScale=1,waveSmoothing=0;struct {unsigned viewportSizeX=512,viewportSizeY=288;double time=0;} renderContext;};}}
