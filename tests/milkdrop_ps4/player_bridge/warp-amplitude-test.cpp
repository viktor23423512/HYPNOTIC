#include "../native_graphics/spatial_warp.hpp"
#include "player_spatial_warp.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <fstream>
int main(int argc,char **argv){assert(argc==3);SpatialWarp old;PlayerSpatialWarp fixed;PresetEquations eq;assert(eq.load(argv[1],true,true));assert(old.load(argv[1],true,true));assert(fixed.load(argv[1],true,true));
 for(unsigned frame=0;frame<120;++frame){double level=.4+(frame%30)*.1;assert(eq.evaluate(frame/60.,60,level,level*.9,level*.8));assert(old.build(eq.output,frame/60.f,1.f/60));assert(fixed.build(eq.output,frame/60.f,1.f/60));assert(old.mesh.count==fixed.mesh.count);assert(!memcmp(old.mesh.vertices.data(),fixed.mesh.vertices.data(),old.mesh.count*sizeof(PresetVertex)));}
 for(double peak:{6.,8.,12.,20.}){assert(eq.evaluate(3,60,peak,peak,peak));bool before=old.build(eq.output,3,1.f/60);bool after=fixed.build(eq.output,3,1.f/60);printf("Audio peak %.0f original %s fixed %s original-error %s\n",peak,before?"PASS":"REJECT",after?"PASS":"REJECT",old.error.c_str());assert(!before&&after);for(unsigned i=0;i<fixed.mesh.count;++i)for(float x:fixed.mesh.vertices[i].color)assert(std::isfinite(x));}
 for(double bad:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1e100})assert(!validPlayerWarpAmount(bad));
 const std::string fixture=std::string(argv[2]);
 for(const char *code:{"warp=1e100;","sy=0;","cx=10001;","zoom=1e-300;"}){
  {std::ofstream file(fixture);file<<"[preset00]\nzoom=1\nper_pixel_1="<<code<<"\n";}
  PlayerSpatialWarp invalid;assert(invalid.load(fixture.c_str(),true,true));PresetEquations::Output base={};base.zoom=1;base.decay=1;
  assert(!invalid.build(base,1,1.f/60));printf("Retained rejection %s : %s\n",code,invalid.error.c_str());
 }
 std::remove(fixture.c_str());
 assert(!validPresetStretch(0)&&!validPresetStretch(1e-8));assert(validPlayerWarpAmount(-1e25));
 puts("PASS exact original preset,120 normal frames byte-identical, four peak failures reproduced/fixed, invalid warp and singular stretch protections retained");
}
