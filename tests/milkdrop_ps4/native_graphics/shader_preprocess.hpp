#pragma once
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <cctype>
// No includes, filesystem reads, function-like macros or unbounded expansion.
inline bool preprocessPresetShader(const std::string &source,std::string &out,std::string &error){
 std::map<std::string,std::string> macros;struct Branch{bool parent,condition,seenElse;};Branch stack[16];unsigned depth=0;bool active=true;
 auto trim=[](std::string s){size_t a=0,b=s.size();while(a<b && std::isspace((unsigned char)s[a]))++a;while(b>a && std::isspace((unsigned char)s[b-1]))--b;return s.substr(a,b-a);};
 auto expand=[&](const std::string &input,std::string &result){
  result=input;std::set<std::string> seen;
  for(unsigned step=0;step<32;++step){if(!seen.insert(result).second){error="Recursive shader macro";return false;}std::string next;bool changed=false;
   for(size_t i=0;i<result.size();){if(std::isalpha((unsigned char)result[i]) || result[i]=='_'){size_t start=i++;while(i<result.size() && (std::isalnum((unsigned char)result[i]) || result[i]=='_'))++i;auto name=result.substr(start,i-start);auto m=macros.find(name);if(m!=macros.end()){next+=m->second;changed=true;}else next+=name;}else next+=result[i++];if(next.size()>65536){error="Shader macro expansion exceeds 64 KiB";return false;}}
   result=next;if(!changed)return true;
  }error="Shader macro expansion depth exceeds 32";return false;
 };
 out.clear();std::istringstream lines(source);std::string line;
 while(std::getline(lines,line)){
  auto text=trim(line);
  if(!text.empty() && text[0]=='#'){
   text=trim(text.substr(1));size_t at=0;while(at<text.size() && std::isalpha((unsigned char)text[at]))++at;auto directive=text.substr(0,at),arg=trim(text.substr(at));
   if(directive=="if"){
    if(depth==16){error="Shader conditional nesting exceeds 16";return false;}
    std::string condition;if(active && !expand(arg,condition))return false;condition=trim(condition);
    if(active && condition!="0" && condition!="1"){error="Shader #if supports only bounded 0/1 conditions";return false;}
    stack[depth++]={active,condition=="1",false};active=active && condition=="1";
   }else if(directive=="else"){
    if(!depth || stack[depth-1].seenElse || !arg.empty()){error="Invalid shader #else";return false;}auto &b=stack[depth-1];b.seenElse=true;active=b.parent && !b.condition;
   }else if(directive=="endif"){
    if(!depth || !arg.empty()){error="Invalid shader #endif";return false;}active=stack[--depth].parent;
   }else if(directive=="define"){
    if(!active)continue;size_t end=0;if(arg.empty() || (!std::isalpha((unsigned char)arg[0]) && arg[0]!='_')){error="Invalid shader macro name";return false;}
    while(end<arg.size() && (std::isalnum((unsigned char)arg[end]) || arg[end]=='_'))++end;
    if(end<arg.size() && arg[end]=='('){error="Function-like shader macros not supported yet";return false;}
    auto name=arg.substr(0,end),value=trim(arg.substr(end));if(name.size()>64 || value.size()>1024 || macros.size()>=64){error="Shader macro storage limit";return false;}
    macros[name]=value;
   }else {error="Shader directive not supported: #"+directive;return false;}
   out+='\n';
  }else if(active){std::string expanded;if(!expand(line,expanded))return false;if(out.size()+expanded.size()+1>65536){error="Expanded shader exceeds 64 KiB";return false;}out+=expanded+'\n';}else out+='\n';
 }
 if(depth){error="Unclosed shader conditional";return false;}return true;
}
