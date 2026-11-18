#include "filesystem_access.h"
#include <atomic>
#include <stdio.h>
#include <orbis/libkernel.h>
#include "third_party/libjbc/libjbc.h"

static std::atomic<int> accessResult{0};
static std::atomic<int> libraryResult{0};
static std::atomic<uintptr_t> observedRoot{0}, observedPrison{0};

static bool kernelHeapPointer(uintptr_t pointer)
{
    return pointer >= 0xffff800000000000ULL && pointer < 0xffffffff00000000ULL;
}

// Upstream permits prison objects in heap OR kernel-image memory.
static bool kernelPrisonPointer(uintptr_t pointer)
{
    return kernelHeapPointer(pointer)
        || (pointer >= 0xffffffff00000000ULL && pointer < 0xfffffffffffff000ULL);
}

void prepareUsbAccess()
{
    // Do not change process privileges if native USB access already works.
    for (int i = 0; i < 8; ++i)
    {
        char path[32];
        snprintf(path, sizeof(path), "/mnt/usb%d", i);
        int fd = sceKernelOpen(path, 0, 0);
        if (fd >= 0)
        {
            sceKernelClose(fd);
            accessResult.store(1);
            return;
        }
    }
    // A read-only kernel-call probe must return a plausible thread pointer.
    // On an unsupported syscall, upstream returns an unchanged userspace value.
    if (!kernelHeapPointer(jbc_krw_get_td()))
    {
        accessResult.store(-1);
        return;
    }
    jbc_cred original = {};
    int result = jbc_get_cred(&original);
    libraryResult.store(result);
    observedRoot.store(original.rdir);
    observedPrison.store(original.prison);
    if (result != 0) { accessResult.store(-2); return; }
    if (!kernelHeapPointer(original.rdir)) { accessResult.store(-6); return; }
    if (!kernelPrisonPointer(original.prison)) { accessResult.store(-7); return; }

    jbc_cred elevated = original;
    result = jbc_jailbreak_cred(&elevated);
    libraryResult.store(result);
    observedRoot.store(elevated.rdir);
    observedPrison.store(elevated.prison);
    if (result != 0) { accessResult.store(-3); return; }
    if (!kernelHeapPointer(elevated.rdir)) { accessResult.store(-8); return; }
    if (!kernelPrisonPointer(elevated.prison)) { accessResult.store(-9); return; }

    result = jbc_set_cred(&elevated);
    libraryResult.store(result);
    if (result != 0) { accessResult.store(-4); return; }
    jbc_cred actual = {};
    result = jbc_get_cred(&actual);
    libraryResult.store(result);
    observedRoot.store(actual.rdir);
    observedPrison.store(actual.prison);
    if (result != 0) { accessResult.store(-10); return; }
    if (actual.rdir != elevated.rdir || actual.prison != elevated.prison || actual.uid != 0)
    {
        accessResult.store(-5);
        return;
    }
    // Process-scoped access lasts until application termination. No mount,
    // persistent system change, firmware patch, or USB write is performed.
    accessResult.store(2);
}

const char *usbAccessStatus()
{
    switch (accessResult.load())
    {
        case 0: return "CHECKING FILESYSTEM ACCESS";
        case 1: return "USB ACCESS ALREADY AVAILABLE";
        case 2: return "LIBJBC ACCESS APPLIED - HYPNOTIC 0.58";
        case -1: return "LIBJBC KERNEL CALL UNAVAILABLE";
        case -2: return "LIBJBC CREDENTIAL READ FAILED";
        case -3: return "LIBJBC ROOT RESOLUTION FAILED";
        case -4: return "LIBJBC CREDENTIAL APPLY FAILED";
        case -6: return "LIBJBC ORIGINAL ROOT ADDRESS REJECTED";
        case -7: return "LIBJBC ORIGINAL PRISON ADDRESS REJECTED";
        case -8: return "LIBJBC TARGET ROOT ADDRESS REJECTED";
        case -9: return "LIBJBC TARGET PRISON ADDRESS REJECTED";
        case -10: return "LIBJBC VERIFICATION READ FAILED";
        default: return "LIBJBC CREDENTIAL VERIFICATION FAILED";
    }
}

const char *usbAccessDetails()
{
    // Called by the render thread only; worker publishes scalar diagnostics atomically.
    static char text[192];
    snprintf(text, sizeof(text), "HYPNOTIC 0.58 - JBC RC %d - ROOT %016llX - PRISON %016llX",
        libraryResult.load(), (unsigned long long)observedRoot.load(),
        (unsigned long long)observedPrison.load());
    return text;
}
