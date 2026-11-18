#include "milkdrop_browser.h"
#include <cassert>
#include <cstdio>
int main(){MilkdropBrowser b;assert(b.cycle(1)==-1);b.category=0;auto first=b.index(0);auto last=b.index(b.visibleCount()-1);b.activePath=b.entries[first].path;assert(b.cycle(-1)==(int)last);assert(b.activePath==b.entries[first].path);b.activePath=b.entries[last].path;assert(b.cycle(1)==(int)first);b.activePath=b.entries[first].path;b.selection=0;b.error="rejected";auto next=b.cycle(1);assert(next==(int)b.index(1));b.error="rejected";assert(b.cycle(1)==(int)b.index(2));assert(b.error.empty());assert(b.activeName()==b.entries[first].name);puts("PASS active category wrap,failed-preset skip,name lookup,and no active change before successful apply");}
