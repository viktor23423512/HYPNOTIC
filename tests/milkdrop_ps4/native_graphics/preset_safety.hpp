#pragma once
// Compatibility rejection is data, never a fatal application error.
// Shader-enabled candidates are restricted to the two audited pipelines.
// This does not sandbox the native compiler or recover GPU faults.
static bool preparePreset(const char *path,bool shaders,PresetEquations &equations,
                          LegacyShake &wave,SpatialWarp &warp,std::string &reason,bool extended=false){
    reason.clear();
    if(!wave.load(path,true,shaders,extended)){reason="FEATURE CHECK: "+wave.error;return false;}
    if(!equations.load(path,true,shaders)){reason="EQUATION LOAD: "+equations.error;return false;}
    if(!warp.load(path,shaders,extended,equations.sharedMemory)){reason="WARP LOAD: "+warp.error;return false;}
    return true;
}
