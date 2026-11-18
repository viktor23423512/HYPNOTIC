#pragma once
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <string>
#include <cstring>
#include <cstddef>
#ifndef _WIN32
#include <orbis/libkernel.h>
#endif
#ifdef _WIN32
#include <filesystem>
#endif
// Read-only bounded traversal; no links, writes or per-frame directory access.
struct PresetFolder {
    static constexpr unsigned capacity=160;
    std::string files[capacity];unsigned count=0,visited=0,directories=0;
    bool limited=false;int lastError=0;unsigned opened=0;
    static bool milk(const std::string &name){
        if(name.size()<5)return false;
        std::string ext=name.substr(name.size()-5);
        for(char &c:ext)c=(char)std::tolower((unsigned char)c);
        return ext==".milk";
    }
    static bool type(const std::string &path,bool &directory,bool &regular){
#ifdef _WIN32
        std::error_code error;auto s=std::filesystem::symlink_status(path,error);
        if(error || std::filesystem::is_symlink(s))return false;
        directory=std::filesystem::is_directory(s);regular=std::filesystem::is_regular_file(s);return true;
#else
        (void)path;(void)directory;(void)regular;return false;
#endif
    }
    void visit(const std::string &root,const std::string &name,bool directory,bool regular,unsigned depth){
        if(name=="." || name=="..")return;
        const std::string path=root+"/"+name;
        if(path.size()>768){limited=true;return;}
        if(directory){scan(path,depth+1);return;}
        if(!regular || !milk(name))return;
        for(unsigned i=0;i<count;++i)if(files[i]==path)return;
        if(count>=capacity){limited=true;return;}
        files[count++]=path;
    }
    void scan(const std::string &root,unsigned depth=0){
        if(depth>4 || visited>=4096 || directories>=256 || count>=capacity){limited=true;return;}
#ifdef _WIN32
        bool dir=false,regular=false;if(!type(root,dir,regular) || !dir)return;
        DIR *handle=opendir(root.c_str());if(!handle){lastError=errno;return;}
        ++opened;++directories;
        while(auto entry=readdir(handle)){
            if(++visited>4096 || count>=capacity){limited=true;break;}
            const std::string name=entry->d_name;
            if(!type(root+"/"+name,dir,regular))continue;
            visit(root,name,dir,regular,depth);
        }
        closedir(handle);
#else
        // Same native directory ABI used by the working HYPNOTIC music scanner.
        // Do not depend on libc opendir/lstat wrappers on the console.
        const int fd=sceKernelOpen(root.c_str(),0,0);if(fd<0){lastError=fd;return;}
        ++opened;++directories;alignas(8) char buffer[4096];bool stop=false;
        while(!stop){
            const int bytes=sceKernelGetdents(fd,buffer,sizeof(buffer));
            if(bytes<=0){if(bytes<0)lastError=bytes;break;}
            if(bytes>(int)sizeof(buffer)){lastError=-1;break;}
            size_t offset=0;
            while(offset<(size_t)bytes){
                if(++visited>4096 || count>=capacity){limited=true;stop=true;break;}
                const size_t available=bytes-offset,nameOffset=offsetof(dirent,d_name);
                if(available<nameOffset+1){lastError=-1;stop=true;break;}
                unsigned short length;memcpy(&length,buffer+offset+offsetof(dirent,d_reclen),sizeof(length));
                const unsigned size=(unsigned char)buffer[offset+offsetof(dirent,d_namlen)];
                if(length<nameOffset+1 || length>available || !size || size>=length-nameOffset || size>=sizeof(((dirent*)0)->d_name)){lastError=-1;stop=true;break;}
                const char *name=buffer+offset+nameOffset;
                if(name[size] || memchr(name,'/',size) || memchr(name,0,size)){lastError=-1;stop=true;break;}
                const unsigned kind=(unsigned char)buffer[offset+offsetof(dirent,d_type)];offset+=length;
                if(kind==DT_LNK)continue;
                if(kind==DT_UNKNOWN){limited=true;continue;}
                visit(root,std::string(name,size),kind==DT_DIR,kind==DT_REG,depth);
            }
        }
        sceKernelClose(fd);
#endif
        std::sort(files,files+count);
    }
};
