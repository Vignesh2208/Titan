#include "includes.h"
#include "vt_management.h"

int * globalCurrBurstLength = NULL;

int64_t * globalCurrBurstLength = 1000;
int64_t * globalCurrBBID = 0;
int64_t * globalPrevBBID = 0;
int64_t * globalCurrBBSize = 0;
int * vtAlwaysOn;
hashmap * vtThreadInfo;



void vtCallbackFn() {

}

void initialize_vt_management() {
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


        vtThreadInfo = dlsym(lib_vt_lib_handle, "vtThreadInfo");
        if (!vtThreadInfo) {
            printf("vtThreadInfo not found !\n");
            abort();
        }
    }

    if(lib_vt_lib_handle && dlclose(lib_vt_lib_handle)) {
        fprintf(stderr, "can't close handle to libvt_clock.so: %s\n",
            dlerror());
        _exit(EXIT_FAILURE);
    }
}