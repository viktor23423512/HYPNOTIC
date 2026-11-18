#pragma once
#include <set>
#include <cctype>
#include <vector>
#include "equation_budget.hpp"
// Explicit import profile: reject missing renderer semantics rather than
// silently ignoring effects. Existing audited shader pipelines are unchanged.
static bool checkImportProfile(const char *path,std::string &reason){
    libprojectM::MilkdropPreset::PresetFileParser parser;
    if(!parser.Read(std::string(path))){reason="File unreadable, empty or larger than 1 MiB";return false;}
    const auto warpCode=parser.GetCode("warp_"),compCode=parser.GetCode("comp_");
    std::vector<std::string> blocks={"per_frame_init_","per_frame_","per_pixel_"};
    for(unsigned i=0;i<4;++i)if(parser.GetBool("shapecode_"+std::to_string(i)+"_enabled",false)){
        blocks.push_back("shape_"+std::to_string(i)+"_init");blocks.push_back("shape_"+std::to_string(i)+"_per_frame");
    }
    size_t totalPointBytes=0;
    for(unsigned i=0;i<4;++i)if(parser.GetBool("wavecode_"+std::to_string(i)+"_enabled",false)){
        for(const char *part:{"_init","_per_frame","_per_point"})blocks.push_back("wave_"+std::to_string(i)+part);
        std::string active;if(!equationBudgetText(parser.GetCode("wave_"+std::to_string(i)+"_per_point"),active)){reason="Unclosed equation comment";return false;}
        totalPointBytes+=active.size();if(active.size()>6144 || totalPointBytes>8192){reason="Custom point execution budget (6 KiB per layer, 8 KiB total)";return false;}
    }
    const std::set<std::string> spatial={"x","y","rad","ang","time","zoom","rot","dx","dy","warp","cx","cy","sx","sy","bass","mid","treb","bass_att","mid_att","treb_att","fps","frame","vol","vol_att","zoomexp","meshx","meshy","pixelsx","pixelsy","aspectx","aspecty"};
    const std::set<std::string> unavailable={"decay","wave_mode","wave_scale","wave_smoothing","wave_dots","wave_thick","wave_additive","wave_usedots","wave_brighten","wave_mod_alpha","wave_mod_alpha_start","wave_mod_alpha_end","progress"};
    for(unsigned b=0;b<blocks.size();++b){
        std::string code;if(!equationBudgetText(parser.GetCode(blocks[b]),code)){reason="Unclosed equation comment";return false;}
        if(code.size()>8192){reason="Equation block exceeds 8 KiB import limit";return false;}
        unsigned nesting=0;std::set<std::string> identifiers;
        for(size_t at=0;at<code.size();){
            if(code[at]=='('){if(++nesting>32){reason="Equation nesting exceeds import limit";return false;}}
            if(code[at]==')' && nesting)--nesting;
            if(!std::isalpha((unsigned char)code[at]) && code[at]!='_'){++at;continue;}
            const size_t begin=at++;
            while(at<code.size() && (std::isalnum((unsigned char)code[at]) || code[at]=='_'))++at;
            std::string token=code.substr(begin,at-begin);
            for(char &c:token)c=(char)std::tolower((unsigned char)c);
            identifiers.insert(token);if(identifiers.size()>128){reason="Equation identifier limit (128)";return false;}
            if(token.size()==5 && token.substr(0,3)=="reg" && std::isdigit((unsigned char)token[3]) && std::isdigit((unsigned char)token[4])){reason="Shared global registers are not isolated for imports yet: "+token;return false;}
            size_t next=at;while(next<code.size() && std::isspace((unsigned char)code[next]))++next;
            if(next<code.size() && code[next]=='('){
                if(token=="while" || token=="loop" || token=="megabuf" || token=="memcpy" || token=="memset" || token=="freembuf" || token=="exec2" || token=="exec3" || token=="assign" || token=="_set"){
                    reason="Equation function not enabled for imports: "+token;return false;
                }
                continue;
            }
            if(b>=3 && (token=="progress")){reason="Shape input not connected yet: "+token;return false;}
            if(b==2 && (token=="gamma" || token=="echo_zoom" || token=="echo_alpha" || token=="echo_orient" || token=="darken_center")){reason="Per-pixel final-stage variable not connected: "+token;return false;}
            if(b==2 && !spatial.count(token)){
                // Unknown user variables are local to this evaluator, exactly
                // as projectM's per-pixel context. Reserved engine/global names
                // still fail; the bounded identifier set limits new storage.
                bool numberedQ=token.size()>1 && token[0]=='q';for(size_t j=1;j<token.size() && numberedQ;++j)numberedQ=std::isdigit((unsigned char)token[j]);
                if((numberedQ && (atoi(token.c_str()+1)<1 || atoi(token.c_str()+1)>32)) || token.compare(0,3,"reg")==0 || unavailable.count(token)){
                    reason="Per-pixel variable not connected yet: "+token;return false;
                }
            }
            if((b<2 || token=="progress") && unavailable.count(token)){
                // decay is wired as a render output. All other names above are not.
                if(token=="decay")continue;
                reason="Engine variable not connected yet: "+token;return false;
            }
        }
    }
    return true;
}
