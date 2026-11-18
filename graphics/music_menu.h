#pragma once
#include "status_text.h"
#include "music_navigation.h"
template<class Scene> void drawMusicLibrary(Scene *scene,MusicNavigation &navigation,MusicFolderBrowser &b,bool paused){
 navigation.refresh(b);
 scene->DrawRectangle(48,164,1824,636,Color{4,10,16});drawStatusText(scene,"HYPNOTIC / MUSIC",194,72,45);
 drawStatusText(scene,"\x85 MIC MODE",194,1248,16);
 char text[160];snprintf(text,sizeof(text),"%d / %d",b.count?b.selection+1:0,b.count);drawStatusText(scene,text,194,1620,12);
 int first=navigation.source-2;if(first<0)first=0;const int tabs=navigation.rootCount+1;if(first>tabs-5)first=tabs>5?tabs-5:0;
 drawStatusText(scene,"\x82",254,72,1);drawStatusText(scene,"\x83",254,1812,1);
 for(int t=first;t<tabs&&t<first+5;++t){int x=120+(t-first)*330;bool selected=t==navigation.source&&!navigation.searchResults;Color ink=selected?Color{0,20,26}:Color{160,180,190};scene->DrawRectangle(x,240,324,48,selected?Color{0,200,230}:Color{18,35,44});drawStatusText(scene,"\x8c",254,x+12,1,ink);drawStatusText(scene,navigation.tabName(t),254,x+42,15,ink);}
 scene->DrawRectangle(64,288,1792,3,Color{0,200,230});navigation.breadcrumb(b,text,sizeof(text));drawStatusText(scene,text,310,80,96);
 const int start=b.selection/6*6;
 for(int row=start;row<b.count&&row<start+6;++row){int y=356+(row-start)*54;const auto &entry=b.entries[row];if(row==b.selection){scene->DrawRectangle(64,y-10,1792,44,Color{0,55,72});scene->DrawRectangle(64,y-10,4,44,Color{0,200,230});}
  snprintf(text,sizeof(text),"%02d",row+1);drawStatusText(scene,text,y,80,4);if(entry.folder)drawStatusText(scene,"\x8c",y,176,1);
  drawStatusText(scene,b.name(entry),y,212,72);
  if(!entry.folder){if(entry.index==usbActiveTrack())drawStatusText(scene,usbAudioFinished()?"\x8a":paused?"\x89":"\x88",y,1584,1,Color{0,200,230});const uint64_t seconds=usbTrackDurationMs(entry.index)/1000;snprintf(text,sizeof(text),"%02llu:%02llu",(unsigned long long)(seconds/60),(unsigned long long)(seconds%60));drawStatusText(scene,text,y,1664,10);}
 }
 if(!usbLibraryReady())drawStatusText(scene,"SCANNING YOUR USB MUSIC...",402,80,90);
 else if(!b.count)drawStatusText(scene,navigation.searchResults?"NO MATCHES - \x8e CHANGE SEARCH":"NO MUSIC FOUND - CONNECT USB AND RELAUNCH",402,80,90);
 if(usbLibraryLimited())drawStatusText(scene,"SCAN LIMIT REACHED - SOME FILES OR DEEP FOLDERS WERE OMITTED",700,80,94);
 else if(b.count){const auto &entry=b.entries[b.selection];if(entry.folder)drawStatusText(scene,"X OPEN FOLDER",700,80,90);else{const char *parent=usbFolderPath(usbTrackFolder(entry.index));if(!strncmp(parent,"/mnt/",5))parent+=5;snprintf(text,sizeof(text),"IN: %.88s",parent);drawStatusText(scene,text,700,80,94);}}
 scene->DrawRectangle(64,744,1792,2,Color{27,45,55});drawStatusText(scene,"\x82\x83 TABS  \x80\x81 BROWSE  L1 R1 PAGE  X OPEN / PLAY  \x8e / L3 SEARCH  \x86 BACK",768,80,98);
}
