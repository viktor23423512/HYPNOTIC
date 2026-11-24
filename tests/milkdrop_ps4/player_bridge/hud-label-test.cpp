#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
struct Color{unsigned char r,g,b;};
struct Rect{int x,y,w,h;Color c;bool operator==(const Rect &b)const{return x==b.x&&y==b.y&&w==b.w&&h==b.h&&c.r==b.c.r&&c.g==b.c.g&&c.b==b.c.b;}};
struct Scene{bool record=true;std::vector<Rect> rects;void DrawRectangle(int x,int y,int w,int h,Color c){assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=1920&&y+h<=1080);if(record)rects.push_back({x,y,w,h,c});}};
#include "player_hud.h"
int main(){UiSettings ui;activeUiSettings=&ui;unsigned cases=0;
 for(int style=0;style<2;++style)for(int font=0;font<6;++font){ui.style=style;ui.font=font;Scene scene;scene.record=false;for(int focus=-1;focus<7;++focus)for(int shuffle=0;shuffle<2;++shuffle)for(int repeat=0;repeat<3;++repeat)for(int paused=0;paused<2;++paused){drawPlayerHud(&scene,PlayerHudData{"TRACK03.OGG","DOT MATRIX TEST",111000,317000,shuffle!=0,repeat,focus,true,600,paused!=0});++cases;}}
 for(int style=0;style<2;++style)for(int kind=0;kind<8;++kind){ui.style=style;Scene icon,focus;hudIcon(&icon,kind,200,200,Color{0,200,230});hudHighlight(&focus,kind,200,200);int l=10000,t=10000,r=0,b=0;for(const auto &v:icon.rects){if(v.x<l)l=v.x;if(v.y<t)t=v.y;if(v.x+v.w>r)r=v.x+v.w;if(v.y+v.h>b)b=v.y+v.h;}assert(focus.rects.size()==1);auto f=focus.rects[0];assert(abs((2*f.x+f.w)-(l+r))<=1&&abs((2*f.y+f.h)-(t+b))<=1);}
 HudIconMask shuffle(3);assert(shuffle.coverage[15][19]>0);assert(shuffle.coverage[17][18]==0);assert(shuffle.coverage[5][34]>0&&shuffle.coverage[25][34]>0);
 Scene off,on;ui.style=1;ui.font=3;drawPlayerHud(&off,PlayerHudData{"Track","Preset",0,100,false,0,-1,false});drawPlayerHud(&on,PlayerHudData{"Track","Preset",0,100,true,2,-1,true});assert(off.rects!=on.rects);
 ui.font=2;assert(uiTextWidth("Shuffle off",32,2,23)==11*18*23/24);
 printf("PASS %u HUD layouts, 16 centred icon highlights, independent shuffle paths and visible toggle state changes\n",cases);
}
