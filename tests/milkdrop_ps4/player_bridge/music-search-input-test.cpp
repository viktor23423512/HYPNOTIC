#include "music_search_input.h"
#include <cassert>
#include <cstdio>
int main(){MusicSearchInput input;assert(strlen(input.keys())==40);input.start("existing");input.erase();assert(!strcmp(input.text,"existin"));input.clear();assert(!*input.text);input.move(-1,0);assert(input.selected==9);input.move(1,0);assert(input.selected==0);
 for(int i=0;i<4;++i)input.move(0,1);assert(input.selected==40);input.move(1,0);assert(input.selected==41);input.move(0,-1);assert(input.selected==39);input.move(0,1);assert(input.selected==41);assert(input.choose()==-1&&!input.open);
 input.start("");for(int i=0;i<100;++i)input.choose();assert(strlen(input.text)==63);for(int i=0;i<100;++i)input.erase();assert(!*input.text);input.selected=36;input.choose();assert(!strcmp(input.text," "));input.selected=40;assert(input.choose()==1&&!input.open);
 puts("PASS keyboard grid/action navigation,submit/cancel,spaces,delete/clear,63character bound");}
