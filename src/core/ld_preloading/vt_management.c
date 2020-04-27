#include "includes.h"
#include "vt_management.h"

s64 * globalCurrBurstLength;
s64 * globalCurrBBID;
s64 * globalCurrBBSize;
int * vtAlwaysOn;
int * vtGlobalTracerID;
int * vtGlobalTimelineID;
int * vtInitialized;



void (*vtAfterForkInChild)(int ThreadID, int ParentProcessPID) = NULL;
void (*vtThreadStart)(int ThreadID) = NULL;
void (*vtThreadFini)(int ThreadID) = NULL;
void (*vtAppFini)(int ThreadID) = NULL;
void (*vtInitialize)() = NULL;
void (*vtYieldVTBurst)(int ThreadID, int save, long syscall_number) = NULL;
void (*vtForceCompleteBurst)(int ThreadID, int save, long syscall_number) = NULL;
void (*vtTriggerSyscallWait)(int ThreadID, int save) = NULL;
void (*vtTriggerSyscallFinish)(int ThreadID) = NULL;
void (*vtSleepForNS)(int ThreadID, s64 duration) = NULL;
void (*vtGetCurrentTimespec)(struct timespec *tp) = NULL;
void (*vtGetCurrentTimeval)(struct timeval * tv) = NULL;
void (*vt_ns_2_timespec)(s64 nsec, struct timespec * ts) = NULL;
s64 (*vtGetCurrentTime)() = NULL;
void (*vtSetPktSendTime)(int payloadHash, int payloadLen, s64 send_tstamp) = NULL;

/*** For Socket Handling ***/
void  (*vtAddSocket)(int ThreadID, int sockFD, int isNonBlocking) = NULL;
int (*vtIsSocketFd)(int ThreadID, int sockFD) = NULL;
int (*vtIsSocketFdNonBlocking)(int ThreadID, int sockFD) = NULL;

/*** For TimerFD Handlng ***/
void  (*vtAddTimerFd)(int ThreadID, int fd, int isNonBlocking) = NULL;
int (*vtIsTimerFd)(int ThreadID, int fd) = NULL;
int (*vtIsTimerFdNonBlocking)(int ThreadID, int fd) = NULL;
int (*vtIsTimerArmed)(int ThreadID, int fd) = NULL;
int (*vtGetNxtTimerFdNumber)(int ThreadID) = NULL;

void (*vtSetTimerFdParams)(int ThreadID, int fd, s64 absExpiryTime,
                         s64 intervalNS, s64 relExpDuration) = NULL;

void (*vtGetTimerFdParams)(int ThreadID, int fd, s64* absExpiryTime,
                         s64* intervalNS, s64* relExpDuration) = NULL;

int (*vtGetNumNewTimerFdExpiries)(int ThreadID, int fd,
                                s64 * nxtExpiryDurationNS) = NULL;

int (*vtComputeClosestTimerExpiryForSelect)(int ThreadID,
    fd_set * readfs, int nfds, s64 * closestTimerExpiryDurationNS) = NULL;

int (*vtComputeClosestTimerExpiryForPoll)(
    int ThreadID, struct pollfd * fds, int nfds,
    s64 * closestTimerExpiryDurationNS) = NULL;


/*** For Sockets and TimerFD ***/
void (*vtCloseFd)(int ThreadID, int fd) = NULL;

/*** For lookahead handling ***/
s64 (*vtGetPacketEAT)() = NULL;
long (*vtGetBBLLookahead)() = NULL; 
int (*vtSetLookahead)(s64 bulkLookaheadValue, long spLookaheadValue) = NULL;



void load_all_vtl_functions(void * lib_vt_lib_handle) {
    if (!lib_vt_lib_handle)
        return;

    /*** For general Virtual Time Process control ops handling ***/
    vtTriggerSyscallWait = dlsym(lib_vt_lib_handle,
                               "TriggerSyscallWait");
    if (!vtTriggerSyscallWait) {
        printf("vtTriggerSyscallWait not found !\n");
        fflush(stdout); abort();
    }

    vtTriggerSyscallFinish = dlsym(lib_vt_lib_handle,
                               "TriggerSyscallFinish");
    if (!vtTriggerSyscallFinish) {
        printf("vtTriggerSyscallFinish not found !\n");
        fflush(stdout); abort();
    }
    
    vtSleepForNS = dlsym(lib_vt_lib_handle, "SleepForNS");
    if (!vtSleepForNS) {
        printf("vtSleepForNS not found !\n");
        fflush(stdout); abort();
    }

    vtGetCurrentTimespec = dlsym(lib_vt_lib_handle, "GetCurrentTimespec");
    if (!vtGetCurrentTimespec) {
        printf("vtGetCurrentTimespec not found !\n");
        fflush(stdout); abort();
    }

    vtGetCurrentTimeval = dlsym(lib_vt_lib_handle, "GetCurrentTimeval");
    if (!vtGetCurrentTimeval) {
        printf("vtGetCurrentTimeval not found !\n");
        fflush(stdout); abort();
    }

    vtGetCurrentTime = dlsym(lib_vt_lib_handle, "GetCurrentTime");
    if (!vtGetCurrentTime) {
        printf("vtGetCurrentTime not found !\n");
        fflush(stdout); abort();
    }

    vt_ns_2_timespec = dlsym(lib_vt_lib_handle, "ns_2_timespec");
    if (!vt_ns_2_timespec) {
        printf("ns_2_timespec not found !\n");
        fflush(stdout); abort();
    }

    vtAfterForkInChild = dlsym(lib_vt_lib_handle, "AfterForkInChild");
    if (!vtAfterForkInChild) {
        printf("vtAfterForkInChild not found !\n");
        fflush(stdout); abort();
    }

    vtThreadStart = dlsym(lib_vt_lib_handle, "ThreadStart");
    if (!vtThreadStart) {
        printf("vtThreadStart not found !\n");
        fflush(stdout); abort();
    }

    vtThreadFini = dlsym(lib_vt_lib_handle, "ThreadFini");
    if (!vtThreadFini) {
        printf("vtThreadFini not found !\n");
        fflush(stdout); abort();
    }

    vtAppFini = dlsym(lib_vt_lib_handle, "AppFini");
    if (!vtAppFini) {
        printf("vtAppFini not found !\n");
        fflush(stdout); abort();
    }

    vtInitialize = dlsym(lib_vt_lib_handle, "initialize_vt_management");
    if (!vtInitialize) {
        printf("vtInitialize not found !\n");
        fflush(stdout); abort();
    }

    vtYieldVTBurst = dlsym(lib_vt_lib_handle, "YieldVTBurst");
    if (!vtYieldVTBurst) {
        printf("vtYieldVTBurst not found !\n");
        fflush(stdout); abort();
    }

    vtForceCompleteBurst = dlsym(lib_vt_lib_handle, "ForceCompleteBurst");
    if (!vtForceCompleteBurst) {
        printf("vtForceCompleteBurst not found !\n");
        fflush(stdout); abort();
    }

    vtSetPktSendTime = dlsym(lib_vt_lib_handle, "SetPktSendTime");
    if (!vtSetPktSendTime) {
        printf("SetPktSendTime not found !\n");
        fflush(stdout); abort();
    }

    /*** For Socket Handling **/
    vtAddSocket = dlsym(lib_vt_lib_handle, "addSocket");
    if (!vtAddSocket) {
        printf("openSocket not found !\n");
        fflush(stdout); abort();
    }

    
    vtIsSocketFd = dlsym(lib_vt_lib_handle, "isSocketFd");
    if (!vtIsSocketFd) {
        printf("isSocketFd not found !\n");
        fflush(stdout); abort();
    }

    
    vtIsSocketFdNonBlocking = dlsym(lib_vt_lib_handle, "isSocketFdNonBlocking");
    if (!vtIsSocketFdNonBlocking) {
        printf("isSocketFdNonBlocking not found !\n");
        fflush(stdout); abort();
    }

    
    /*** For TimerFD Handling ***/
    vtAddTimerFd = dlsym(lib_vt_lib_handle, "addTimerFd");
    if (!vtAddTimerFd) {
        printf("addTimerFd not found !\n");
        fflush(stdout); abort();
    }

    
    vtIsTimerFd = dlsym(lib_vt_lib_handle, "isTimerFd");
    if (!vtIsTimerFd) {
        printf("isTimerFd not found !\n");
        fflush(stdout); abort();
    }

    

    vtIsTimerFdNonBlocking = dlsym(lib_vt_lib_handle, "isTimerFdNonBlocking");
    if (!vtIsTimerFdNonBlocking) {
        printf("isTimerFdNonBlocking not found !\n");
        fflush(stdout); abort();
    }
    
    vtIsTimerArmed = dlsym(lib_vt_lib_handle, "isTimerArmed");
    if (!vtIsTimerArmed) {
        printf("isTimerArmed not found !\n");
        fflush(stdout); abort();
    }


    vtGetNxtTimerFdNumber = dlsym(lib_vt_lib_handle, "getNxtTimerFdNumber");
    if (!vtGetNxtTimerFdNumber) {
        printf("getNxtTimerFdNumber not found !\n");
        fflush(stdout); abort();
    }


    vtSetTimerFdParams = dlsym(lib_vt_lib_handle, "setTimerFdParams");
    if (!vtSetTimerFdParams) {
        printf("setTimerFdParams not found !\n");
        fflush(stdout); abort();
    }


    vtGetTimerFdParams = dlsym(lib_vt_lib_handle, "getTimerFdParams");
    if (!vtGetTimerFdParams) {
        printf("getTimerFdParams not found !\n");
        fflush(stdout); abort();
    }


    vtGetNumNewTimerFdExpiries = dlsym(lib_vt_lib_handle,
                                        "getNumNewTimerFdExpiries");
    if (!vtGetNumNewTimerFdExpiries) {
        printf("getNumNewTimerFdExpiries not found !\n");
        fflush(stdout); abort();
    }
  

    vtComputeClosestTimerExpiryForSelect = dlsym(lib_vt_lib_handle,
                                "computeClosestTimerExpiryForSelect");
    if (!vtComputeClosestTimerExpiryForSelect) {
        printf("computeClosestTimerExpiryForSelect not found !\n");
        fflush(stdout); abort();
    }

    vtComputeClosestTimerExpiryForPoll = dlsym(lib_vt_lib_handle,
                                "computeClosestTimerExpiryForPoll");
    if (!vtComputeClosestTimerExpiryForPoll) {
        printf("computeClosestTimerExpiryForPoll not found !\n");
        fflush(stdout); abort();
    }

    vtCloseFd = dlsym(lib_vt_lib_handle, "closeFd");
    if (!vtCloseFd) {
        printf("closeSocket not found !\n");
        fflush(stdout); abort();
    }

    /*** For lookahead handling ***/
    vtGetPacketEAT = dlsym(lib_vt_lib_handle, "getPacketEAT");
    if (!vtGetPacketEAT) {
        printf("getPacketEAT not found !\n");
        fflush(stdout); abort();
    }

    vtGetBBLLookahead = dlsym(lib_vt_lib_handle, "getBBLLookahead");
    if (!vtGetBBLLookahead) {
        printf("getBBLLookahead not found !\n");
        fflush(stdout); abort();
    } 
    vtSetLookahead = dlsym(lib_vt_lib_handle, "setLookahead");
    if (!vtSetLookahead) {
        printf("setLookahead not found !\n");
        fflush(stdout); abort();
    }
}

void initialize_vtl() {
    void * lib_vt_lib_handle;

    lib_vt_lib_handle = dlopen("libvtllogic.so", RTLD_NOLOAD | RTLD_NOW);
    printf("Loading pointers to VTL Logic functions\n");
    fflush(stdout);

    if (lib_vt_lib_handle) {
        globalCurrBurstLength = dlsym(lib_vt_lib_handle, "currBurstLength");
        if (!globalCurrBurstLength) {
            printf("GLOBAL insn counter not found !\n");
            fflush(stdout); abort();
        }

        globalCurrBBID = dlsym(lib_vt_lib_handle, "currBBID");
        if (!globalCurrBBID) {
            printf("globalCurrBBID not found !\n");
            fflush(stdout); abort();
        }

        globalCurrBBSize = dlsym(lib_vt_lib_handle, "currBBSize");
        if (!globalCurrBBSize) {
            printf("globalCurrBBSize not found !\n");
            fflush(stdout); abort();
        }

        vtAlwaysOn = dlsym(lib_vt_lib_handle, "alwaysOn");
        if (!vtAlwaysOn) {
            printf("vtAlwaysOn not found !\n");
            fflush(stdout); abort();
        }


        vtGlobalTracerID = dlsym(lib_vt_lib_handle, "globalTracerID");
        if (!vtGlobalTracerID) {
            printf("globalTracerID not found !\n");
            fflush(stdout); abort();
        }

        vtGlobalTimelineID = dlsym(lib_vt_lib_handle, "globalTimelineID");
        if (!vtGlobalTimelineID) {
            printf("globalTimelineID not found !\n");
            fflush(stdout); abort();
        }

        vtInitialized = dlsym(lib_vt_lib_handle, "vtInitializationComplete");
        if (!vtInitialized) {
            printf("vtInitializationComplete not found !");
            fflush(stdout); abort();
        }

    }

    load_all_vtl_functions(lib_vt_lib_handle);


    if(lib_vt_lib_handle && dlclose(lib_vt_lib_handle)) {
        fprintf(stderr, "can't close handle to libvt_clock.so: %s\n",
                dlerror());
        _exit(EXIT_FAILURE);
    }
    printf("Loaded pointers to all VTL Logic functions\n");
    printf("Now Initializing VT-Management \n");
    fflush(stdout);
    vtInitialize();
}
