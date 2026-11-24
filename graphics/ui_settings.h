#pragma once
#include "audio_output_settings.h"
struct UiSettings {
 AudioOutputSettings audio;
 int theme=0,font=0,hudSeconds=5,sensitivity=2;bool stats=false;
 int style=0,styleFonts[2]={0,3};
 static constexpr int themeCount=9,fontCount=6,settingCount=9;
 static int bound(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
 void setStyle(int value){styleFonts[bound(style,0,1)]=bound(font,0,fontCount-1);style=bound(value,0,1);font=styleFonts[style];}
 void setFont(int value){font=bound(value,0,fontCount-1);styleFonts[bound(style,0,1)]=font;}
 const char *styleName()const{return style==1?"MODERN":"RETRO";}
 const char *themeName()const{static const char *names[]={"NEON CYAN","MATRIX GREEN","AMBER","VIOLET","MONOCHROME","OCEAN BLUE","ROSE","CORAL","MINT"};return names[bound(theme,0,themeCount-1)];}
 const char *fontName()const{static const char *names[]={"PIXEL","TERMINAL","DOT MATRIX","ATKINSON HYPERLEGIBLE","JETBRAINS MONO","RAJDHANI"};return names[bound(font,0,fontCount-1)];}
};
inline const UiSettings *activeUiSettings=nullptr;
inline const UiSettings &currentUiSettings(){static const UiSettings fallback;return activeUiSettings?*activeUiSettings:fallback;}
