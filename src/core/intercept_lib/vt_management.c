#include "includes.h"
#include "vt_management.h"

s64 * globalCurrBurstCyclesLeft;

#ifndef DISABLE_LOOKAHEAD
s64 * globalCurrBBID;
#endif

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

#ifndef DISABLE_LOOKAHEAD
/*** For lookahead handling ***/
s64 (*vtGetPacketEAT)() = NULL;
long (*vtGetBBLLookahead)() = NULL; 
int (*vtSetLookahead)(s64 bulkLookaheadValue, long spLookaheadValue) = NULL;
void (*vtSetPktSendTime)(int payloadHash, int payloadLen, s64 send_tstamp) = NULL;
#endif


void LoadAllVtlFunctions(void * lib_vt_lib_handle) {
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

    vtInitialize = dlsym(lib_vt_lib_handle, "InitializeVtManagement");
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


    /*** For Socket Handling **/
    vtAddSocket = dlsym(lib_vt_lib_handle, "AddSocket");
    if (!vtAddSocket) {
        printf("openSocket not found !\n");
        fflush(stdout); abort();
    }

    
    vtIsSocketFd = dlsym(lib_vt_lib_handle, "IsSocketFd");
    if (!vtIsSocketFd) {
        printf("IsSocketFd not found !\n");
        fflush(stdout); abort();
    }

    
    vtIsSocketFdNonBlocking = dlsym(lib_vt_lib_handle, "IsSocketFdNonBlocking");
    if (!vtIsSocketFdNonBlocking) {
        printf("IsSocketFdNonBlocking not found !\n");
        fflush(stdout); abort();
    }

    
    /*** For TimerFD Handling ***/
    vtAddTimerFd = dlsym(lib_vt_lib_handle, "AddTimerFd");
    if (!vtAddTimerFd) {
        printf("AddTimerFd not found !\n");
        fflush(stdout); abort();
    }

    
    vtIsTimerFd = dlsym(lib_vt_lib_handle, "IsTimerFd");
    if (!vtIsTimerFd) {
        printf("IsTimerFd not found !\n");
        fflush(stdout); abort();
    }

    

    vtIsTimerFdNonBlocking = dlsym(lib_vt_lib_handle, "IsTimerFdNonBlocking");
    if (!vtIsTimerFdNonBlocking) {
        printf("IsTimerFdNonBlocking not found !\n");
        fflush(stdout); abort();
    }
    
    vtIsTimerArmed = dlsym(lib_vt_lib_handle, "IsTimerArmed");
    if (!vtIsTimerArmed) {
        printf("IsTimerArmed not found !\n");
        fflush(stdout); abort();
    }


    vtGetNxtTimerFdNumber = dlsym(lib_vt_lib_handle, "GetNxtTimerFdNumber");
    if (!vtGetNxtTimerFdNumber) {
        printf("GetNxtTimerFdNumber not found !\n");
        fflush(stdout); abort();
    }


    vtSetTimerFdParams = dlsym(lib_vt_lib_handle, "SetTimerFdParams");
    if (!vtSetTimerFdParams) {
        printf("SetTimerFdParams not found !\n");
        fflush(stdout); abort();
    }


    vtGetTimerFdParams = dlsym(lib_vt_lib_handle, "GetTimerFdParams");
    if (!vtGetTimerFdParams) {
        printf("GetTimerFdParams not found !\n");
        fflush(stdout); abort();
    }


    vtGetNumNewTimerFdExpiries = dlsym(lib_vt_lib_handle,
                                        "GetNumNewTimerFdExpiries");
    if (!vtGetNumNewTimerFdExpiries) {
        printf("GetNumNewTimerFdExpiries not found !\n");
        fflush(stdout); abort();
    }
  

    vtComputeClosestTimerExpiryForSelect = dlsym(lib_vt_lib_handle,
                                "ComputeClosestTimerExpiryForSelect");
    if (!vtComputeClosestTimerExpiryForSelect) {
        printf("ComputeClosestTimerExpiryForSelect not found !\n");
        fflush(stdout); abort();
    }

    vtComputeClosestTimerExpiryForPoll = dlsym(lib_vt_lib_handle,
                                "ComputeClosestTimerExpiryForPoll");
    if (!vtComputeClosestTimerExpiryForPoll) {
        printf("ComputeClosestTimerExpiryForPoll not found !\n");
        fflush(stdout); abort();
    }

    vtCloseFd = dlsym(lib_vt_lib_handle, "CloseFd");
    if (!vtCloseFd) {
        printf("closeSocket not found !\n");
        fflush(stdout); abort();
    }

    #ifndef DISABLE_LOOKAHEAD
    /*** For lookahead handling ***/
    vtSetPktSendTime = dlsym(lib_vt_lib_handle, "SetPktSendTime");
    if (!vtSetPktSendTime) {
        printf("SetPktSendTime not found !\n");
        fflush(stdout); abort();
    }

    vtGetPacketEAT = dlsym(lib_vt_lib_handle, "GetPacketEAT");
    if (!vtGetPacketEAT) {
        printf("GetPacketEAT not found !\n");
        fflush(stdout); abort();
    }

    vtGetBBLLookahead = dlsym(lib_vt_lib_handle, "getBBLLookahead");
    if (!vtGetBBLLookahead) {
        printf("getBBLLookahead not found !\n");
        fflush(stdout); abort();
    } 
    vtSetLookahead = dlsym(lib_vt_lib_handle, "SetLookahead");
    if (!vtSetLookahead) {
        printf("SetLookahead not found !\n");
        fflush(stdout); abort();
    }
    #endif
}

void InitializeVtlLogic() {
    void * lib_vt_lib_handle;

    lib_vt_lib_handle = dlopen("libvtllogic.so", RTLD_NOLOAD | RTLD_NOW);
    printf("Loading pointers to VTL Logic functions\n");
    fflush(stdout);

    if (lib_vt_lib_handle) {
        globalCurrBurstCyclesLeft = dlsym(lib_vt_lib_handle, "currBurstCyclesLeft");
        if (!globalCurrBurstCyclesLeft) {
            printf("GLOBAL insn counter not found !\n");
            fflush(stdout); abort();
        }

        #ifndef DISABLE_LOOKAHEAD
        globalCurrBBID = dlsym(lib_vt_lib_handle, "currBBID");
        if (!globalCurrBBID) {
            printf("globalCurrBBID not found !\n");
            fflush(stdout); abort();
        }
        #endif

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

    LoadAllVtlFunctions(lib_vt_lib_handle);


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
