#pragma once
#include <string>
// projectM's unmodified MIT HLSL parser/GLSL generator, plus an explicit native
// single-main-texture composite ABI. Not a full MilkDrop shader environment.
bool translateComposite(const std::string &body,std::string &glsl,std::string &error);
bool translateTextured(const std::string &body,std::string &glsl,std::string &error,bool warp);
bool translateWarp(const std::string &body,std::string &glsl,std::string &error);
