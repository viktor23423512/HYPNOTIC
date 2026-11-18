#include "track_library.h"
#include "usb_folder_library.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <cstddef>
#include <sys/stat.h>
static TrackLibrary library;static UsbFolderLibrary folders;
static const unsigned DT_UNKNOWN=0,DT_DIR=4,DT_REG=8,DT_LNK=10;
struct dirent{uint32_t d_fileno;unsigned short d_reclen;unsigned char d_type,d_namlen;char d_name[256];};
using OrbisKernelStat=struct stat;
struct Child{std::string name;unsigned type;};struct Node{bool directory=false,playable=false,corrupt=false;std::vector<Child> children;};
static std::map<std::string,Node> nodes;struct Handle{std::string path;unsigned position=0;};static std::map<int,Handle> handles;static int nextFd=1,decoded=0;
static int sceKernelOpen(const char *p,int flags,int){assert(flags==0);if(!nodes.count(p)||!nodes[p].directory)return -2;int fd=nextFd++;handles[fd]={p,0};return fd;}
static int sceKernelClose(int fd){assert(handles.erase(fd)==1);return 0;}
static int sceKernelGetdents(int fd,void *out,size_t size){auto &h=handles.at(fd);auto &n=nodes.at(h.path);if(n.corrupt){memset(out,0,16);return 16;}size_t used=0;
 while(h.position<n.children.size()){auto &c=n.children[h.position];size_t bytes=(offsetof(dirent,d_name)+c.name.size()+1+7)&~size_t(7);if(used+bytes>size)break;dirent e={};e.d_reclen=bytes;e.d_type=c.type;e.d_namlen=c.name.size();strcpy(e.d_name,c.name.c_str());memcpy((char*)out+used,&e,bytes);used+=bytes;++h.position;}return used;}
static int sceKernelStat(const char *p,OrbisKernelStat *st){if(!nodes.count(p))return -2;st->st_mode=nodes[p].directory?S_IFDIR:S_IFREG;return 0;}
struct AudioDecoder{uint64_t frames=48000;unsigned sampleRate=48000;static bool supportedExtension(const char *n){auto s=std::string(n);for(auto &c:s)c=TrackLibrary::lower(c);for(auto ext:{".wav",".mp3",".flac",".ogg"})if(s.size()>=strlen(ext)&&s.substr(s.size()-strlen(ext))==ext)return true;return false;}bool open(const char *p){++decoded;return nodes.count(p)&&nodes[p].playable;}};
static uint64_t scaleAudioFrames(uint64_t frames,unsigned from,unsigned to){return frames*to/from;}
#include "usb_music_scan.h"
static bool ready=true;
bool usbLibraryReady(){return ready;}bool usbLibraryLimited(){return folders.limited;}
int usbFolderCount(){return ready?folders.count:0;}const char *usbFolderPath(int i){return folders.paths[i];}int usbFolderParent(int i){return folders.parent[i];}
int usbTrackCount(){return ready?library.count:0;}int usbTrackFolder(int i){return library.folder[i];}const char *usbTrackName(int i){return TrackLibrary::name(library.paths[i]);}
#include "music_folder_browser.h"
static void directory(const std::string &p){nodes[p].directory=true;}static void child(const std::string &p,const std::string &name,unsigned type,bool playable=false){nodes[p].children.push_back({name,type});nodes[p+"/"+name]={type==DT_DIR,playable};}
static void reset(){assert(handles.empty());nodes.clear();library.count=0;folders.count=0;folders.limited=false;decoded=0;}
static void scan(const char *root){int id=folders.add(root,-1);bool found=false;int error=0;scanMusicFolder(id,0,found,error);assert(error==0&&handles.empty());}
int main(){
 directory("/mnt/usb0");child("/mnt/usb0","Music",DT_DIR);child("/mnt/usb0","empty",DT_DIR);child("/mnt/usb0",".hidden",DT_DIR);child("/mnt/usb0","link",DT_LNK);child("/mnt/usb0","root.WAV",DT_REG,true);
 child("/mnt/usb0/Music","zAlbum",DT_DIR);child("/mnt/usb0/Music","Album",DT_DIR);child("/mnt/usb0/Music","unknown",DT_UNKNOWN);nodes["/mnt/usb0/Music/unknown"].directory=true;
 child("/mnt/usb0/Music/Album","B.MP3",DT_REG,true);child("/mnt/usb0/Music/Album","a.flac",DT_REG,true);child("/mnt/usb0/Music/Album","bad.ogg",DT_REG,false);child("/mnt/usb0/Music/Album","cover.jpg",DT_REG,false);child("/mnt/usb0/Music/unknown","x.ogg",DT_REG,true);
 scan("/mnt/usb0");assert(library.count==4&&folders.count==6&&decoded==5);assert(!library.add("/usb0/root.WAV"));library.sort();for(int i=0;i<library.count;++i){std::string p=library.paths[i];assert(p.substr(0,p.rfind('/'))==folders.paths[library.folder[i]]);assert(library.durationMs[i]==1000);}
 directory("/mnt/usb1");scan("/mnt/usb1");MusicFolderBrowser browser;ready=false;browser.refresh();assert(browser.count==0);ready=true;browser.refresh();assert(browser.count==2);browser.apply();assert(browser.folder==0&&browser.count==3);assert(browser.entries[0].folder&&std::string(browser.name(browser.entries[0]))=="empty");browser.apply();assert(browser.count==0);assert(browser.back()&&browser.selection==0);
 browser.move(1);browser.apply();assert(std::string(browser.path())=="/mnt/usb0/Music");assert(browser.count==3&&std::string(browser.name(browser.entries[0]))=="Album");browser.apply();assert(browser.count==2&&std::string(browser.name(browser.entries[0]))=="a.flac");int play=browser.apply();assert(play>=0&&std::string(library.paths[play])=="/mnt/usb0/Music/Album/a.flac");assert(browser.back()&&browser.back()&&browser.back()&&!browser.back());assert(browser.folder==-1);browser.move(999);assert(browser.selection==1);browser.move(-999);assert(browser.selection==0);
 browser.search("B.mp3");assert(browser.searching&&browser.count==1);int result=browser.apply();assert(result>=0&&std::string(library.paths[result])=="/mnt/usb0/Music/Album/B.MP3");assert(browser.back()&&!browser.searching&&browser.folder==-1);
 browser.search("not found");assert(browser.count==0&&browser.apply()==-1);browser.search("");assert(browser.count==4);assert(browser.back());
 reset();directory("/mnt/usb0");nodes["/mnt/usb0"].corrupt=true;int root=folders.add("/mnt/usb0",-1);bool found=false;int error=0;scanMusicFolder(root,0,found,error);assert(error<0&&handles.empty());
 reset();directory("/mnt/usb0");for(int i=0;i<4200;++i)child("/mnt/usb0",std::to_string(i)+".ogg",DT_REG,true);scan("/mnt/usb0");assert(library.count==4096&&folders.limited);library.sort();for(int i=1;i<library.count;++i)assert(library.order(i-1,i)<=0);MusicFolderBrowser many;many.search("");assert(many.count==4096);many.move(99999);assert(many.selection==4095);int last=many.apply();assert(last>=0&&last<4096);many.search("409");assert(many.count>0);for(int i=0;i<many.count;++i)assert(MusicFolderBrowser::matches(many.name(many.entries[i]),"409"));
 reset();directory("/mnt/usb0");std::string p="/mnt/usb0";for(int i=0;i<20;++i){child(p,"nested",DT_DIR);p+="/nested";}scan("/mnt/usb0");assert(folders.count==13&&folders.limited);
 reset();directory("/mnt/usb0");for(int i=0;i<1100;++i)child("/mnt/usb0",std::to_string(i),DT_DIR);scan("/mnt/usb0");assert(folders.count==1024&&folders.limited);
 puts("PASS nested native scan,4formats,unknown types,hidden/link skipping,decoder rejection,alias dedup,sort-folder mapping,drive/folder/song/back selection,empty folder,malformed records,FD closure,track/folder/depth limits");
}
