#pragma once
#include <string>
// Common, source-driven MilkDrop shader profile. No preset-name branches.
bool translateCommonShader(const std::string &source,bool warp,std::string &glsl,std::string &error,bool *pointNoise=nullptr);

// Missing stages use projectM's standard stage, not a preset-specific fallback.
inline void completeShaderStages(std::string &warp,std::string &comp){
 if(warp.empty())warp="shader_body { ret=GetPixel(uv)*md_blur3.z; }";
 if(comp.empty())comp="shader_body { ret=GetPixel(uv); }";
}
