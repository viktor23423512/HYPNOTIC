#pragma once
#include "milkdrop_browser.h"
#include <limits.h>
#include "ui_settings.h"
#include <stdio.h>
#include <stdlib.h>
// Stable identities, never list positions, are used for favorites and recents.
// Built-ins and all MilkDrop entries use the existing render/apply paths.
struct VisualizerLibrary {
 static constexpr int tabCount=14,capacity=301,none=INT_MIN,recentCapacity=24;
 UiSettings appearance;
 MilkdropBrowser &milk;const char *const *builtins;int builtinCount;
 int tab=0,selection=0,count=0,rows[capacity]={};bool searching=false,dirty=false,renderHD=true;
 std::string query,remembered[tabCount],favorites[capacity],recent[recentCapacity];
 unsigned favoriteCount=0,recentCount=0;uint64_t changed=0;
 VisualizerLibrary(MilkdropBrowser &m,const char *const *names,int n):milk(m),builtins(names),builtinCount(n){rebuild();}
 static unsigned lower(unsigned c){return c>='A'&&c<='Z'?c+32:c;}
 static bool matches(const char *text,const char *needle){for(;*text;++text){const char *a=text,*b=needle;while(*a&&*b&&lower((unsigned char)*a)==lower((unsigned char)*b)){++a;++b;}if(!*b)return true;}return !*needle;}
 bool valid(int id)const{return id<0?id>=-builtinCount:id<(int)milk.count;}
 const char *name(int id)const{return !valid(id)?"":id<0?builtins[-id-1]:milk.entries[id].name.c_str();}
 std::string key(int id)const{if(!valid(id))return "";return id==-10?"builtin:cube-orbit-eq":id<0?"builtin:"+std::to_string(-id-1):milk.entries[id].path;}
 int resolve(const std::string &key)const{for(int i=0;i<builtinCount;++i)if(key==this->key(-i-1))return -i-1;for(unsigned i=0;i<milk.count;++i)if(key==milk.entries[i].path)return (int)i;return none;}
 const char *tabName(int t)const{return t==0?"BUILT-IN":t==1?"FAVORITES":t==2?"RECENT":t<13?milkdropCategories[t-3]:"IMPORTED";}
 int selected()const{return count&&selection>=0&&selection<count?rows[selection]:none;}
 bool favorite(int id)const{auto k=key(id);for(unsigned i=0;i<favoriteCount;++i)if(favorites[i]==k)return true;return false;}
 void touch(uint64_t now){dirty=true;changed=now;}
 void remember(){if(!searching&&count)remembered[tab]=key(selected());}
 void add(int id){if(valid(id)&&count<capacity)rows[count++]=id;}
 void rebuild(){int old=selection;count=0;
  if(searching){for(int i=0;i<builtinCount;++i)if(matches(name(-i-1),query.c_str()))add(-i-1);for(unsigned i=0;i<milk.count;++i)if(matches(name(i),query.c_str()))add(i);}
  else if(tab==0){for(int i=0;i<builtinCount;++i)add(-i-1);}
  else if(tab==1||tab==2){const auto *keys=tab==1?favorites:recent;unsigned n=tab==1?favoriteCount:recentCount;for(unsigned i=0;i<n;++i)add(resolve(keys[i]));}
  else{const char *category=tab==13?"IMPORTED":milkdropCategories[tab-3];for(unsigned i=0;i<milk.count;++i)if(milk.entries[i].category==category)add(i);}
  selection=old<0?0:old>=count?(count?count-1:0):old;
  if(!searching&&!remembered[tab].empty())for(int i=0;i<count;++i)if(key(rows[i])==remembered[tab]){selection=i;break;}
 }
 void move(int d,uint64_t now){selection+=d;if(selection<0)selection=0;if(selection>=count)selection=count?count-1:0;remember();touch(now);}
 void changeTab(int d,uint64_t now){remember();searching=false;query.clear();tab=(tab+tabCount+d)%tabCount;selection=0;rebuild();touch(now);}
 void search(const char *text,uint64_t now){remember();query=text;searching=true;selection=0;rebuild();touch(now);}
 void endSearch(){searching=false;query.clear();selection=0;rebuild();}
 void toggleFavorite(uint64_t now){int id=selected();if(id==none)return;auto k=key(id);if(k.empty()||k.size()>768||k.find_first_of("\r\n\t")!=std::string::npos)return;
  for(unsigned i=0;i<favoriteCount;++i)if(favorites[i]==k){for(unsigned j=i+1;j<favoriteCount;++j)favorites[j-1]=favorites[j];--favoriteCount;rebuild();touch(now);return;}
  if(favoriteCount<capacity){favorites[favoriteCount++]=k;touch(now);}
 }
 void applied(int id,uint64_t now){if(!valid(id))return;auto k=key(id);if(k.size()>768||k.find_first_of("\r\n\t")!=std::string::npos)return;
  unsigned at=0;while(at<recentCount&&recent[at]!=k)++at;if(at==recentCount&&recentCount<recentCapacity)++recentCount;
  if(at>=recentCapacity)at=recentCapacity-1;for(unsigned i=at;i>0;--i)recent[i]=recent[i-1];recent[0]=k;touch(now);if(tab==2&&!searching)rebuild();
 }
 std::string encode()const{std::string out="HYPNOTIC_UI_1\nV\t"+std::to_string(tab)+"\nQ\t"+(renderHD?"1080":"288")+"\n";out+="U\t"+std::to_string(appearance.theme)+","+std::to_string(appearance.font)+","+std::to_string(appearance.hudSeconds)+","+std::to_string(appearance.sensitivity)+","+std::to_string(appearance.stats?1:0)+"\n";out+="S\t"+std::to_string(appearance.style)+","+std::to_string(appearance.style==0?appearance.font:appearance.styleFonts[0])+","+std::to_string(appearance.style==1?appearance.font:appearance.styleFonts[1])+"\n";const auto &a=appearance.audio;out+="A\t"+std::to_string(a.volume)+","+std::to_string(a.muted?1:0)+","+std::to_string(a.eqEnabled?1:0);for(int gain:a.bands)out+=","+std::to_string(gain);out+="\n";for(int t=0;t<tabCount;++t)if(!remembered[t].empty())out+="T"+std::to_string(t)+"\t"+remembered[t]+"\n";for(unsigned i=0;i<favoriteCount;++i)out+="F\t"+favorites[i]+"\n";for(unsigned i=0;i<recentCount;++i)out+="R\t"+recent[i]+"\n";return out;}
 bool decode(const std::string &text){if(text.empty()||text.back()!='\n'||text.size()>262144||text.compare(0,14,"HYPNOTIC_UI_1\n")!=0)return false;
  appearance=UiSettings{};renderHD=true;favoriteCount=recentCount=0;for(auto &s:remembered)s.clear();bool hasStyle=false;size_t at=14;
  while(at<text.size()){size_t end=text.find('\n',at);if(end==std::string::npos)return false;size_t split=text.find('\t',at);if(split<end){auto tag=text.substr(at,split-at),value=text.substr(split+1,end-split-1);if(value.size()<=768&&value.find_first_of("\r\n\t")==std::string::npos){
   if(tag=="U"){int theme,font,hud,sens,stats;char extra;if(sscanf(value.c_str(),"%d,%d,%d,%d,%d%c",&theme,&font,&hud,&sens,&stats,&extra)==5&&theme>=0&&theme<UiSettings::themeCount&&font>=0&&font<UiSettings::fontCount&&hud>=3&&hud<=15&&sens>=0&&sens<=6&&(stats==0||stats==1)){appearance.theme=theme;appearance.font=font;appearance.hudSeconds=hud;appearance.sensitivity=sens;appearance.stats=stats!=0;}}
   else if(tag=="S"){int style,retro,modern;char extra;if(sscanf(value.c_str(),"%d,%d,%d%c",&style,&retro,&modern,&extra)==3&&style>=0&&style<2&&retro>=0&&retro<UiSettings::fontCount&&modern>=0&&modern<UiSettings::fontCount){appearance.style=style;appearance.styleFonts[0]=retro;appearance.styleFonts[1]=modern;hasStyle=true;}}
   else if(tag=="A")appearance.audio.decode(value.c_str());
   else if(tag=="Q")renderHD=value=="1080";
   else if(tag=="V"){char *tail=nullptr;long n=strtol(value.c_str(),&tail,10);if(tail&&!*tail&&n>=0&&n<tabCount)tab=(int)n;}
   else if(tag=="F"||tag=="R"){auto *keys=tag=="F"?favorites:recent;unsigned &n=tag=="F"?favoriteCount:recentCount;unsigned cap=tag=="F"?capacity:recentCapacity;bool duplicate=false;for(unsigned i=0;i<n;++i)duplicate|=keys[i]==value;if(!duplicate&&!value.empty()&&n<cap)keys[n++]=value;}
   else if(tag.size()>1&&tag[0]=='T'){char *tail=nullptr;long n=strtol(tag.c_str()+1,&tail,10);if(tail&&!*tail&&n>=0&&n<tabCount)remembered[n]=value;}
  }}at=end+1;}
  if(hasStyle)appearance.font=appearance.styleFonts[appearance.style];else appearance.styleFonts[0]=appearance.font;
  selection=0;searching=false;query.clear();rebuild();dirty=false;return true;
 }
};
