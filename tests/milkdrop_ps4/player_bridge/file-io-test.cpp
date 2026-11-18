#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <algorithm>
#include <cassert>
static bool changedRoot=false,shortRead=false;static int nextFd=20;static std::map<int,std::pair<std::string,size_t>> files;
static const std::string data="original preset bytes\r\nzoom=.99\n";
static int sceKernelOpen(const char *path,int,int){if(!strcmp(path,"/app0"))return changedRoot?-2:7;if(!strcmp(path,"/mnt/usb0/a.milk")){files[nextFd]={data,0};return nextFd++;}return -2;}
static int openat(int anchor,const char *path,int,...){assert(anchor==7);if(!strcmp(path,"missing"))return -1;files[nextFd]={data,0};return nextFd++;}
static int sceKernelClose(int fd){assert(files.erase(fd)==1);return 0;}
static int64_t sceKernelLseek(int fd,int64_t offset,int whence){auto &f=files.at(fd);if(whence==2)return f.first.size();f.second=(size_t)offset;return offset;}
static size_t sceKernelRead(int fd,void *out,size_t capacity){auto &f=files.at(fd);if(shortRead&&f.second>0)return 0;size_t n=std::min(capacity,std::min(size_t(3),f.first.size()-f.second));memcpy(out,f.first.data()+f.second,n);f.second+=n;return n;}
#define MILKDROP_FILE_IO_TEST
#include "../../../graphics/milkdrop_file_io.cpp"
int main(){std::string error;assert(milkdropFilesInit(error));changedRoot=true;std::vector<char> bytes;
 assert(readMilkdropFile("/app0/assets/a.milk",bytes,1024,error)&&std::string(bytes.begin(),bytes.end())==data&&files.empty());
 assert(readMilkdropFile("/mnt/usb0/a.milk",bytes,1024,error)&&std::string(bytes.begin(),bytes.end())==data&&files.empty());
 assert(!readMilkdropFile("/app0/../outside",bytes,1024,error)&&files.empty());
 assert(!readMilkdropFile("/app0/assets/a.milk",bytes,2,error)&&bytes.empty()&&files.empty());
 shortRead=true;assert(!readMilkdropFile("/app0/assets/a.milk",bytes,1024,error)&&bytes.empty()&&files.empty());shortRead=false;
 assert(!readMilkdropFile("/app0/missing",bytes,1024,error)&&files.empty());
 puts("PASS package anchor survives root change; external native read; exact bytes; partial reads; traversal/size/missing-file rejection; descriptors closed");}
