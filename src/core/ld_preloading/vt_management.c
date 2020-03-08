#include "includes.h"
#include "vt_management.h"

s64 * globalCurrBurstLength;
s64 * globalCurrBBID;
s64 * globalPrevBBID;
s64 * globalCurrBBSize;
int * vtAlwaysOn;
int * vtGlobalTracerID;
int * vtGlobalTimelineID;
int * vtInitialized;



void (*vtAfterForkInChild)(int ThreadID) = NULL;
void (*vtThreadStart)(int ThreadID) = NULL;
void (*vtThreadFini)(int ThreadID) = NULL;
void (*vtAppFini)(int ThreadID) = NULL;
void (*vtMarkCurrBBL)(int ThreadID) = NULL;
void (*vtInitialize)() = NULL;
void (*vtYieldVTBurst)(int ThreadID, int save) = NULL;
void (*vtForceCompleteBurst)(int ThreadID, int save) = NULL;
void (*vtTriggerSyscallWait)(int ThreadID, int save) = NULL;
void (*vtTriggerSyscallFinish)(int ThreadID) = NULL;
void (*vtSleepForNS)(int ThreadID, s64 duration) = NULL;
void (*vtGetCurrentTimespec)(struct timespec *tp) = NULL;
void (*vtGetCurrentTimeval)(struct timeval * tv) = NULL;
s64 (*vtGetCurrentTime)() = NULL;



void load_all_vtl_functions(void * lib_vt_lib_handle) {
    if (!lib_vt_lib_handle)
        return;

    vtTriggerSyscallWait = dlsym(lib_vt_lib_handle,
                               "TriggerSyscallWait");
    if (!vtTriggerSyscallWait) {
        printf("vtTriggerSyscallWait not found !\n");
        abort();
    }

    vtTriggerSyscallFinish = dlsym(lib_vt_lib_handle,
                               "TriggerSyscallFinish");
    if (!vtTriggerSyscallFinish) {
        printf("vtTriggerSyscallFinish not found !\n");
        abort();
    }
    
    vtSleepForNS = dlsym(lib_vt_lib_handle, "SleepForNS");
    if (!vtSleepForNS) {
        printf("vtSleepForNS not found !\n");
        abort();
    }

    vtGetCurrentTimespec = dlsym(lib_vt_lib_handle, "GetCurrentTimespec");
    if (!vtGetCurrentTimespec) {
        printf("vtGetCurrentTimespec not found !\n");
        abort();
    }

    vtGetCurrentTimeval = dlsym(lib_vt_lib_handle, "GetCurrentTimeval");
    if (!vtGetCurrentTimeval) {
        printf("vtGetCurrentTimeval not found !\n");
        abort();
    }

    vtGetCurrentTime = dlsym(lib_vt_lib_handle, "GetCurrentTime");
    if (!vtGetCurrentTime) {
        printf("vtGetCurrentTime not found !\n");
        abort();
    }

    vtAfterForkInChild = dlsym(lib_vt_lib_handle, "AfterForkInChild");
    if (!vtAfterForkInChild) {
        printf("vtAfterForkInChild not found !\n");
        abort();
    }

    vtThreadStart = dlsym(lib_vt_lib_handle, "ThreadStart");
    if (!vtThreadStart) {
        printf("vtThreadStart not found !\n");
        abort();
    }

    vtThreadFini = dlsym(lib_vt_lib_handle, "ThreadFini");
    if (!vtThreadFini) {
        printf("vtThreadFini not found !\n");
        abort();
    }

    vtAppFini = dlsym(lib_vt_lib_handle, "AppFini");
    if (!vtAppFini) {
        printf("vtAppFini not found !\n");
        abort();
    }

    vtMarkCurrBBL = dlsym(lib_vt_lib_handle, "markCurrBBL");
    if (!vtMarkCurrBBL) {
        printf("vtMarkCurrBBL not found !\n");
        abort();
    }

    vtInitialize = dlsym(lib_vt_lib_handle, "initialize_vt_management");
    if (!vtInitialize) {
        printf("vtInitialize not found !\n");
        abort();
    }

    vtYieldVTBurst = dlsym(lib_vt_lib_handle, "YieldVTBurst");
    if (!vtYieldVTBurst) {
        printf("vtYieldVTBurst not found !\n");
        abort();
    }

    vtForceCompleteBurst = dlsym(lib_vt_lib_handle, "ForceCompleteBurst");
    if (!vtForceCompleteBurst) {
        printf("vtForceCompleteBurst not found !\n");
        abort();
    }
}

void initialize_vtl() {
    void * lib_vt_lib_handle;

    lib_vt_lib_handle = dlopen("libvtllogic.so", RTLD_NOLOAD | RTLD_NOW);

    if (lib_vt_lib_handle) {
        globalCurrBurstLength = dlsym(lib_vt_lib_handle, "currBurstLength");
        if (!globalCurrBurstLength) {
            printf("GLOBAL insn counter not found !\n");
            abort();
        }

        globalCurrBBID = dlsym(lib_vt_lib_handle, "currBBID");
        if (!globalCurrBBID) {
            printf("globalCurrBBID not found !\n");
            abort();
        }

        globalPrevBBID = dlsym(lib_vt_lib_handle, "prevBBID");
        if (!globalPrevBBID) {
            printf("globalPrevBBID not found !\n");
            abort();
        }

        globalCurrBBSize = dlsym(lib_vt_lib_handle, "currBBSize");
        if (!globalCurrBBSize) {
            printf("globalCurrBBSize not found !\n");
            abort();
        }

        vtAlwaysOn = dlsym(lib_vt_lib_handle, "alwaysOn");
        if (!vtAlwaysOn) {
            printf("vtAlwaysOn not found !\n");
            abort();
        }


        vtGlobalTracerID = dlsym(lib_vt_lib_handle, "globalTracerID");
        if (!vtGlobalTracerID) {
            printf("globalTracerID not found !\n");
            abort();
        }

        vtGlobalTimelineID = dlsym(lib_vt_lib_handle, "globalTimelineID");
        if (!vtGlobalTimelineID) {
            printf("globalTimelineID not found !\n");
            abort();
        }

        vtInitialized = dlsym(lib_vt_lib_handle, "vtInitializationComplete");
        if (!vtInitialized) {
            printf("vtInitializationComplete not found !");
            abort();
        }

    }

    load_all_vtl_functions(lib_vt_lib_handle);

    if(lib_vt_lib_handle && dlclose(lib_vt_lib_handle)) {
        fprintf(stderr, "can't close handle to libvt_clock.so: %s\n",
                dlerror());
        _exit(EXIT_FAILURE);
    }

    printf("Initializing VT-Management \n");
    vtInitialize();
}
