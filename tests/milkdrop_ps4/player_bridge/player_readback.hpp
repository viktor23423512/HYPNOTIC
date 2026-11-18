#pragma once
// Exact-byte DMA readback only: no image processing or preset changes.
static unsigned char *playerReadback=nullptr;
static bool playerReadbackReady=false,playerReadbackRequested=false;
static constexpr size_t playerReadbackCapacity=16*1024*1024;
static void invalidatePlayerReadback(size_t bytes){
 for(size_t at=0;at<bytes;at+=64)__asm__ __volatile__("clflush (%0)"::"r"(playerReadback+at):"memory");
 __atomic_thread_fence(__ATOMIC_SEQ_CST);
}
static const unsigned char *copyPlayerReadback(const unsigned char *source,size_t bytes){
 if(!playerReadback || !source || !bytes || bytes>playerReadbackCapacity || (bytes&3))fail("MILKDROP READBACK BOUNDS",-1);
 // Flush previous CPU cache lines before the GPU overwrites this buffer.
 invalidatePlayerReadback(bytes);
 auto command=sceGnmCmdInit(geometryCommands,geometryCommandBytes,nullptr,nullptr);
 sceGnmDrawCmdWaitGraphicsWrite(&command,GNM_ACQUIRE_TARGET_CB0);
 if(!sceGnmDrawCmdCopyMemory(&command,(uint64_t)(uintptr_t)playerReadback,(uint64_t)(uintptr_t)source,(uint32_t)bytes))fail("MILKDROP READBACK COMMAND",-1);
 finishGeometryCommands(command,1000);
 invalidatePlayerReadback(bytes);
 return playerReadback;
}
static void initializePlayerReadback(){
 off_t offset=0;int rc=sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),playerReadbackCapacity,0x4000,ORBIS_KERNEL_WB_ONION,&offset);
 if(rc){printf("HYPNOTIC MILKDROP READBACK ALLOC FALLBACK %08X\n",(unsigned)rc);return;}
 void *mapped=nullptr;
 rc=sceKernelMapDirectMemory(&mapped,playerReadbackCapacity,ORBIS_KERNEL_PROT_CPU_READ|ORBIS_KERNEL_PROT_CPU_RW|ORBIS_KERNEL_PROT_GPU_READ|ORBIS_KERNEL_PROT_GPU_WRITE,0,offset,0x4000);
 if(rc){sceKernelReleaseDirectMemory(offset,playerReadbackCapacity);printf("HYPNOTIC MILKDROP READBACK MAP FALLBACK %08X\n",(unsigned)rc);return;}
 playerReadback=(unsigned char*)mapped;
 // Both feedback surfaces contain different self-test images here. Alternate
 // them twice to detect stale CPU cache data as well as byte/layout errors.
 for(unsigned pass=0;pass<4;++pass){auto &s=feedback[pass&1];const size_t bytes=s.pitch*height*4;
  const auto copied=copyPlayerReadback(s.data,bytes);
  if(memcmp(copied,s.data,bytes)){printf("HYPNOTIC MILKDROP READBACK BYTE CHECK FAIL %u - DIRECT FALLBACK\n",pass);return;}
 }
 playerReadbackReady=true;printf("HYPNOTIC MILKDROP READBACK BYTE CHECK PASS - CPU CACHED DMA\n");
}
void milkdropPlayerSetReadback(bool enabled){playerReadbackRequested=enabled;}
bool milkdropPlayerReadbackReady(){return playerReadbackReady;}
