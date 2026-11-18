#pragma once
#include <string>
// Count executable text, not comments, within the existing 1 MiB file limit.
inline bool equationBudgetText(const std::string &source,std::string &out){
 out.clear();bool line=false,block=false;
 for(size_t i=0;i<source.size();++i){char c=source[i],next=i+1<source.size()?source[i+1]:0;
  if(line){if(c=='\n'){line=false;out+='\n';}continue;}
  if(block){if(c=='*' && next=='/'){block=false;++i;out+=' ';}else if(c=='\n')out+='\n';continue;}
  if(c=='/' && next=='/'){line=true;++i;out+=' ';continue;}if(c=='/' && next=='*'){block=true;++i;out+=' ';continue;}out+=c;
 }return !block;
}
