#include <orbis/libkernel.h>
#include <cstdio>
static bool nativeUsbReady=false;
void prepareUsbAccess(){
    nativeUsbReady=false;
    for(unsigned i=0;i<8;++i){
        char path[32];snprintf(path,sizeof(path),"/mnt/usb%u",i);
        const int fd=sceKernelOpen(path,0,0);
        if(fd>=0){sceKernelClose(fd);nativeUsbReady=true;}
    }
    printf("MILKDROP IMPORT NATIVE USB ACCESS %s - NO CREDENTIAL CHANGES\n",nativeUsbReady?"READY":"UNAVAILABLE");
}
const char *usbAccessStatus(){return nativeUsbReady?"NATIVE USB ACCESS READY":"USB NOT EXPOSED - BUNDLED PRESETS STILL AVAILABLE";}
