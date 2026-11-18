#pragma once
struct UiSettings {
 int theme=0,font=0,hudSeconds=5,sensitivity=2;bool stats=false;
 static constexpr int themeCount=5,fontCount=3;
 static int bound(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
 const char *themeName()const{static const char *names[]={"NEON CYAN","MATRIX GREEN","AMBER","VIOLET","MONOCHROME"};return names[bound(theme,0,4)];}
 const char *fontName()const{static const char *names[]={"PIXEL","TERMINAL","DOT MATRIX"};return names[bound(font,0,2)];}
};
inline const UiSettings *activeUiSettings=nullptr;
inline const UiSettings &currentUiSettings(){static const UiSettings fallback;return activeUiSettings?*activeUiSettings:fallback;}
