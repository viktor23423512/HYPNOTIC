#pragma once
#include "ui_settings.h"
// Only UI draw calls pass through this adapter; visualizer colours are untouched.
inline Color uiColor(Color c){
 const int theme=currentUiSettings().theme;if(theme<=0||theme>=UiSettings::themeCount)return c;
 const Color accents[4]={{65,230,110},{245,175,45},{195,125,255},{230,235,240}};
 const Color selected[4]={{6,53,27},{57,37,10},{41,23,65},{40,43,49}};
 const Color backgrounds[4]={{3,13,8},{15,11,5},{10,7,20},{9,11,14}};
 const Color text[4]={{170,205,182},{215,196,164},{195,185,216},{200,205,212}};
 const int t=theme-1;
 if(c.r==0&&c.g==200&&c.b==230)return accents[t];
 if(c.r==0&&c.g==55&&c.b==72)return selected[t];
 if(c.r==4&&c.g==10&&c.b==16)return backgrounds[t];
 if(c.r==160&&c.g==180&&c.b==190)return text[t];
 if((c.r==18&&c.g==35&&c.b==44)||(c.r==20&&c.g==35&&c.b==44)||(c.r==27&&c.g==45&&c.b==55))return Color{(unsigned char)(backgrounds[t].r+15),(unsigned char)(backgrounds[t].g+15),(unsigned char)(backgrounds[t].b+15)};
 return c;
}
template<class Scene> struct ThemedScene{Scene *scene;explicit ThemedScene(Scene *s):scene(s){}void DrawRectangle(int x,int y,int w,int h,Color c){scene->DrawRectangle(x,y,w,h,uiColor(c));}};
