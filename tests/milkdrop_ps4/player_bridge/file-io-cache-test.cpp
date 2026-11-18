#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <algorithm>
#include <cassert>
#include <cstddef>
static bool changedRoot=false,shortRead=false;static unsigned opens=0;static int nextFd=20;static std::map<int,std::pair<std::string,size_t>> files;
static const std::string preset="original preset bytes\r\nzoom=.99\n",image="PNG-bytes";
struct MilkdropBundledAsset{const char *path;size_t bytes,offset;};
static const MilkdropBundledAsset milkdropBundledAssets[]={{"/app0/a.milk",preset.size(),0},{"/app0/a.png",image.size(),preset.size()}};
static const size_t milkdropBundledBytes=preset.size()+image.size();
static int sceKernelOpen(const char *path,int,int){++opens;std::string data;if(!strcmp(path,"/app0/a.milk")&&!changedRoot)data=preset;else if(!strcmp(path,"/app0/a.png")&&!changedRoot)data=image;else if(!strcmp(path,"/mnt/usb0/a.milk"))data=preset;else return -2;files[nextFd]={data,0};return nextFd++;}
static int sceKernelClose(int fd){assert(files.erase(fd)==1);return 0;}
static int64_t sceKernelLseek(int fd,int64_t offset,int whence){auto &f=files.at(fd);if(whence==2)return f.first.size();f.second=(size_t)offset;return offset;}
static size_t sceKernelRead(int fd,void *out,size_t capacity){auto &f=files.at(fd);if(shortRead&&f.second>0)return 0;size_t n=std::min(capacity,std::min(size_t(3),f.first.size()-f.second));memcpy(out,f.first.data()+f.second,n);f.second+=n;return n;}
#define MILKDROP_FILE_IO_TEST
#include "../../../graphics/milkdrop_file_io.cpp"
int main(){std::string error;std::vector<char> bytes;assert(!readMilkdropFile("/app0/a.milk",bytes,1024,error));assert(milkdropFilesInit(error)&&files.empty());unsigned cachedOpens=opens;changedRoot=true;
 assert(milkdropFilesInit(error)&&opens==cachedOpens);assert(readMilkdropFile("/app0/a.milk",bytes,1024,error)&&std::string(bytes.begin(),bytes.end())==preset&&opens==cachedOpens);bytes[0]='X';assert(readMilkdropFile("/app0/a.milk",bytes,1024,error)&&bytes[0]=='o');
 assert(readMilkdropFile("/app0/a.png",bytes,1024,error)&&std::string(bytes.begin(),bytes.end())==image&&opens==cachedOpens);
 assert(readMilkdropFile("/mnt/usb0/a.milk",bytes,1024,error)&&std::string(bytes.begin(),bytes.end())==preset&&files.empty());
 assert(!readMilkdropFile("/app0/../outside",bytes,1024,error));assert(!readMilkdropFile("/app0/a.milk",bytes,2,error)&&bytes.empty());shortRead=true;assert(!readMilkdropFile("/mnt/usb0/a.milk",bytes,1024,error)&&bytes.empty()&&files.empty());shortRead=false;
 assert(!readMilkdropFile("/app0/missing",bytes,1024,error));puts("PASS raw-byte package cache survives root change without later file opens; immutable cached data; bounded external imports; EOF/size/missing/traversal rejection; descriptor cleanup");}
