#include "ui_preferences.h"
#include <orbis/libkernel.h>
#include <fcntl.h>
// Only app-owned preferences are written. Never touch USB songs or presets.
static const char *path="/data/hypnotic/ui-v1.txt";
bool loadUiPreferences(VisualizerLibrary &library){
 int fd=sceKernelOpen(path,0,0);if(fd<0)return false;std::string text;char buffer[4096];bool ok=true;
 for(;;){const auto n=sceKernelRead(fd,buffer,sizeof(buffer));if((int64_t)n<0||n>sizeof(buffer)){ok=false;break;}if(!n)break;if(text.size()+n>262144){ok=false;break;}text.append(buffer,n);}sceKernelClose(fd);
 return ok&&library.decode(text);
}
bool saveUiPreferences(const VisualizerLibrary &library){
 sceKernelMkdir("/data/hypnotic",0777);const char *temporary="/data/hypnotic/ui-v1.tmp";const auto text=library.encode();
 int fd=sceKernelOpen(temporary,O_WRONLY|O_CREAT|O_TRUNC,0600);if(fd<0)return false;size_t offset=0;bool ok=true;
 while(offset<text.size()){const auto n=sceKernelWrite(fd,text.data()+offset,text.size()-offset);if((int64_t)n<=0||n>text.size()-offset){ok=false;break;}offset+=n;}
 if(ok)ok=sceKernelFsync(fd)==0;sceKernelClose(fd);if(ok)ok=sceKernelRename(temporary,path)==0;return ok;
}
