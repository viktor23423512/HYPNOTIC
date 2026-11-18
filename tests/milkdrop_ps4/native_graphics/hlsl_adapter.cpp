#include "hlsl_adapter.hpp"
#include "HLSLParser.h"
#include "GLSLGenerator.h"
#include <cctype>

static bool replaceOnce(std::string &text,const char *old,const char *replacement){
    const auto pos=text.find(old);if(pos==std::string::npos || text.find(old,pos+1)!=std::string::npos)return false;
    text.replace(pos,std::char_traits<char>::length(old),replacement);return true;
}
static bool stripComments(const std::string &source,std::string &text){
    bool line=false,block=false;
    for(size_t i=0;i<source.size();++i){
        const char c=source[i],next=i+1<source.size()?source[i+1]:0;
        if(line){if(c=='\n'){line=false;text+='\n';}continue;}
        if(block){if(c=='*' && next=='/'){block=false;++i;text+=' ';}else if(c=='\n')text+='\n';continue;}
        if(c=='/' && next=='/'){line=true;++i;continue;}
        if(c=='/' && next=='*'){block=true;++i;continue;}
        text+=c;
    }
    return !block;
}
static bool translateShader(const std::string &body,std::string &glsl,std::string &error,bool warp,bool textures=false){
    glsl.clear();error.clear();std::string clean;
    if(body.size()>16384 || !stripComments(body,clean)){error="Composite source limit or comment";return false;}
    if(textures){const auto at=clean.find("sampler sampler_rand00;");if(at!=std::string::npos)clean.erase(at,std::string("sampler sampler_rand00;").size());}
    const auto entry=clean.find("shader_body"),opening=clean.find('{',entry),closing=clean.rfind('}');
    if(entry==std::string::npos || opening==std::string::npos || closing==std::string::npos || closing<=opening ||
       clean.find("shader_body",entry+1)!=std::string::npos){error="Composite entry/braces";return false;}
    // This stage deliberately accepts just a shader_body. Extra global helpers,
    // textures and uniforms require explicit engine bindings in a later stage.
    for(size_t i=0;i<entry;++i)if(!std::isspace((unsigned char)clean[i])){error="Composite global declarations unsupported";return false;}
    for(size_t i=entry+11;i<opening;++i)if(!std::isspace((unsigned char)clean[i])){error="Composite entry syntax";return false;}
    for(size_t i=closing+1;i<clean.size();++i)if(!std::isspace((unsigned char)clean[i])){error="Composite trailing declarations";return false;}
    size_t last=closing;
    while(last>opening+1 && std::isspace((unsigned char)clean[last-1]))--last;
    if(last>opening+1 && clean[last-1]!=';' && clean[last-1]!='}'){
        error="Composite statement lacks terminator";return false;
    }
    int braces=0;
    for(size_t i=opening;i<=closing;++i){if(clean[i]=='{')++braces;else if(clean[i]=='}')--braces;
        if(braces<0 || (braces==0 && i!=closing)){error="Composite brace structure";return false;}}
    if(braces!=0){error="Composite unbalanced braces";return false;}
    const std::string prefixBase=warp?"sampler2D sampler_main;\nuniform float4 texsize; uniform float4 audio0;\n#define treb audio0.z\nvoid PS(float4 nativeUV : TEXCOORD0,out float4 result : COLOR0){\nfloat2 uv=nativeUV.xy;float2 uv_orig=nativeUV.zw;\nfloat3 ret=0;\n":"sampler2D sampler_main;\nvoid PS(float2 uv : TEXCOORD0,out float4 result : COLOR0){\nfloat3 ret=0;\n";
    std::string prefix=prefixBase;
    if(textures && !warp)prefix="uniform float4 texsize;uniform float4 audio0;\n"+prefix;
    if(textures){prefix="sampler2D sampler_noise_lq;sampler2D sampler_rand00;uniform float4 rand_frame;uniform float4 texsize_noise_lq;\n"+prefix;}
    const std::string source=prefix+
        clean.substr(opening+1,closing-opening-1)+"\nresult=float4(ret.xyz,1.0);\n}\n";
    M4::Allocator allocator;M4::HLSLTree tree(&allocator);M4::HLSLParser parser(&allocator,&tree);std::string processed;
    if(!parser.ApplyPreprocessor("composite",source.c_str(),source.size(),processed) || !parser.Parse("composite",processed.c_str(),processed.size())){error="projectM HLSL parse/preprocess failed";return false;}
    M4::GLSLGenerator generator;
    if(!generator.Generate(&tree,M4::GLSLGenerator::Target_FragmentShader,M4::GLSLGenerator::Version_330,"PS",M4::GLSLGenerator::Options(M4::GLSLGenerator::Flag_AlternateNanPropagation))){error="projectM GLSL generation failed";return false;}
    glsl=generator.GetResult();
    if(!replaceOnce(glsl,"#version 330","#version 450") ||
       !replaceOnce(glsl,warp?"in vec4 frag_TEXCOORD0;":"in vec2 frag_TEXCOORD0;",warp?"layout(location=0) in vec4 frag_TEXCOORD0;":"layout(location=0) in vec2 frag_TEXCOORD0;") ||
       !replaceOnce(glsl,"out vec4 rast_FragData[1];","layout(location=0) out vec4 rast_FragData[1];") ||
       !replaceOnce(glsl,"sampler2D sampler_main;","layout(set=0,binding=0) uniform sampler2D sampler_main;")){
        glsl.clear();error="Unexpected translator interface; native binding rejected";return false;
    }
    if(textures){
        if(!replaceOnce(glsl,"sampler2D sampler_noise_lq;","layout(set=0,binding=2) uniform sampler2D sampler_noise_lq;") || !replaceOnce(glsl,"sampler2D sampler_rand00;","layout(set=0,binding=3) uniform sampler2D sampler_rand00;") ||
           !replaceOnce(glsl,"uniform vec4 texsize;","layout(set=0,binding=1,std140) uniform TextureInputs { vec4 texsize; vec4 audio0; vec4 rand_frame; vec4 texsize_noise_lq; };") || !replaceOnce(glsl,"uniform vec4 audio0;","") || !replaceOnce(glsl,"uniform vec4 rand_frame;","") || !replaceOnce(glsl,"uniform vec4 texsize_noise_lq;","")){glsl.clear();error="Unexpected texture bindings";return false;}
    }else if(warp && (!replaceOnce(glsl,"uniform vec4 texsize;","layout(set=0,binding=1,std140) uniform WarpInputs { vec4 texsize; vec4 audio0; };") || !replaceOnce(glsl,"uniform vec4 audio0;",""))){glsl.clear();error="Unexpected warp uniforms";return false;}
    return true;
}
bool translateComposite(const std::string &body,std::string &glsl,std::string &error){return translateShader(body,glsl,error,false);}
bool translateWarp(const std::string &body,std::string &glsl,std::string &error){return translateShader(body,glsl,error,true);}


bool translateTextured(const std::string &body,std::string &glsl,std::string &error,bool warp){return translateShader(body,glsl,error,warp,true);}
