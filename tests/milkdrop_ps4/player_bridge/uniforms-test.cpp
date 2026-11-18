#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include "../native_graphics/blur_config.hpp"
#include "../native_graphics/hue_field.hpp"
#include "../native_graphics/transform_limits.hpp"
struct PresetEquations {
    struct Output { double zoom,rot,decay,red,green,blue,dx,dy; double cx=.5,cy=.5,waveX=.5,waveY=.5,mystery=0;
        double blurMin[3]={0,0,0},blurMax[3]={1,1,1},blurEdge=0;
        double border[10]={},motion[9]={};
        double textureWrap=1;
        double zoomExponent=1;
        double waveAlpha=1,warpValue=0,stretchX=1,stretchY=1,q[32]={},audio[6]={},seconds=0,fps=60,frame=0;
        double gamma=1,echoZoom=1,echoAlpha=0,echoOrient=0,darkenCenter=0;
    };
};
static void shaderRoamingInputs(float seconds,float *output){
 const float fast[]={.329f,1.293f,5.070f,20.051f},fastPhase[]={1.2f,3.9f,2.5f,5.4f};
 const float slow[]={.0050f,.0085f,.0133f,.0217f},slowPhase[]={2.7f,5.3f,4.5f,3.8f};
 for(unsigned i=0;i<4;++i){output[i]=.5f+.5f*cosf(seconds*fast[i]+fastPhase[i]);output[4+i]=.5f+.5f*sinf(seconds*fast[i]+fastPhase[i]);output[8+i]=.5f+.5f*cosf(seconds*slow[i]+slowPhase[i]);output[12+i]=.5f+.5f*sinf(seconds*slow[i]+slowPhase[i]);}
}

static BlurConfig commonBlurConfig;static float *commonInputs=nullptr;
static float commonPresetRandom[4]={.1,.2,.3,.4},commonHueOffsets[4]={3,7,11,13};
struct Image{unsigned width,height,texture;};static Image feedback[2]={{512,288,1},{512,288,2}},imageTextures[2]={{64,128,3},{256,32,4}};
static unsigned imageChoice=0;static unsigned char tables[4][1024];static unsigned char *commonTables[2][2]={{tables[0],tables[1]},{tables[2],tables[3]}};
static void writeTexture(unsigned char *p,unsigned,unsigned texture,bool clamp){memcpy(p,&texture,4);p[4]=clamp;}
static uint32_t textureRandom=0x4859504e;
static uint32_t nextTextureRandom(){textureRandom^=textureRandom<<13;textureRandom^=textureRandom>>17;textureRandom^=textureRandom<<5;return textureRandom;}
static void fail(const char *s,int){throw std::runtime_error(s);}
#include "uniforms-original-under-test.hpp"
#include "player_uniforms.hpp"
int main(){
 alignas(16) float gpu[108]={},expected[108];commonInputs=gpu;
 checkPlayerUniforms();assert(playerUseStagedUniforms&&commonInputs==gpu);
 for(unsigned n=0;n<1000;++n){PresetEquations::Output v={};v.seconds=n*.017;v.frame=n;v.decay=.9+(n%10)*.01;v.textureWrap=n&1;v.blurEdge=(n%100)*.01;imageChoice=n&1;
  for(unsigned i=0;i<32;++i)v.q[i]=sin(n*.03+i)*3;
  for(unsigned i=0;i<6;++i)v.audio[i]=(n%17+i)*.01;
  auto seed=textureRandom;setCommonInputs(v);memcpy(expected,gpu,sizeof(gpu));auto expectedRandom=textureRandom;unsigned char expectedTables[sizeof(tables)];memcpy(expectedTables,tables,sizeof(tables));
  textureRandom=seed;playerSetCommonInputs(v);assert(commonInputs==gpu);assert(!memcmp(gpu,expected,sizeof(gpu)));assert(textureRandom==expectedRandom);assert(!memcmp(tables,expectedTables,sizeof(tables)));
 }
 PresetEquations::Output bad={};bad.blurEdge=2;bool thrown=false;try{playerSetCommonInputs(bad);}catch(const std::runtime_error&){thrown=true;}assert(thrown&&commonInputs==gpu);
 playerUseStagedUniforms=false;PresetEquations::Output v={};playerSetCommonInputs(v);assert(commonInputs==gpu);
 puts("PASS 1000 frames bit-exact uniforms, identical RNG state and descriptors, pointer restoration on exception, original fallback");
}
