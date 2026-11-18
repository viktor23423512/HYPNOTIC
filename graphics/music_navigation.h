#pragma once
#include "music_folder_browser.h"
// Published scan data is immutable. No filesystem work is done while navigating.
struct MusicNavigation {
 bool audible[UsbFolderLibrary::capacity]={};int roots[8]={},rootCount=0,source=0;
 int savedFolder[9]={},savedRow[9]={},seenFolders=-1,seenTracks=-1;
 bool initialized=false,searchResults=false;int searchFolder=-1,searchRow=0,searchSource=0;
 void remember(const MusicFolderBrowser &b){if(!searchResults){savedFolder[source]=b.folder;savedRow[source]=b.selection;}}
 void restore(MusicFolderBrowser &b){b.searching=source==0;b.query[0]=0;b.folder=source?savedFolder[source]:-1;b.selection=savedRow[source];b.dirty=true;b.refresh();}
 void refresh(MusicFolderBrowser &b){
  if(!usbLibraryReady()){b.refresh();return;}
  const int nf=usbFolderCount(),nt=usbTrackCount();
  if(nf!=seenFolders||nt!=seenTracks){
   seenFolders=nf;seenTracks=nt;for(auto &v:audible)v=false;
   for(int t=0;t<nt;++t){int f=usbTrackFolder(t);for(int depth=0;f>=0&&f<nf&&f<UsbFolderLibrary::capacity&&depth<=UsbFolderLibrary::maxDepth+1;++depth){audible[f]=true;f=usbFolderParent(f);}}
   rootCount=0;for(int f=0;f<nf&&f<UsbFolderLibrary::capacity&&rootCount<8;++f)if(usbFolderParent(f)<0&&audible[f])roots[rootCount++]=f;
   b.visibleFolders=audible;b.dirty=true;
  }
  if(!initialized){initialized=true;for(int i=0;i<rootCount;++i)savedFolder[i+1]=roots[i];source=rootCount?1:0;restore(b);}
  b.refresh();
 }
 void changeSource(int delta,MusicFolderBrowser &b){refresh(b);remember(b);source=(source+delta+rootCount+1)%(rootCount+1);searchResults=false;restore(b);}
 int apply(MusicFolderBrowser &b){const int track=b.apply();remember(b);return track;}
 bool back(MusicFolderBrowser &b){
  if(searchResults){source=searchSource;savedFolder[source]=searchFolder;savedRow[source]=searchRow;searchResults=false;restore(b);return true;}
  if(source==0||b.folder==roots[source-1])return false;
  const bool result=b.back();remember(b);return result;
 }
 void search(const char *text,MusicFolderBrowser &b){refresh(b);if(!searchResults){searchSource=source;searchFolder=b.folder;searchRow=b.selection;}searchResults=true;b.search(text);}
 const char *tabName(int tab)const{return tab==0?"ALL MUSIC":TrackLibrary::name(usbFolderPath(roots[tab-1]));}
 void breadcrumb(const MusicFolderBrowser &b,char *out,size_t size)const{
  if(searchResults){snprintf(out,size,"SEARCH: %.72s",b.query);return;}
  if(!source){snprintf(out,size,"ALL TRACKS / ALL USB DRIVES");return;}
  const char *path=b.path();if(!strncmp(path,"/mnt/",5))path+=5;snprintf(out,size,"%s",path);
 }
};
