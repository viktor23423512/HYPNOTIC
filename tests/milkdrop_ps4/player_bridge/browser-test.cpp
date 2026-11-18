#include "milkdrop_browser.h"
#include <cassert>
#include <cstdio>
int main(){MilkdropBrowser b;assert(b.count==130);unsigned total=0;for(unsigned c=0;c<10;++c){assert(b.category==c);unsigned n=b.visibleCount();assert(n>=10);total+=n;b.activePath="still-active";b.move(999);assert(b.selection==n-1);auto index=b.index(b.selection);assert(index<b.count);assert(b.entries[index].category==b.categoryName());b.move(-999);assert(b.selection==0);b.changeCategory(1);assert(b.activePath=="still-active");}assert(total==130&&b.category==10&&b.visibleCount()==0);b.move(1);assert(b.selection==0);assert(b.index(0)==b.count);b.changeCategory(1);assert(b.category==0);puts("PASS130 originals, ten category bounds, empty imports, browse/apply separation and category wrap");}
