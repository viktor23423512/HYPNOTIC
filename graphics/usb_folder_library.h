#pragma once
#include "track_library.h"
#include <string.h>
struct UsbFolderLibrary {
 static constexpr int capacity=1024,maxDepth=12;
 int count=0;bool limited=false;
 int parent[capacity]={};char paths[capacity][TrackLibrary::pathSize]={};
 int add(const char *path,int owner){
  if(owner < -1 || owner>=count || strlen(path)>=TrackLibrary::pathSize)return -1;
  for(int i=0;i<count;++i)if(TrackLibrary::compare(TrackLibrary::aliasKey(path),TrackLibrary::aliasKey(paths[i]))==0)return i;
  if(count==capacity){limited=true;return -1;}
  const int i=count++;parent[i]=owner;strcpy(paths[i],path);return i;
 }
};
