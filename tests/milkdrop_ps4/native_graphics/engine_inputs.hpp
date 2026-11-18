#pragma once
#include <cmath>
// Source conventions: projectM 4.1.8 PerFrameContext, PerPixelContext and
// MilkdropShader::_c8.._c11. Native evaluation uses a fixed 16x10/512x288 mesh.
static const char *const engineInputNames[]={"meshx","meshy","pixelsx","pixelsy","aspectx","aspecty"};
static void loadEngineInputs(PRJM_EVAL_F **variables,bool pixel){
 const double values[]={16,10,512,288,1,pixel?.5625:1./.5625};
 for(unsigned i=0;i<6;++i)*variables[i]=values[i];
}
static void shaderRoamingInputs(float seconds,float *output){
 const float fast[]={.329f,1.293f,5.070f,20.051f},fastPhase[]={1.2f,3.9f,2.5f,5.4f};
 const float slow[]={.0050f,.0085f,.0133f,.0217f},slowPhase[]={2.7f,5.3f,4.5f,3.8f};
 for(unsigned i=0;i<4;++i){output[i]=.5f+.5f*cosf(seconds*fast[i]+fastPhase[i]);output[4+i]=.5f+.5f*sinf(seconds*fast[i]+fastPhase[i]);output[8+i]=.5f+.5f*cosf(seconds*slow[i]+slowPhase[i]);output[12+i]=.5f+.5f*sinf(seconds*slow[i]+slowPhase[i]);}
}
