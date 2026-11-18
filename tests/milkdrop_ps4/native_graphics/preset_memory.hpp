#pragma once
#include <projectm-eval.h>
#include <PresetFileParser.hpp>
#include <memory>
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "equation_budget.hpp"
// Same per-preset global-memory ownership as projectM 4.1.8 PresetState.
// Its pinned evaluator has 128 pages of 65536 doubles (64 MiB maximum).
// Reserve all pages at load only for presets that use gmegabuf, so executing
// an equation never allocates a global-memory page halfway through a frame.
struct PresetMemory {
 projectm_eval_mem_buffer buffer=nullptr;PRJM_EVAL_F registers[100]={};bool populated=false,prepared=false;
 PresetMemory()=default;PresetMemory(const PresetMemory&)=delete;PresetMemory& operator=(const PresetMemory&)=delete;
 ~PresetMemory(){if(buffer)projectm_eval_memory_buffer_destroy(buffer);}
 bool prepare(libprojectM::MilkdropPreset::PresetFileParser &parser,std::string &error){
  if(buffer)return prepared;
  buffer=projectm_eval_memory_buffer_create();if(!buffer){error="Preset global-memory table allocation";return false;}
  std::vector<std::string> blocks={"per_frame_init_","per_frame_","per_pixel_"};
  for(unsigned i=0;i<4;++i){
   if(parser.GetBool("shapecode_"+std::to_string(i)+"_enabled",false))for(const char *part:{"_init","_per_frame"})blocks.push_back("shape_"+std::to_string(i)+part);
   if(parser.GetBool("wavecode_"+std::to_string(i)+"_enabled",false))for(const char *part:{"_init","_per_frame","_per_point"})blocks.push_back("wave_"+std::to_string(i)+part);
  }
  bool needed=false;
  for(const auto &block:blocks){std::string code;if(!equationBudgetText(parser.GetCode(block),code)){error="Unclosed equation comment";return false;}
   for(size_t i=0;i<code.size();){if(!std::isalpha((unsigned char)code[i]) && code[i]!='_'){++i;continue;}size_t a=i++;while(i<code.size() && (std::isalnum((unsigned char)code[i]) || code[i]=='_'))++i;auto token=code.substr(a,i-a);for(char &c:token)c=(char)std::tolower((unsigned char)c);needed|=token=="gmegabuf";}
  }
  if(needed){static_assert(sizeof(PRJM_EVAL_F)==8,"Pinned double evaluator required");for(unsigned i=0;i<128;++i){buffer[i]=(PRJM_EVAL_F*)calloc(65536,sizeof(PRJM_EVAL_F));if(!buffer[i]){error="Preset gmegabuf allocation (64 MiB limit)";return false;}}populated=true;}
  prepared=true;return true;
 }
 void clear(){memset(registers,0,sizeof(registers));if(populated)for(unsigned i=0;i<128;++i)memset(buffer[i],0,65536*sizeof(PRJM_EVAL_F));}
};
inline std::shared_ptr<PresetMemory> preparePresetMemory(libprojectM::MilkdropPreset::PresetFileParser &parser,std::string &error){auto memory=std::make_shared<PresetMemory>();if(!memory->prepare(parser,error))return {};return memory;}
