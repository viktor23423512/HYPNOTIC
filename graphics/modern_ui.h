#pragma once
#include "status_text.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
template<class Scene> struct UiScaledText {
 Scene *scene;int x,y,size;
 void DrawRectangle(int xx,int yy,int w,int h,Color c){const int left=x+xx*size/24,top=y+yy*size/24;int width=(xx+w)*size/24-xx*size/24,height=(yy+h)*size/24-yy*size/24;scene->DrawRectangle(left,top,width>0?width:1,height>0?height:1,c);}
 Color TextBackground(int xx,int yy)const{return uiTextBackground(scene,x+xx*size/24,y+yy*size/24,0);}
};
inline int uiModernTextSize(int size){return size;}
// Every label has an explicit width budget; preserve the original retro functions.
template<class Scene> void uiLabel(Scene *s,const char *text,int x,int y,int size=24,Color color=Color{160,180,190},int width=1664,int align=0){
 size=uiModernTextSize(size);char label[192];int n=0,used=0;while(text[n]&&n<191){const unsigned c=(unsigned char)text[n];const int next=currentUiSettings().font<3?((n+1)*18*size/24-n*18*size/24):uiGlyphAdvance(c,currentUiSettings().font,size);if(used+next>width)break;label[n]=text[n];used+=next;++n;}label[n]=0;
 if(text[n]){const int dots=uiTextWidth("...",3,-1,size);while(n>0&&uiTextWidth(label,n,-1,size)+dots>width)label[--n]=0;if(dots<=width&&n+3<192){strcat(label,"...");n+=3;}used=uiTextWidth(label,n,-1,size);}
 const int measured=used;if(align==1)x-=measured/2;else if(align==2)x-=measured;
 if(currentUiSettings().font>=3){drawUiFont(s,label,y,x,n,uiColor(color),currentUiSettings().font,size);int cursor=x;for(int i=0;i<n;++i){if((unsigned char)label[i]>=128){char icon[2]={label[i],0};UiScaledText<Scene> scaled{s,cursor,y,size};drawStatusText(&scaled,icon,0,0,1,color);}cursor+=uiGlyphAdvance((unsigned char)label[i],currentUiSettings().font,size);}}else{UiScaledText<Scene> scaled{s,x,y,size};drawStatusText(&scaled,label,0,0,n,color);}
}
template<class Scene> void uiRounded(Scene *s,int x,int y,int w,int h,int radius,Color color){
 for(int row=0;row<h;++row){int inset=0;if(row<radius||row>=h-radius){float dy=row<radius?radius-row-.5f:row-(h-radius)+.5f;inset=(int)ceilf(radius-sqrtf(radius*radius-dy*dy)-.5f);}s->DrawRectangle(x+inset,y+row,w-2*inset,1,color);}
}
template<class Scene> void uiModernBrand(Scene *s){
 // Keep the original pixel wordmark regardless of the selected body font.
 const char *brand="HYPNOTIC";UiSettings pixel=currentUiSettings();pixel.font=0;const auto *saved=activeUiSettings;activeUiSettings=&pixel;drawStatusText(s,brand,45,88,8,Color{0,200,230});activeUiSettings=saved;
 uiRounded(s,1460,40,37,26,7,Color{100,125,140});uiRounded(s,1462,42,33,22,5,Color{4,10,16});s->DrawRectangle(1478,45,1,16,Color{160,180,190});
 uiLabel(s,"Music",1510,39,23);
 for(int i=0;i<3;++i)s->DrawRectangle(1604,46+i*7,27,2,Color{160,180,190});s->DrawRectangle(1610,43,4,8,Color{160,180,190});s->DrawRectangle(1622,50,4,8,Color{160,180,190});s->DrawRectangle(1615,57,4,8,Color{160,180,190});
 uiRounded(s,1644,38,43,30,8,Color{100,125,140});uiRounded(s,1646,40,39,26,6,Color{4,10,16});for(int i=0;i<3;++i)s->DrawRectangle(1658,46+i*5,15,1,Color{160,180,190});
 uiLabel(s,"Settings",1700,39,23);
}
template<class Scene> void uiModernPanel(Scene *s,const char *title,unsigned selected,unsigned count){
 uiModernBrand(s);s->DrawRectangle(88,100,1744,680,Color{4,10,16});uiLabel(s,title,128,132,36,Color{236,243,248});
 char value[32];snprintf(value,sizeof(value),"%02u / %u",selected,count);uiLabel(s,value,1792,144,23,Color{0,200,230},220,2);
}
template<class Scene> int uiModernTab(Scene *s,const char *name,int x,bool active){
 if(!strcmp(name,"BUILT-IN"))name="Built-in";else if(!strcmp(name,"FAVORITES"))name="Favorites";else if(!strcmp(name,"RECENT"))name="Recent";else if(!strcmp(name,"IMPORTED"))name="Imported";else if(!strcmp(name,"ALL MUSIC"))name="All music";
 int w=uiTextWidth(name,1024,-1,22)+48;if(w<124)w=124;if(w>240)w=240;
 s->DrawRectangle(x,204,w,48,active?Color{0,200,230}:Color{18,35,44});uiLabel(s,name,x+w/2,214,22,active?Color{0,20,26}:Color{160,180,190},w-28,1);return w+12;
}
template<class Scene> void uiModernFooter(Scene *s,const char *hint){s->DrawRectangle(128,687,1664,1,Color{27,45,55});uiLabel(s,hint,130,720,21,Color{160,180,190},1540);uiLabel(s,"\x86 Close",1792,720,21,Color{160,180,190},190,2);}
template<class Scene> void uiModernStar(Scene *s,int x,int y,bool favorite){
 const int pts[11][2]={{0,-11},{3,-4},{11,-3},{5,2},{7,10},{0,6},{-7,10},{-5,2},{-11,-3},{-3,-4},{0,-11}};
 Color color=favorite?Color{0,200,230}:Color{82,106,124};for(int i=0;i<10;++i){int dx=pts[i+1][0]-pts[i][0],dy=pts[i+1][1]-pts[i][1],n=abs(dx)>abs(dy)?abs(dx):abs(dy);for(int j=0;j<=n;++j)s->DrawRectangle(x+pts[i][0]+dx*j/n,y+pts[i][1]+dy*j/n,2,2,color);}
}
