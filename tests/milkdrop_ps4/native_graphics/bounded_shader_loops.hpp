#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
// A deliberately narrow proof, not an execution-time clamp: accepted loops
// retain their original source and can execute no more than 32 iterations.
namespace BoundedShaderLoops {
struct Token {std::string s;unsigned scope=0;};
inline bool integer(const std::string &s,int &v){if(s.empty() || (s.size()>1 && s[0]=='0'))return false;char *end=nullptr;long x=strtol(s.c_str(),&end,10);if(*end || x<0 || x>32)return false;v=(int)x;return true;}
inline bool write(const std::vector<Token>&t,size_t i){
 const auto next=i+1<t.size()?t[i+1].s:"";const auto prev=i?t[i-1].s:"";
 return next=="=" || next=="+=" || next=="-=" || next=="*=" || next=="/=" || next=="%=" || next=="&=" || next=="|=" || next=="^=" || next=="<<=" || next==">>=" || next=="++" || next=="--" || prev=="++" || prev=="--";
}
inline bool identifier(const std::string&s){return !s.empty() && (std::isalpha((unsigned char)s[0]) || s[0]=='_');}
inline bool type(const std::string&s){return s=="int" || s=="float" || s=="float1";}
inline bool callsAllowed(const std::vector<Token>&t,size_t a,size_t b){
 const std::string allowed="|if|float|float1|float2|float3|float4|int|int2|int3|int4|bool|abs|acos|asin|atan|atan2|ceil|clamp|cos|cosh|cross|degrees|distance|dot|exp|exp2|floor|fmod|frac|length|lerp|log|log2|log10|max|min|mul|normalize|pow|radians|reflect|refract|round|rsqrt|saturate|sign|sin|sinh|smoothstep|sqrt|step|tan|tanh|trunc|tex2D|tex2d|tex2Dlod|tex2dlod|tex3D|tex3d|tex3Dlod|tex3dlod|GetPixel|GetBlur1|GetBlur2|GetBlur3|lum|";
 for(size_t i=a;i+1<b;++i)if(t[i+1].s=="(" && (std::isalpha((unsigned char)t[i].s[0]) || t[i].s[0]=='_'))
  if(allowed.find("|"+t[i].s+"|")==std::string::npos)return false;
 return true;
}
inline bool validate(const std::string &source,std::string &error){
 std::vector<Token> t;std::vector<unsigned> parents(1,0);unsigned scope=0;
 for(size_t i=0;i<source.size();){if(std::isspace((unsigned char)source[i])){++i;continue;}size_t a=i++;char c=source[a];
  if(std::isalnum((unsigned char)c) || c=='_')while(i<source.size() && (std::isalnum((unsigned char)source[i]) || source[i]=='_' || (std::isdigit((unsigned char)c) && source[i]=='.')))++i;
  else if(i<source.size()){std::string pair=source.substr(a,2);if(pair=="++" || pair=="--" || pair=="<=" || pair==">=" || pair=="==" || pair=="!=" || pair=="&&" || pair=="||" || pair=="+=" || pair=="-=" || pair=="*=" || pair=="/=" || pair=="%=" || pair=="&=" || pair=="|=" || pair=="^=")++i;
   else if(i+1<source.size() && ((pair=="<<" || pair==">>") && source[i+1]=='='))i+=2;}
  const auto s=source.substr(a,i-a);t.push_back({s,scope});if(s=="{"){parents.push_back(scope);scope=parents.size()-1;}if(s=="}"){if(!scope){error="Shader braces unbalanced";return false;}scope=parents[scope];}
 }
 auto fail=[&](const char *why){error=std::string("Shader loop proof: ")+why;return false;};
 auto close=[&](size_t start,const char *open,const char *shut){unsigned depth=0;for(size_t j=start;j<t.size();++j){if(t[j].s==open)++depth;if(t[j].s==shut && --depth==0)return j;}return t.size();};
 // Find a dominating literal declaration; mutable locals must be in the same
 // block. Globals must be const. Any intervening write or shadowing rejects.
 auto literal=[&](const std::string &name,size_t at,unsigned loopScope,int &v){
  for(size_t j=at;j-->1;){if(t[j].s!=name)continue;
   if(t[j].scope!=loopScope && t[j].scope!=0)continue;
   if(t[j-1].s==",")return false; // ambiguous multi-declaration/reference
   if(!identifier(t[j-1].s))continue;
   if(!type(t[j-1].s))return false;
   if(t[j].scope!=loopScope && t[j].scope!=0)continue;
   if(t[j].scope==0 && !(j>=2 && t[j-2].s=="const"))return false;
   if(j+3>=at || t[j+1].s!="=" || t[j+3].s!=";" || !integer(t[j+2].s,v))return false;
   for(size_t k=j+4;k<at;++k)if(t[k].s==name && (write(t,k) || (k+1<at && (t[k+1].s=="." || t[k+1].s=="[")) || (k && type(t[k-1].s))))return false;
   if(!callsAllowed(t,j+4,at))return false;
   return true;
  }return false;
 };
 unsigned total=0,weighted=0;
 for(size_t i=0;i<t.size();++i){if(t[i].s=="do")return fail("do loops are not enabled");if(t[i].s!="for" && t[i].s!="while")continue;
  const bool isFor=t[i].s=="for";if(i+1>=t.size() || t[i+1].s!="(")return fail("missing condition");
  size_t end=close(i+1,"(",")"),body=end+1;if(end>=t.size() || body>=t.size() || t[body].s!="{")return fail("braced body required");
  size_t bodyEnd=close(body,"{","}");if(bodyEnd>=t.size())return fail("unbalanced body");
  size_t ca=i+2,cb=end,inc=end;std::string counter;int initial=0,bound=0;
  if(isFor){size_t sep=ca;while(sep<end && t[sep].s!=";")++sep;inc=sep+1;while(inc<end && t[inc].s!=";")++inc;
   size_t init=ca;if(init<sep && type(t[init].s))++init;
   if(sep-init!=3 || t[init+1].s!="=" || !integer(t[init+2].s,initial))return fail("literal counter initializer required");counter=t[init].s;ca=sep+1;cb=inc;
   if(inc+3!=end || t[inc+1].s!=counter || t[inc+2].s!="++")return fail("unit increment required");
  }else{if(ca>=end)return fail("empty condition");counter=t[ca].s;if(!literal(counter,i,t[i].scope,initial))return fail("dominating literal counter required");}
  if(cb-ca<3 || t[ca].s!=counter || (t[ca+1].s!="<" && t[ca+1].s!="<="))return fail("counter must lead bounded condition");
  std::string limit=t[ca+2].s;if(!integer(limit,bound) && !literal(limit,i,t[i].scope,bound))return fail("literal upper bound required");
  std::string stopFlag;int flagInitial=0;
  if(cb-ca>3){
   if(isFor){if(t[ca+3].s!="&&")return fail("unproven additional for condition");}
   else{if(cb-ca!=7 || t[ca+3].s!="&&" || t[ca+5].s!="==" || t[ca+6].s!="0")return fail("unproven additional while condition");
    stopFlag=t[ca+4].s;if(stopFlag==counter || stopFlag==limit || !literal(stopFlag,i,t[i].scope,flagInitial) || flagInitial!=0)return fail("literal zero stop flag required");}
  }
  for(size_t j=ca;j<cb;++j)if(write(t,j))return fail("condition has side effects");
  unsigned iterations=bound>=initial?(unsigned)(bound-initial+(t[ca+1].s=="<=")):0;
  if(iterations>32 || (total+=iterations)>128 || (weighted+=(unsigned)(bodyEnd-body)*iterations)>8192)return fail("iteration/work budget exceeded");
  size_t increment=bodyEnd,exitWrite=bodyEnd;
  if(!isFor && stopFlag.empty()){
   if(bodyEnd<body+4 || t[bodyEnd-3].s!=counter || t[bodyEnd-2].s!="++" || t[bodyEnd-1].s!=";")return fail("unconditional final increment required");increment=bodyEnd-3;
   if(t[increment].scope!=t[body+1].scope)return fail("conditional counter increment");
  }else if(!isFor){
   // Every terminal branch either increments the bounded counter or changes
   // the stop flag so the next condition is false. No injected loop cap.
   for(size_t j=body+1;j+1<bodyEnd;++j)if(t[j].s=="if" && t[j].scope==t[body+1].scope && t[j+1].s=="("){
    size_t c=close(j+1,"(",")"),yes=c+1;if(yes>=bodyEnd || t[yes].s!="{")continue;
    size_t ye=close(yes,"{","}"),no=ye+2;
    if(no>=bodyEnd || t[ye+1].s!="else" || t[no].s!="{")continue;
    size_t ne=close(no,"{","}");if(ne+1!=bodyEnd)continue;
    if(ye-yes==5 && t[yes+1].s==stopFlag && t[yes+2].s=="=" && t[yes+3].s=="1" && t[yes+4].s==";" &&
       ne-no==4 && t[no+1].s==counter && t[no+2].s=="++" && t[no+3].s==";"){exitWrite=yes+1;increment=no+1;break;}
   }
   if(increment==bodyEnd)return fail("both terminal branches must advance or stop");
  }
  for(size_t j=body+1;j<bodyEnd;++j){
   // A single local temporary is safe. Reject shadowing of the proven
   // counter/bound/flag and ambiguous comma declarations at statement level.
   if(j+1<bodyEnd && identifier(t[j].s) && identifier(t[j+1].s) && t[j].s!="return" && t[j].s!="else"){
    const auto &name=t[j+1].s;if(name==counter || name==limit || (!stopFlag.empty() && name==stopFlag))return fail("loop control variable shadowed");
    unsigned depth=0;for(size_t k=j+2;k<bodyEnd && t[k].s!=";";++k){if(t[k].s=="(" || t[k].s=="[")++depth;else if((t[k].s==")" || t[k].s=="]") && depth)--depth;else if(t[k].s=="," && !depth)return fail("ambiguous multi-declaration in loop");}
   }
   if(t[j].s=="for" || t[j].s=="while" || t[j].s=="do" || t[j].s=="continue" || t[j].s=="out" || t[j].s=="inout")return fail("nested loop or unproven control flow");
   if((t[j].s==counter || t[j].s==limit || (!stopFlag.empty() && t[j].s==stopFlag)) && j+1<bodyEnd && (t[j+1].s=="." || t[j+1].s=="["))return fail("counter/bound component access not enabled");
   if(t[j].s==counter && ((write(t,j) && j!=increment) || (j && type(t[j-1].s))))return fail("counter modified or shadowed in body");
   if(!stopFlag.empty() && t[j].s==stopFlag && ((write(t,j) && j!=exitWrite) || (j && type(t[j-1].s))))return fail("stop flag modified outside exit branch");
   if(t[j].s==limit && write(t,j))return fail("bound modified in body");
  }
  if(!callsAllowed(t,ca,cb) || !callsAllowed(t,body+1,bodyEnd))return fail("body calls an unproven helper");
  // Explicit out/inout helpers anywhere could mutate a counter passed from
  // outside the body; this narrow profile intentionally excludes them.
  for(const auto &v:t)if(v.s=="out" || v.s=="inout")return fail("reference parameters not enabled with loops");
  for(size_t j=0;j+1<t.size();++j)if(t[j+1].s=="(" && j && t[j-1].s!="if"){
   size_t e=close(j+1,"(",")");
   const std::string types="|void|bool|int|int2|int3|int4|float|float1|float2|float3|float4|float2x2|float3x3|float4x4|half|half2|half3|half4|";
   const bool declaration=types.find("|"+t[j-1].s+"|")!=std::string::npos;
   if((declaration || (e+1<t.size() && (t[e+1].s=="{" || t[e+1].s==":"))) && t[j].s!="if" && t[j].s!="for" && t[j].s!="while"){
    for(size_t k=ca;k<bodyEnd;++k)if(t[k].s==t[j].s && k+1<t.size() && t[k+1].s=="(")return fail("user helper call in loop");
   }
  }
  i=bodyEnd;
 }
 return true;
}
}
