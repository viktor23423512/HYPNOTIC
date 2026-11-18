#include "common_shader.hpp"
#include "shader_preprocess.hpp"
#include "bounded_shader_loops.hpp"
#include "HLSLParser.h"
#include "GLSLGenerator.h"
#include <cctype>
#include <cstring>
static bool replaceAll(std::string &s,const std::string &a,const std::string &b){
    size_t pos=0;bool found=false;while((pos=s.find(a,pos))!=std::string::npos){s.replace(pos,a.size(),b);pos+=b.size();found=true;}return found;
}
bool translateCommonShader(const std::string &source,bool warp,std::string &glsl,std::string &error,bool *pointNoise){
    glsl.clear();error.clear();if(pointNoise)*pointNoise=false;
    if(source.size()>32768){error="Shader exceeds 32 KiB profile limit";return false;}
    std::string clean;bool line=false,block=false;
    for(size_t i=0;i<source.size();++i){char c=source[i],next=i+1<source.size()?source[i+1]:0;
        if(line){if(c=='\n'){clean+='\n';line=false;}continue;}
        if(block){if(c=='*' && next=='/'){block=false;++i;clean+=' ';}else if(c=='\n')clean+='\n';continue;}
        if(c=='/' && next=='/'){line=true;++i;continue;}if(c=='/' && next=='*'){block=true;++i;continue;}clean+=c;
    }
    if(block){error="Unclosed shader comment";return false;}
    std::string expanded;if(!preprocessPresetShader(clean,expanded,error))return false;clean.swap(expanded);
    if(!BoundedShaderLoops::validate(clean,error))return false;
    for(size_t pos=0;pos<clean.size();){
        if(clean[pos]=='#'){error="Shader preprocessor directives are not enabled in this profile";return false;}
        if(!std::isalpha((unsigned char)clean[pos]) && clean[pos]!='_'){++pos;continue;}
        size_t start=pos++;while(pos<clean.size() && (std::isalnum((unsigned char)clean[pos]) || clean[pos]=='_'))++pos;
        const auto token=clean.substr(start,pos-start);
        if(token=="progress" || token.compare(0,4,"rot_")==0){error="Shader feature not enabled yet: "+token;return false;}
    }
    // Common case-insensitive texture intrinsic spelling found in MilkDrop files.
    for(const auto &alias:{std::pair<const char*,const char*>("tex2d","tex2D"),{"tex3d","tex3D"},{"tex2dlod","tex2Dlod"},{"tex3dlod","tex3Dlod"}}){
        for(size_t p=0;(p=clean.find(alias.first,p))!=std::string::npos;){size_t end=p+strlen(alias.first);
            if((p==0 || (!std::isalnum((unsigned char)clean[p-1]) && clean[p-1]!='_')) && (end==clean.size() || (!std::isalnum((unsigned char)clean[end]) && clean[end]!='_')))clean.replace(p,strlen(alias.first),alias.second);p=end;}
    }
    replaceAll(clean,"sampler_fw_noisevol_hq","sampler_noisevol_hq");
    replaceAll(clean,"sampler_wp_noise_lq","sampler_pw_noise_lq");
    // projectM TextureManager accepts both orders of the filter/wrap prefix.
    const char *reverse[]={"sampler_cf_main","sampler_wf_main","sampler_cp_main","sampler_wp_main"};
    const char *canonical[]={"sampler_fc_main","sampler_fw_main","sampler_pc_main","sampler_pw_main"};
    for(unsigned i=0;i<4;++i)replaceAll(clean,reverse[i],canonical[i]);
    // All samplers are engine-bound. Unknown named images fail visibly.
    size_t sampler=0;
    while((sampler=clean.find("sampler ",sampler))!=std::string::npos){
        const size_t end=clean.find(';',sampler);if(end==std::string::npos){error="Sampler declaration lacks terminator";return false;}
        std::string name=clean.substr(sampler+8,end-sampler-8);while(!name.empty() && std::isspace((unsigned char)name.back()))name.pop_back();
        while(!name.empty() && std::isspace((unsigned char)name.front()))name.erase(0,1);
        if(name.empty() || name.size()>80 || (!std::isalpha((unsigned char)name[0]) && name[0]!='_')){error="Unsupported sampler declaration";return false;}
        for(char c:name)if(!std::isalnum((unsigned char)c) && c!='_'){error="Unsupported sampler declaration";return false;}
        const char *allowed[]={"sampler_main","sampler_blur1","sampler_blur2","sampler_blur3","sampler_noise_lq","sampler_rand00","sampler_fc_main","sampler_fw_main","sampler_pc_main","sampler_pw_main","sampler_noise_mq","sampler_noise_hq","sampler_noisevol_lq","sampler_noisevol_hq","sampler_noise_lq_lite","sampler_pw_noise_lq"};bool ok=false;
        for(auto item:allowed)if(name==item)ok=true;
        if(!ok){
            std::string rest=clean;rest.erase(sampler,end-sampler+1);bool used=false;
            for(size_t at=0;at<rest.size();){if(!std::isalpha((unsigned char)rest[at]) && rest[at]!='_'){++at;continue;}size_t begin=at++;while(at<rest.size() && (std::isalnum((unsigned char)rest[at]) || rest[at]=='_'))++at;if(rest.substr(begin,at-begin)==name)used=true;}
            // Drop an unused declaration, never substitute an image in use.
            if(used){error="Texture sampler not bound yet: "+name;return false;}
        }clean.erase(sampler,end-sampler+1);
    }
    auto used=[&](const char *name){for(size_t p=0;p<clean.size();){if(!std::isalpha((unsigned char)clean[p]) && clean[p]!='_'){++p;continue;}size_t a=p++;while(p<clean.size() && (std::isalnum((unsigned char)clean[p]) || clean[p]=='_'))++p;if(clean.substr(a,p-a)==name)return true;}return false;};
    const bool point=used("sampler_pw_noise_lq");
    if(point && used("sampler_rand00")){error="Texture binding budget: point noise and file image share slot 6";return false;}
    if(pointNoise)*pointNoise=point;
    const auto entry=clean.find("shader_body");if(entry==std::string::npos || clean.find("shader_body",entry+11)!=std::string::npos){error="Expected one shader_body entry";return false;}
    size_t opening=entry+11;while(opening<clean.size() && std::isspace((unsigned char)clean[opening]))++opening;
    if(opening>=clean.size() || clean[opening]!='{'){error="Invalid shader_body declaration";return false;}
    unsigned depth=1;size_t end=opening+1;
    for(;end<clean.size() && depth;++end){if(clean[end]=='{')++depth;else if(clean[end]=='}')--depth;if(depth>32){error="Shader brace depth limit";return false;}}
    if(depth){error="Shader braces unbalanced";return false;}
    std::string prefix=
        "sampler2D sampler_main;sampler2D sampler_blur1;sampler2D sampler_blur2;sampler2D sampler_blur3;sampler2D sampler_noise_lq;sampler2D sampler_rand00;sampler2D sampler_fc_main;sampler2D sampler_fw_main;sampler2D sampler_pc_main;sampler2D sampler_pw_main;\n"
        "sampler2D sampler_noise_mq;sampler2D sampler_noise_hq;sampler3D sampler_noisevol_lq;sampler3D sampler_noisevol_hq;sampler2D sampler_noise_lq_lite;sampler2D sampler_pw_noise_lq;\n"
        "uniform float4 md_texsize;uniform float4 md_audio;uniform float4 md_audio_att;uniform float4 md_clock;uniform float4 md_aspect;uniform float4 md_rand_frame;uniform float4 md_rand_preset;\n"
        "uniform float4 md_q0;uniform float4 md_q1;uniform float4 md_q2;uniform float4 md_q3;uniform float4 md_q4;uniform float4 md_q5;uniform float4 md_q6;uniform float4 md_q7;\n"
        "uniform float4 md_hue0;uniform float4 md_hue1;uniform float4 md_hue2;uniform float4 md_hue3;\n"
        "uniform float4 md_roam_cos;uniform float4 md_roam_sin;uniform float4 md_slow_cos;uniform float4 md_slow_sin;uniform float4 md_noise_size;uniform float4 md_image_size;uniform float4 md_blur12;uniform float4 md_blur3;\n"
        "#define texsize_main md_texsize\n#define texsize md_texsize\n#define aspect md_aspect\n#define time md_clock.x\n#define fps md_clock.y\n#define frame md_clock.z\n#define progress md_clock.w\n"
        "#define bass md_audio.x\n#define mid md_audio.y\n#define treb md_audio.z\n#define vol md_audio.w\n"
        "#define bass_att md_audio_att.x\n#define mid_att md_audio_att.y\n#define treb_att md_audio_att.z\n#define vol_att md_audio_att.w\n"
        "#define rand_frame md_rand_frame\n#define rand_preset md_rand_preset\n#define roam_cos md_roam_cos\n#define roam_sin md_roam_sin\n#define slow_roam_cos md_slow_cos\n#define slow_roam_sin md_slow_sin\n"
        "#define texsize_noise_lq md_noise_size\n#define texsize_rand00 md_image_size\n#define M_PI 3.14159265358979323846\n#define M_PI_2 1.57079632679489661923\n#define M_PI_4 .78539816339744830962\n#define M_INV_PI .31830988618379067154\n#define M_INV_PI_2 .15915494309189533577\n"
        "float3 GetPixel(float2 p){return tex2D(sampler_main,p).xyz;}\n"
        "float3 GetBlur1(float2 p){return tex2D(sampler_blur1,p).xyz*md_blur12.x+md_blur12.y;}\n"
        "float3 GetBlur2(float2 p){return tex2D(sampler_blur2,p).xyz*md_blur12.z+md_blur12.w;}\n"
        "float3 GetBlur3(float2 p){return tex2D(sampler_blur3,p).xyz*md_blur3.x+md_blur3.y;}\n"
        "#define blur1_min md_blur12.y\n#define blur1_max (md_blur12.x+md_blur12.y)\n#define blur2_min md_blur12.w\n#define blur2_max (md_blur12.z+md_blur12.w)\n#define blur3_min md_blur3.y\n#define blur3_max (md_blur3.x+md_blur3.y)\n"
        "float lum(float3 p){return dot(p,float3(.32,.49,.29));}\n";
    prefix+="static const float4 md_noise256_size=float4(256,256,.00390625,.00390625);static const float4 md_noise32_size=float4(32,32,.03125,.03125);\n";
    prefix+="#define texsize_noise_mq md_noise256_size\n#define texsize_noise_hq md_noise256_size\n#define texsize_noise_lq_lite md_noise32_size\n#define texsize_noisevol_lq md_noise32_size\n#define texsize_noisevol_hq md_noise32_size\n";
    for(unsigned i=0;i<8;++i)prefix+="#define _q"+std::string(1,'a'+i)+" md_q"+std::to_string(i)+"\n";
    const char *swizzle="xyzw";
    for(unsigned i=0;i<32;++i)prefix+="#define q"+std::to_string(i+1)+" md_q"+std::to_string(i/4)+"."+swizzle[i%4]+"\n";
    prefix+="float3 presetHue(float2 p){return md_hue0.xyz*p.x*p.y+md_hue1.xyz*(1-p.x)*p.y+md_hue2.xyz*p.x*(1-p.y)+md_hue3.xyz*(1-p.x)*(1-p.y); }\n";
    const std::string hue=warp?"float3 hue_shader=float3(1,1,1);\n":"float3 hue_shader=presetHue(float2(uv_orig.x,1-uv_orig.y));\n";
    const std::string body="void PS(float4 nativeUV : TEXCOORD0,out float4 result : COLOR0){\nfloat2 uv=nativeUV.xy;float2 uv_orig=nativeUV.zw;float2 p=(uv_orig-.5)*aspect.xy;float rad=length(p)*2;float ang=atan2(-p.y,p.x);float3 ret=0;\n"+hue+clean.substr(opening+1,end-opening-2)+"\nresult=float4(ret,1);}\n";
    const auto text=prefix+clean.substr(0,entry)+"\n"+body+clean.substr(end);
    M4::Allocator allocator;M4::HLSLTree tree(&allocator);M4::HLSLParser parser(&allocator,&tree);std::string processed;
    if(!parser.ApplyPreprocessor("imported",text.c_str(),text.size(),processed) || !parser.Parse("imported",processed.c_str(),processed.size())){glsl=processed;error="MilkDrop HLSL preprocessing/parsing failed";return false;}
    M4::GLSLGenerator generator;
    if(!generator.Generate(&tree,M4::GLSLGenerator::Target_FragmentShader,M4::GLSLGenerator::Version_330,"PS",M4::GLSLGenerator::Options(M4::GLSLGenerator::Flag_AlternateNanPropagation))){error="HLSL to GLSL translation failed";return false;}
    glsl=generator.GetResult();
    replaceAll(glsl,"#version 330","#version 450");replaceAll(glsl,"in vec4 frag_TEXCOORD0;","layout(location=0) in vec4 frag_TEXCOORD0;");replaceAll(glsl,"out vec4 rast_FragData[1];","layout(location=0) out vec4 rast_FragData[1];");
    const char *textures[]={"sampler_main","sampler_blur1","sampler_blur2","sampler_blur3","sampler_noise_lq","sampler_rand00","sampler_fc_main","sampler_fw_main","sampler_pc_main","sampler_pw_main","sampler_noise_mq","sampler_noise_hq","sampler_noisevol_lq","sampler_noisevol_hq","sampler_noise_lq_lite","sampler_pw_noise_lq"};
    const unsigned bindings[]={0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,6};
    for(unsigned i=0;i<16;++i)if(!replaceAll(glsl,std::string((i==12 || i==13)?"sampler3D ":"sampler2D ")+textures[i]+";","layout(set=0,binding="+std::to_string(bindings[i])+") uniform "+((i==12 || i==13)?"sampler3D ":"sampler2D ")+textures[i]+";")){error="Missing generated sampler binding";glsl.clear();return false;}
    const char *fields[]={"md_texsize","md_audio","md_audio_att","md_clock","md_aspect","md_rand_frame","md_rand_preset","md_q0","md_q1","md_q2","md_q3","md_q4","md_q5","md_q6","md_q7","md_roam_cos","md_roam_sin","md_slow_cos","md_slow_sin","md_noise_size","md_image_size","md_blur12","md_blur3","md_hue0","md_hue1","md_hue2","md_hue3"};
    std::string uniformBlock="layout(set=0,binding=1,std140) uniform MilkDropInputs {\n";
    for(auto field:fields)uniformBlock+=std::string("vec4 ")+field+";\n";uniformBlock+="};\n";
    for(unsigned i=0;i<27;++i)if(!replaceAll(glsl,std::string("uniform vec4 ")+fields[i]+";",i==0?uniformBlock:"")){error=std::string("Unexpected uniform interface: ")+fields[i];return false;}
    (void)warp;return true;
}
