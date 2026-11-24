#pragma once
#include "ui_settings.h"
#include <math.h>
// UI-only mapping: never wrap visualizer draw calls in ThemedScene.
inline Color uiColor(Color c){
 const auto &ui=currentUiSettings();const int t=UiSettings::bound(ui.theme,0,UiSettings::themeCount-1);
 const Color accents[]={{0,200,230},{65,230,110},{245,175,45},{195,125,255},{230,235,240},{85,157,255},{244,137,182},{255,135,108},{106,225,189}};
 const Color selected[]={{0,55,72},{6,53,27},{57,37,10},{41,23,65},{40,43,49},{23,36,57},{50,28,39},{51,29,25},{24,47,41}};
 const Color panels[]={{4,10,16},{3,13,8},{15,11,5},{10,7,20},{9,11,14},{8,12,19},{17,10,14},{18,11,9},{9,16,14}};
 const Color text[]={{160,180,190},{170,205,182},{215,196,164},{195,185,216},{200,205,212},{177,195,219},{217,187,202},{220,193,181},{183,212,204}};
 if(c.r==0&&c.g==200&&c.b==230)return ui.style==1&&t==0?Color{20,216,235}:accents[t];
 if(c.r==0&&c.g==55&&c.b==72)return ui.style==1&&t==0?Color{13,56,68}:selected[t];
 if(c.r==4&&c.g==10&&c.b==16)return ui.style==1?Color{10,17,26}:panels[t];
 if(c.r==160&&c.g==180&&c.b==190)return ui.style==1?Color{213,224,234}:text[t];
 if((c.r==18&&c.g==35&&c.b==44)||(c.r==20&&c.g==35&&c.b==44)||(c.r==27&&c.g==45&&c.b==55))return ui.style==1?Color{24,35,47}:t?Color{(unsigned char)(panels[t].r+15),(unsigned char)(panels[t].g+15),(unsigned char)(panels[t].b+15)}:c;
 return c;
}
inline bool uiSame(Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b;}
template<class Scene> struct ThemedScene{
 Scene *scene;
 struct Backdrop{int x,y,w,h;Color color;};Backdrop backdrops[64];unsigned used=0;
 explicit ThemedScene(Scene *s):scene(s){}
 void BeginUi(){used=0;}
 Color TextBackground(int x,int y)const{for(unsigned i=used;i>0;--i){const auto &b=backdrops[i-1];if(x>=b.x&&y>=b.y&&x<b.x+b.w&&y<b.y+b.h)return b.color;}return Color{0,0,0};}
 void DrawRectangle(int x,int y,int w,int h,Color original){
  if(w<=0||h<=0)return;Color c=uiColor(original);
  if(currentUiSettings().style==1&&w<=4&&h>=28&&uiSame(original,Color{0,200,230})){y+=10;h-=20;}
  const bool panel=uiSame(original,Color{4,10,16});
  const bool selection=uiSame(original,Color{0,55,72});
  const bool tab=uiSame(original,Color{18,35,44})||uiSame(original,Color{0,200,230});
  if(w>=28&&h>=28){if(used==64){for(unsigned i=1;i<64;++i)backdrops[i-1]=backdrops[i];--used;}backdrops[used++]={x,y,w,h,c};}
  int r=currentUiSettings().style==1?(panel?24:selection?10:tab?12:0):0;
  if(w<28||h<28)r=0;if(r>w/2)r=w/2;if(r>h/2)r=h/2;
  if(!r){scene->DrawRectangle(x,y,w,h,c);return;}
  // Horizontal strips retain the existing rectangle batch and GPU lifecycle.
  if(h>2*r)scene->DrawRectangle(x,y+r,w,h-2*r,c);
  for(int row=0;row<r;++row){const float dy=r-row-.5f;const int inset=(int)ceilf(r-sqrtf(r*r-dy*dy)-.5f);scene->DrawRectangle(x+inset,y+row,w-2*inset,1,c);scene->DrawRectangle(x+inset,y+h-1-row,w-2*inset,1,c);}
 }
};
template<class Scene> inline auto uiTextBackground(Scene *s,int x,int y,int)->decltype(s->TextBackground(x,y)){return s->TextBackground(x,y);}
template<class Scene> inline Color uiTextBackground(Scene *,int,int,long){return Color{0,0,0};}
