#include "PresetFileParser.hpp"
#include "milkdrop_file_io.h"
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
bool readMilkdropFile(const char *path,std::vector<char> &bytes,size_t limit,std::string &error){std::ifstream f(path,std::ios::binary);if(!f)return false;f.seekg(0,std::ios::end);auto n=f.tellg();if(n<0||(size_t)n>limit)return false;f.seekg(0);bytes.assign(std::istreambuf_iterator<char>(f),{});return !f.bad();}
int main(int argc,char**argv){assert(argc==2);std::ifstream list(argv[1]);std::string path;unsigned count=0,values=0;
 while(std::getline(list,path)){std::ifstream f(path,std::ios::binary);libprojectM::MilkdropPreset::PresetFileParser a,b;assert(a.Read(path));assert(b.Read(f));assert(a.PresetValues()==b.PresetValues());for(const char *code:{"per_frame_","per_frame_init_","per_pixel_","warp_","comp_"})assert(a.GetCode(code)==b.GetCode(code));values+=a.PresetValues().size();++count;}
 printf("PASS %u original presets / %u exact parser values; equations and shader blocks unchanged through file adapter\n",count,values);
}
