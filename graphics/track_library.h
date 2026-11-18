#pragma once
#include <stdint.h>
#include <string.h>

// Built once by the audio thread, then published as an immutable list.
class TrackLibrary
{
public:
    static constexpr int capacity = 4096;
    static constexpr int pathSize = 512;
    int count = 0;
    int folder[capacity] = {};
    uint64_t durationMs[capacity] = {};
    char paths[capacity][pathSize] = {};
    static char lower(char c) { return c >= 'A' && c <= 'Z' ? c + ('a'-'A') : c; }
    static int compare(const char *a, const char *b)
    {
        while (*a && lower(*a) == lower(*b)) { ++a; ++b; }
        return (unsigned char)lower(*a) - (unsigned char)lower(*b);
    }
    static const char *name(const char *path)
    {
        const char *result = path;
        for (const char *p = path; *p; ++p) if (*p == '/') result = p + 1;
        return result;
    }
    static const char *aliasKey(const char *path)
    {
        const char *prefix="/mnt/usb";
        unsigned i=0;
        while(prefix[i] && path[i]==prefix[i]) ++i;
        return prefix[i] ? path : path+4;
    }
    bool add(const char *path, uint64_t milliseconds = 0, int parentFolder = -1)
    {
        unsigned n=0;
        while (path[n]) { if (n >= pathSize-1) return false; ++n; }
        // Compare alias keys, but preserve the actual working path for opens.
        for (int i=0;i<count;++i)
            if (compare(aliasKey(paths[i]),aliasKey(path))==0) return false;
        if (count >= capacity) return false;
        for (unsigned i=0;i<=n;++i) paths[count][i]=path[i];
        durationMs[count] = milliseconds;folder[count]=parentFolder;
        ++count;
        return true;
    }
    int order(int a,int b)const{int v=compare(name(paths[a]),name(paths[b]));return v?v:compare(paths[a],paths[b]);}
    void swapRows(int a,int b){char temp[pathSize];memcpy(temp,paths[a],pathSize);memcpy(paths[a],paths[b],pathSize);memcpy(paths[b],temp,pathSize);auto ms=durationMs[a];durationMs[a]=durationMs[b];durationMs[b]=ms;int f=folder[a];folder[a]=folder[b];folder[b]=f;}
    void sift(int root,int end){for(int child=root*2+1;child<end;child=root*2+1){if(child+1<end && order(child,child+1)<0)++child;if(order(root,child)>=0)return;swapRows(root,child);root=child;}}
    void sort(){ // In-place O(n log n), bounded memory for larger USB libraries.
        for(int start=count/2;start>0;)sift(--start,count);
        for(int end=count-1;end>0;--end){swapRows(0,end);sift(0,end);}
    }
};
