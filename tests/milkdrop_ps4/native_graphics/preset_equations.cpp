#include "preset_equations.hpp"
#include "engine_inputs.hpp"
#include <cstdio>
#include "blur_config.hpp"
#include "transform_limits.hpp"

// The separate test executes all equations sequentially on its main thread.
// These are the documented host callbacks, not replacements for SDK APIs.
void projectm_eval_memory_host_lock_mutex() {}
void projectm_eval_memory_host_unlock_mutex() {}

PresetEquations::~PresetEquations() {
    if(perFrame)projectm_eval_code_destroy(perFrame);
    if(initial)projectm_eval_code_destroy(initial);
    if(context)projectm_eval_context_destroy(context);
}
void PresetEquations::restart(){
    frames=0;if(!context)return;if(sharedMemory)sharedMemory->clear();projectm_eval_context_reset_variables(context);
    *zoom=defaults[0];*rot=defaults[1];*decay=defaults[2];*red=defaults[3];*green=defaults[4];*blue=defaults[5];
    *pushX=defaults[6];*pushY=defaults[7];*centerX=defaults[8];*centerY=defaults[9];
    *waveX=defaults[10];*waveY=defaults[11];*mystery=defaults[12];
    *warpValue=defaults[13];*stretchX=defaults[14];*stretchY=defaults[15];*waveAlpha=defaults[16];
    for(unsigned i=0;i<7;++i)*blurParameters[i]=blurDefaults[i];
    for(unsigned i=0;i<5;++i)*finalParameters[i]=finalDefaults[i];
    loadEngineInputs(engineInputs,false);*zoomExp=zoomExpDefault;
    for(unsigned i=0;i<19;++i)*layerParameters[i]=layerDefaults[i];
    *wrapParameter=wrapDefault;
    *time=0;*fps=60;*frame=0;*bass=*mid=*treb=*bassAtt=*midAtt=*trebAtt=0;
    if(initial)projectm_eval_code_execute(initial);
}
bool PresetEquations::compilerError(const char *stage) {
    int line=0,column=0;
    const char *message=projectm_eval_get_error(context,&line,&column);
    error=std::string(stage)+" L"+std::to_string(line)+" C"+std::to_string(column)+": "+(message?message:"unknown error");
    return false;
}
bool PresetEquations::load(const char *path,bool spatial,bool shaders) {
    if(context){error="This diagnostic context loads only once";return false;}
    libprojectM::MilkdropPreset::PresetFileParser parser;
    if(!parser.Read(std::string(path))){error="Cannot read packaged preset file";return false;}
    // Fail explicitly if someone swaps in a shader/per-pixel preset. The native
    // adapter is only a per-frame-equation test; silently ignoring those would
    // misleadingly suggest full compatibility.
    if((!shaders && (!parser.GetCode("warp_").empty() || !parser.GetCode("comp_").empty())) ||
       (!spatial && !parser.GetCode("per_pixel_").empty())) {
        error="Shader/per-pixel presets are not supported by this test";return false;
    }
    defaults[0]=parser.GetFloat("zoom",1.0f);
    defaults[1]=parser.GetFloat("rot",0.0f);
    defaults[2]=parser.GetFloat("fDecay",0.98f);
    defaults[3]=parser.GetFloat("wave_r",1.0f);
    defaults[4]=parser.GetFloat("wave_g",1.0f);
    defaults[5]=parser.GetFloat("wave_b",1.0f);
    defaults[6]=parser.GetFloat("dx",0);defaults[7]=parser.GetFloat("dy",0);
    defaults[8]=parser.GetFloat("cx",.5f);defaults[9]=parser.GetFloat("cy",.5f);
    defaults[16]=parser.GetFloat("fWaveAlpha",1);defaults[13]=parser.GetFloat("warp",0);defaults[14]=parser.GetFloat("sx",1);defaults[15]=parser.GetFloat("sy",1);
    defaults[10]=parser.GetFloat("wave_x",.5f);defaults[11]=parser.GetFloat("wave_y",.5f);defaults[12]=parser.GetFloat("fWaveParam",0);
    sharedMemory=preparePresetMemory(parser,error);if(!sharedMemory)return false;
    context=projectm_eval_context_create(sharedMemory->buffer,&sharedMemory->registers);
    if(!context){error="Equation context allocation failed";return false;}
    const char *names[]={"time","fps","frame","zoom","rot","decay","wave_r","wave_g","wave_b","bass","mid","treb","dx","dy","bass_att","mid_att","treb_att","cx","cy","wave_x","wave_y","wave_mystery"};
    PRJM_EVAL_F **variables[]={&time,&fps,&frame,&zoom,&rot,&decay,&red,&green,&blue,&bass,&mid,&treb,&pushX,&pushY,&bassAtt,&midAtt,&trebAtt,&centerX,&centerY,&waveX,&waveY,&mystery};
    for(unsigned i=0;i<22;++i) {
        *variables[i]=projectm_eval_context_register_variable(context,names[i]);
        if(!*variables[i]){error="Equation variable allocation failed";return false;}
    }
    const char *extraNames[]={"warp","sx","sy","vol","vol_att","wave_a"};
    PRJM_EVAL_F **extras[]={&warpValue,&stretchX,&stretchY,&volume,&volumeAtt,&waveAlpha};
    for(unsigned i=0;i<6;++i){*extras[i]=projectm_eval_context_register_variable(context,extraNames[i]);if(!*extras[i]){error="Warp/audio variable allocation";return false;}}
    for(unsigned i=0;i<32;++i){q[i]=projectm_eval_context_register_variable(context,("q"+std::to_string(i+1)).c_str());if(!q[i]){error="Q variable allocation";return false;}}
    for(unsigned i=0;i<6;++i){engineInputs[i]=projectm_eval_context_register_variable(context,engineInputNames[i]);if(!engineInputs[i]){error="Engine input allocation";return false;}}
    zoomExpDefault=parser.GetFloat("fZoomExponent",1);zoomExp=projectm_eval_context_register_variable(context,"zoomexp");if(!zoomExp){error="Zoom exponent allocation";return false;}
    wrapDefault=parser.GetBool("bTexWrap",true)?1:0;wrapParameter=projectm_eval_context_register_variable(context,"wrap");if(!wrapParameter){error="Wrap variable allocation";return false;}
    const char *layerNames[]={"ob_size","ob_r","ob_g","ob_b","ob_a","ib_size","ib_r","ib_g","ib_b","ib_a","mv_x","mv_y","mv_dx","mv_dy","mv_l","mv_r","mv_g","mv_b","mv_a"};
    const char *layerKeys[]={"ob_size","ob_r","ob_g","ob_b","ob_a","ib_size","ib_r","ib_g","ib_b","ib_a","nMotionVectorsX","nMotionVectorsY","mv_dx","mv_dy","mv_l","mv_r","mv_g","mv_b","mv_a"};
    const double layerInitial[]={.01,0,0,0,0,.01,.25,.25,.25,0,12,9,0,0,.9,1,1,1,parser.GetBool("bMotionVectorsOn",false)?1.:0.};
    for(unsigned i=0;i<19;++i){layerDefaults[i]=parser.GetFloat(layerKeys[i],layerInitial[i]);layerParameters[i]=projectm_eval_context_register_variable(context,layerNames[i]);if(!layerParameters[i]){error="Layer variable allocation";return false;}}
    const char *blurNames[]={"blur1_min","blur1_max","blur2_min","blur2_max","blur3_min","blur3_max","blur1_edge_darken"};
    const char *blurKeys[]={"b1n","b1x","b2n","b2x","b3n","b3x","b1ed"};
    for(unsigned i=0;i<7;++i){blurDefaults[i]=parser.GetFloat(blurKeys[i],i<6?(i%2?1:0):0);blurParameters[i]=projectm_eval_context_register_variable(context,blurNames[i]);if(!blurParameters[i]){error="Blur variable allocation";return false;}}
    const char *finalNames[]={"gamma","echo_zoom","echo_alpha","echo_orient","darken_center"};
    const char *finalKeys[]={"fGammaAdj","fVideoEchoZoom","fVideoEchoAlpha","nVideoEchoOrientation","bDarkenCenter"};
    const double finalInitial[]={1,1,0,0,0};
    for(unsigned i=0;i<5;++i){finalDefaults[i]=parser.GetFloat(finalKeys[i],finalInitial[i]);finalParameters[i]=projectm_eval_context_register_variable(context,finalNames[i]);if(!finalParameters[i]){error="Final stage variable allocation";return false;}}
    *zoom=defaults[0];*rot=defaults[1];*decay=defaults[2];
    *red=defaults[3];*green=defaults[4];*blue=defaults[5];
    *pushX=defaults[6];*pushY=defaults[7];*centerX=defaults[8];*centerY=defaults[9];*waveX=defaults[10];*waveY=defaults[11];*mystery=defaults[12];
    *warpValue=defaults[13];*stretchX=defaults[14];*stretchY=defaults[15];*waveAlpha=defaults[16];
    for(unsigned i=0;i<7;++i)*blurParameters[i]=blurDefaults[i];
    for(unsigned i=0;i<5;++i)*finalParameters[i]=finalDefaults[i];
    loadEngineInputs(engineInputs,false);*zoomExp=zoomExpDefault;
    for(unsigned i=0;i<19;++i)*layerParameters[i]=layerDefaults[i];
    *wrapParameter=wrapDefault;
    *time=0;*fps=60;*frame=0;*bass=*mid=*treb=0;*bassAtt=*midAtt=*trebAtt=0;
    const auto initialCode=parser.GetCode("per_frame_init_");
    if(!initialCode.empty()) {
        initial=projectm_eval_code_compile(context,initialCode.c_str());
        if(!initial)return compilerError("INIT");
        projectm_eval_code_execute(initial);
    }
    const auto frameCode=parser.GetCode("per_frame_");
    perFrame=projectm_eval_code_compile(context,frameCode.empty()?"zoom=zoom;":frameCode.c_str());
    if(!perFrame)return compilerError("PER FRAME");
    return true;
}
bool PresetEquations::evaluate(double seconds,double framesPerSecond,double bassLevel,double midLevel,double trebleLevel,double bassAverage,double midAverage,double trebleAverage) {
    if(!perFrame || !std::isfinite(seconds) || !std::isfinite(framesPerSecond) || framesPerSecond<=0) {
        error="Invalid equation frame input";return false;
    }
    // projectM reloads configured render defaults before each per-frame run.
    // Custom state variables remain in the evaluator's context across frames.
    *zoom=defaults[0];*rot=defaults[1];*decay=defaults[2];
    *red=defaults[3];*green=defaults[4];*blue=defaults[5];
    *pushX=defaults[6];*pushY=defaults[7];*centerX=defaults[8];*centerY=defaults[9];*waveX=defaults[10];*waveY=defaults[11];*mystery=defaults[12];
    *warpValue=defaults[13];*stretchX=defaults[14];*stretchY=defaults[15];*waveAlpha=defaults[16];
    for(unsigned i=0;i<7;++i)*blurParameters[i]=blurDefaults[i];
    for(unsigned i=0;i<5;++i)*finalParameters[i]=finalDefaults[i];
    loadEngineInputs(engineInputs,false);*zoomExp=zoomExpDefault;
    for(unsigned i=0;i<19;++i)*layerParameters[i]=layerDefaults[i];
    *wrapParameter=wrapDefault;
    *time=seconds;*fps=framesPerSecond;*frame=++frames;
    if(!std::isfinite(bassLevel) || !std::isfinite(midLevel) || !std::isfinite(trebleLevel)) {
        error="Non-finite audio input";return false;
    }
    // Preserve projectM relative loudness peaks; output validation remains below.
    *bass=bassLevel<0?0:bassLevel;
    *mid=midLevel<0?0:midLevel;
    *treb=trebleLevel<0?0:trebleLevel;
    const double averages[]={bassAverage<0?*bass:bassAverage,midAverage<0?*mid:midAverage,trebleAverage<0?*treb:trebleAverage};
    PRJM_EVAL_F *destinations[]={bassAtt,midAtt,trebAtt};
    for(unsigned i=0;i<3;++i){if(!std::isfinite(averages[i])){error="Non-finite averaged audio";return false;}*destinations[i]=averages[i]<0?0:averages[i];}
    *volume=(*bass+*mid+*treb)/3;*volumeAtt=(*bassAtt+*midAtt+*trebAtt)/3;
    projectm_eval_code_execute(perFrame);
    const double values[]={*zoom,*rot,*decay,*red,*green,*blue};
    for(double value:values)if(!std::isfinite(value)){error="Non-finite preset output";return false;}
    if(!validPresetScalar(*pushX) || !validPresetScalar(*pushY)){error="Invalid translation";return false;}
    if(!validPresetScalar(*centerX) || !validPresetScalar(*centerY)){error="Invalid warp center";return false;}
    output={values[0],values[1],values[2],values[3],values[4],values[5],*pushX,*pushY,*centerX,*centerY,*waveX,*waveY,*mystery};
    if(!validPresetScalar(*waveX) || !validPresetScalar(*waveY) || !std::isfinite(*mystery)){error="Invalid waveform parameters";return false;}
    if(!std::isfinite(*waveAlpha)){error="Non-finite wave alpha";return false;}output.waveAlpha=*waveAlpha;
    output.textureWrap=*wrapParameter;if(!std::isfinite(output.textureWrap)){error="Non-finite texture wrap";return false;}
    output.zoomExponent=*zoomExp;if(!std::isfinite(*zoomExp) || *zoomExp<=0 || *zoomExp>100){error="Invalid zoom exponent";return false;}
    output.warpValue=*warpValue;output.stretchX=*stretchX;output.stretchY=*stretchY;
    output.seconds=seconds;output.fps=framesPerSecond;output.frame=frames;
    for(unsigned i=0;i<32;++i){output.q[i]=*q[i];if(!std::isfinite(output.q[i])){error="Non-finite Q variable";return false;}}
    for(unsigned i=0;i<6;++i)output.audio[i]=i<3?*(i==0?bass:i==1?mid:treb):*destinations[i-3];
    if(!validPresetScalar(*warpValue) || !validPresetStretch(*stretchX) || !validPresetStretch(*stretchY)){error="Invalid warp/stretch output";return false;}
    for(unsigned i=0;i<3;++i){output.blurMin[i]=*blurParameters[i*2];output.blurMax[i]=*blurParameters[i*2+1];}output.blurEdge=*blurParameters[6];
    BlurConfig checked;if(!checked.set(output.blurMin,output.blurMax,output.blurEdge)){error="Invalid blur output/range/edge";return false;}
    for(unsigned i=0;i<19;++i){const double value=*layerParameters[i];if(!std::isfinite(value) || fabs(value)>100000){error="Invalid border/vector output";return false;}if(i<10)output.border[i]=value;else output.motion[i-10]=value;}
    if(output.border[0]<0 || output.border[5]<0 || output.border[0]+output.border[5]>1){error="Border size limits";return false;}
    // The fixed meshes do not grow with zoom/rotation. Keep a numeric budget,
    // not the old visual smoke-test limits; actual UVs are checked before draw.
    if(!validPresetZoom(output.zoom) || !validPresetScalar(output.rot) || !validPresetScalar(output.decay)) {
        error="Transform numeric budget: zoom="+std::to_string(output.zoom)+" rot="+std::to_string(output.rot)+" decay="+std::to_string(output.decay);return false;
    }
    for(unsigned i=0;i<5;++i)if(!validPresetScalar(*finalParameters[i])){error="Invalid final-stage input";return false;}
    output.gamma=std::max(0.,std::min(8.,*finalParameters[0]));output.echoZoom=std::max(.001,std::min(1000.,*finalParameters[1]));
    output.echoAlpha=*finalParameters[2];output.echoOrient=*finalParameters[3];output.darkenCenter=*finalParameters[4];
    return true;
}

bool runEquationChecks(std::string &error) {
    auto ctx=projectm_eval_context_create(nullptr,nullptr);
    if(!ctx){error="Self-check context allocation";return false;}
    auto x=projectm_eval_context_register_variable(ctx,"x");
    auto count=projectm_eval_context_register_variable(ctx,"count");
    auto result=projectm_eval_context_register_variable(ctx,"result");
    if(!x || !count || !result){projectm_eval_context_destroy(ctx);error="Self-check variable allocation";return false;}
    auto code=projectm_eval_code_compile(ctx,"count=count+1; result=if(above(x,0),sin(x)+sqr(x),cos(x)-abs(x));");
    if(!code){projectm_eval_context_destroy(ctx);error="Self-check compilation";return false;}
    *count=0;bool success=true;
    for(unsigned i=0;i<120;++i) {
        *x=((int)i-60)/17.0;
        projectm_eval_code_execute(code);
        const double expected=*x>0?std::sin(*x)+(*x)*(*x):std::cos(*x)-std::fabs(*x);
        if(!std::isfinite(*result) || std::fabs(*result-expected)>1e-9 || *count!=i+1)success=false;
    }
    projectm_eval_code_destroy(code);
    // Malformed code must return an error rather than a runnable program.
    auto invalid=projectm_eval_code_compile(ctx,"result=sin(;");
    if(invalid){success=false;projectm_eval_code_destroy(invalid);}
    projectm_eval_context_destroy(ctx);
    if(!success)error="Arithmetic, branches, state or invalid-code check failed";
    return success;
}
