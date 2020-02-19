#include "includes.h"
#include "vt_management.h"

int64_t * globalCurrBurstLength;
int64_t * globalCurrBBID;
int64_t * globalPrevBBID;
int64_t * globalCurrBBSize;
int * vtAlwaysOn;
int * vtGlobalTracerID;
int * vtGlobalTimelineID;



void (*vtAfterForkInChild)(int ThreadID) = NULL;
void (*vtThreadStart)(int ThreadID) = NULL;
void (*vtThreadFini)(int ThreadID) = NULL;
void (*vtAppFini)(int ThreadID) = NULL;
void (*vtMarkCurrBBL)(int ThreadID) = NULL;
void (*vtInitialize)() = NULL;
void (*vtYieldVTBurst)(int ThreadID, int save) = NULL;
void (*vtForceCompleteBurst)(int ThreadID, int save) = NULL;



void load_all_vtl_functions(void * lib_vt_lib_handle) {
    if (!lib_vt_lib_handle)
        return;
    
    vtAfterForkInChild = dlsym(lib_vt_lib_handle,
                               "AfterForkInChild");
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

    vtMarkCurrBBL = dlsym(lib_vt_lib_handle, "MarkCurrBBL");
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

    lib_vt_lib_handle = dlopen("libvtlib.so", RTLD_NOLOAD | RTLD_NOW);

    if (lib_vt_lib_handle) {
        globalCurrBurstLength = dlsym(lib_vt_lib_handle,
                                      "currBurstLength");
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