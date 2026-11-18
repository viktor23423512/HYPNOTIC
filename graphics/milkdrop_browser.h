#pragma once
#include "milkdrop_catalog.h"
#include "../tests/milkdrop_ps4/native_graphics/preset_folder.hpp"
struct MilkdropBrowser {
 struct Entry{std::string name,category,path;};Entry entries[290];unsigned count=milkdropEntryCount,category=0,selection=0;bool scanned=false;std::string activePath,error;
 MilkdropBrowser(){for(unsigned i=0;i<count;++i)entries[i]={milkdropEntries[i].name,milkdropEntries[i].category,milkdropEntries[i].path};}
 const char *categoryName()const{return category<10?milkdropCategories[category]:"IMPORTED";}
 unsigned visibleCount()const{unsigned n=0;for(unsigned i=0;i<count;++i)n+=entries[i].category==categoryName();return n;}
 unsigned index(unsigned row)const{for(unsigned i=0;i<count;++i)if(entries[i].category==categoryName()){if(!row--)return i;}return count;}
 void move(int step){const auto n=visibleCount();if(!n){selection=0;return;}int next=(int)selection+step;selection=next<0?0:(unsigned)next>=n?n-1:next;}
 void changeCategory(int step){category=(category+11+step)%11;selection=0;error.clear();}
 const char *activeName()const{for(unsigned i=0;i<count;++i)if(entries[i].path==activePath)return entries[i].name.c_str();return "MILKDROP";}
 int peekNext()const{unsigned at=count;for(unsigned i=0;i<count;++i)if(entries[i].path==activePath){at=i;break;}if(at==count)return -1;for(unsigned n=1;n<=count;++n){unsigned i=(at+n)%count;if(entries[i].category==entries[at].category)return (int)i;}return -1;}
 int cycle(int direction){unsigned active=count;for(unsigned i=0;i<count;++i)if(entries[i].path==activePath){active=i;break;}if(active==count)return -1;
  category=10;for(unsigned c=0;c<10;++c)if(entries[active].category==milkdropCategories[c]){category=c;break;}
  unsigned row=0;for(unsigned i=0;i<active;++i)if(entries[i].category==categoryName())++row;
  const unsigned n=visibleCount();if(!n)return -1;if(!error.empty())row=selection;
  selection=(row+n+(direction<0?-1:1))%n;error.clear();return (int)index(selection);
 }
 void rescan(){PresetFolder folder;folder.scan("/data/hypnotic/presets");for(unsigned i=0;i<8;++i)folder.scan("/mnt/usb"+std::to_string(i)+"/HYPNOTIC/presets");count=milkdropEntryCount;
  for(unsigned i=0;i<folder.count && count<290;++i){auto path=folder.files[i];auto at=path.find_last_of('/');entries[count++]={path.substr(at+1),"IMPORTED",path};}scanned=true;if(selection>=visibleCount())selection=0;
 }
};
