#pragma once
// Audio-thread-only startup scan. Publish the completed catalogue once; the
// renderer never reads mutable directory/decoder state. USB access is read-only.
static void scanMusicFolder(int folder,int depth,bool &foundAudio,int &readError){
 const int fd=sceKernelOpen(folders.paths[folder],0,0);if(fd<0){if(!readError)readError=fd;return;}
 alignas(8) char entries[4096];bool stop=false;
 while(!stop){const int bytes=sceKernelGetdents(fd,entries,sizeof(entries));
  if(bytes<=0){if(bytes<0 && !readError)readError=bytes;break;}
  if(bytes>(int)sizeof(entries)){readError=-1;break;}
  size_t offset=0;while(offset<(size_t)bytes){const size_t nameOffset=offsetof(dirent,d_name),available=bytes-offset;
   if(available<nameOffset+1){readError=-1;stop=true;break;}
   unsigned short length;memcpy(&length,entries+offset+offsetof(dirent,d_reclen),sizeof(length));
   unsigned names=(unsigned char)entries[offset+offsetof(dirent,d_namlen)];
   if(length<nameOffset+1 || length>available || !names || names>=length-nameOffset || names>=sizeof(((dirent*)0)->d_name)){readError=-1;stop=true;break;}
   const char *name=entries+offset+nameOffset;unsigned type=(unsigned char)entries[offset+offsetof(dirent,d_type)];offset+=length;
   if(name[names]!=0 || memchr(name,'/',names) || memchr(name,0,names)){readError=-1;stop=true;break;}
   if(name[0]=='.' || type==DT_LNK)continue;
   char path[TrackLibrary::pathSize];int n=snprintf(path,sizeof(path),"%s/%s",folders.paths[folder],name);if(n<0 || n>=(int)sizeof(path)){folders.limited=true;continue;}
   if(type==DT_UNKNOWN){OrbisKernelStat st={};if(sceKernelStat(path,&st)<0)continue;type=S_ISDIR(st.st_mode)?DT_DIR:S_ISREG(st.st_mode)?DT_REG:DT_UNKNOWN;}
   if(type==DT_DIR){if(depth>=UsbFolderLibrary::maxDepth){folders.limited=true;continue;}const int child=folders.add(path,folder);if(child>=0)scanMusicFolder(child,depth+1,foundAudio,readError);}
   else if(type==DT_REG && AudioDecoder::supportedExtension(name)){
    foundAudio=true;AudioDecoder decoder;if(!decoder.open(path))continue;
    library.add(path,scaleAudioFrames(decoder.frames,decoder.sampleRate,1000),folder);
    if(library.count==TrackLibrary::capacity){folders.limited=true;stop=true;break;}
   }
   if(library.count==TrackLibrary::capacity){stop=true;break;}
  }
 }
 sceKernelClose(fd);
}
