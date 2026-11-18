#pragma once
// Keep all original calculations and RNG calls. Only the CPU work destination
// changes; the existing GPU descriptor still points to gpuInputs throughout.
static bool playerUseStagedUniforms=true;
static void playerSetCommonInputs(const PresetEquations::Output &v){
 if(!playerUseStagedUniforms){setCommonInputs(v);return;}
 alignas(16) float cpuInputs[108];
 float *const gpuInputs=commonInputs;
 struct Restore {float *original;~Restore(){commonInputs=original;}} restore{gpuInputs};
 commonInputs=cpuInputs;
 setCommonInputs(v);
 memcpy(gpuInputs,cpuInputs,sizeof(cpuInputs));
}

static void checkPlayerUniforms(){
 alignas(16) unsigned char saved[432],expected[432];memcpy(saved,commonInputs,sizeof(saved));
 const auto savedRandom=textureRandom;const auto savedConfig=commonBlurConfig;bool equal=true;
 for(unsigned frame=0;frame<4;++frame){PresetEquations::Output v={};v.decay=.98;v.seconds=frame*123.456;v.fps=59.94;v.frame=frame*100;v.textureWrap=frame&1;v.blurEdge=frame*.2;
  for(unsigned i=0;i<6;++i)v.audio[i]=(frame+i)*.123;
  for(unsigned i=0;i<32;++i)v.q[i]=(int(i)-16)*.013*frame;
  const auto random=textureRandom;setCommonInputs(v);memcpy(expected,commonInputs,sizeof(expected));
  textureRandom=random;playerSetCommonInputs(v);equal&=memcmp(expected,commonInputs,sizeof(expected))==0;
 }
 textureRandom=savedRandom;commonBlurConfig=savedConfig;memcpy(commonInputs,saved,sizeof(saved));playerUseStagedUniforms=equal;
 printf("HYPNOTIC MILKDROP UNIFORM BYTE CHECK %s - %s\n",equal?"PASS":"FAIL",equal?"CPU STAGED":"ORIGINAL FALLBACK");
}
