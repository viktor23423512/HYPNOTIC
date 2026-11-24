#pragma once
#include "ui_font_data.h"
inline unsigned uiFontIndex(int font,int size=24){unsigned best=0;int distance=1000;for(unsigned i=0;i<uiFontSizeCount;++i){int d=(int)uiFontSizes[i]-size;if(d<0)d=-d;if(d<distance){best=i;distance=d;}}return (font-3)*uiFontSizeCount+best;}
inline int uiGlyphAdvance(unsigned c,int font,int size=24){return font>=3&&font<6&&c>=32&&c<=126?uiFontAdvances[uiFontIndex(font,size)][c-32]:18*size/24;}
inline int uiTextWidth(const char *text,int maxChars=1024,int font=-1,int size=24){if(font<0)font=currentUiSettings().font;int width=0,n=0;for(;n<maxChars&&text[n];++n)if(font>=3)width+=uiGlyphAdvance((unsigned char)text[n],font,size);return font<3?n*18*size/24:width;}

// Native-size raster coverage, grouped by ink colour. No runtime font parser,
// texture allocation, bitmap enlargement or GPU ownership changes.
template<class Scene> inline void drawUiFont(Scene *scene,const char *text,int y,int startX,int maxChars,Color ink,int font,int size=24){
 if(font<3||font>5)return;const unsigned index=uiFontIndex(font,size);
 for(unsigned level=1;level<=8;++level){int cursor=startX;for(int n=0;n<maxChars&&text[n];++n){
  const unsigned c=(unsigned char)text[n];const int x=cursor;cursor+=uiGlyphAdvance(c,font,size);if(c<32||c>126)continue;
  const Color bg=uiTextBackground(scene,x,y+size/2,0);
  const Color color={(unsigned char)((ink.r*level+bg.r*(8-level)+4)/8),(unsigned char)((ink.g*level+bg.g*(8-level)+4)/8),(unsigned char)((ink.b*level+bg.b*(8-level)+4)/8)};
  const auto *offset=uiFontOffsets[index];for(unsigned i=offset[c-32];i<offset[c-31];++i){const auto &run=uiFontRuns[i];if(run.coverage==level)scene->DrawRectangle(x+run.x,y+run.y,run.width,1,color);}
 }}
}
