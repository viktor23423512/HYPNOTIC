#pragma once
#include "audio.h"
#include <stdio.h>
#include "usb_folder_library.h"
struct MusicFolderBrowser {
 const bool *visibleFolders=nullptr;
 struct Entry{int index;bool folder;};
 Entry entries[TrackLibrary::capacity+UsbFolderLibrary::capacity]={};
 int folder=-1,selection=0,count=0,seenFolders=-1,seenTracks=-1;bool dirty=true,searching=false;char query[64]={};int returnFolder=-1,returnSelection=0;
 const char *name(const Entry &e)const{return e.folder?TrackLibrary::name(usbFolderPath(e.index)):usbTrackName(e.index);}
 void refresh(){int nf=usbFolderCount(),nt=usbTrackCount();if(!dirty && nf==seenFolders && nt==seenTracks)return;
  count=0;seenFolders=nf;seenTracks=nt;dirty=false;
  if(!searching)for(int i=0;i<nf;++i)if(usbFolderParent(i)==folder&&(!visibleFolders||visibleFolders[i]))entries[count++]={i,true};
  for(int i=0;i<nt;++i)if(searching?matches(usbTrackName(i),query):usbTrackFolder(i)==folder)entries[count++]={i,false};
  for(int i=1;i<count;++i){Entry e=entries[i];int j=i-1;while(j>=0){auto &p=entries[j];int order=p.folder!=e.folder?(p.folder?-1:1):TrackLibrary::compare(name(p),name(e));if(order<=0)break;entries[j+1]=p;--j;}entries[j+1]=e;}
  if(selection>=count)selection=count?count-1:0;
 }
 void move(int delta){refresh();selection+=delta;if(selection<0)selection=0;if(selection>=count)selection=count?count-1:0;}
 // Return the original global playback index, never a folder/page index.
 int apply(){refresh();if(!count)return -1;auto e=entries[selection];if(!e.folder)return e.index;folder=e.index;selection=0;dirty=true;refresh();return -1;}
 bool back(){if(searching){searching=false;folder=returnFolder;selection=returnSelection;dirty=true;refresh();return true;}if(folder<0)return false;int child=folder;folder=usbFolderParent(folder);selection=0;dirty=true;refresh();for(int i=0;i<count;++i)if(entries[i].folder && entries[i].index==child){selection=i;break;}return true;}
 static bool matches(const char *text,const char *needle){if(!*needle)return true;for(;*text;++text){const char *a=text,*b=needle;while(*a && *b && TrackLibrary::lower(*a)==TrackLibrary::lower(*b)){++a;++b;}if(!*b)return true;}return false;}
 void search(const char *text){if(!searching){returnFolder=folder;returnSelection=selection;}searching=true;snprintf(query,sizeof(query),"%s",text);selection=0;dirty=true;refresh();}
 const char *path()const{return searching?"ALL USB MUSIC":folder<0?"USB DRIVES":usbFolderPath(folder);}
};
