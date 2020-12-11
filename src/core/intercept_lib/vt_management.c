#include "includes.h"
#include "vt_management.h"

s64 * globalCurrBurstCyclesLeft;

#ifndef DISABLE_LOOKAHEAD
s64 * globalCurrBBID;
#endif

int * vtGlobalTracerID;
int * vtGlobalTimelineID;
int * vtInitialized;


#ifndef DISABLE_VT_SOCKET_LAYER
void (*vtAfterForkInChild)(int ThreadID, int ParendProcessPID, pthread_t stackThread) = NULL;
#else
void (*vtAfterForkInChild)(int ThreadID, int ParendProcessPID) = NULL;
#endif
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
void  (*vtAddSocket)(int ThreadID, int sockFD, int sockFdProtoType, int isNonBlocking) = NULL;
int (*vtIsSocketFd)(int ThreadID, int sockFD) = NULL;
int (*vtIsTCPSocket)(int ThreadID, int sockFD) = NULL;
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

/** Virtual TCP socket layer ***/
int (*_vsocket)(int domain, int type, int protocol) = NULL;
int (*_vconnect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen) = NULL;
int (*_vwrite)(int sockfd, const void *buf, const unsigned int count, int *did_block) = NULL;
int (*_vread)(int sockfd, void *buf, const unsigned int count, int *did_block) = NULL;
int (*_vclose)(int sockfd) = NULL;
int (*_vpoll)(struct pollfd fds[], nfds_t nfds) = NULL;
int (*_vgetsockopt)(int fd, int level, int optname, void *optval, socklen_t *optlen) = NULL;
int (*_vsetsockopt)(int sockfd, int level, int option_name,
                    const void *option_value, socklen_t option_len) = NULL;
int (*_vgetpeername)(int socket, struct sockaddr *restrict addr,
                    socklen_t *restrict address_len) = NULL;
int (*_vgetsockname)(int socket, struct sockaddr *restrict addr,
                    socklen_t *restrict address_len) = NULL;
int (*_vlisten)(int socket, int backlog) = NULL;
int (*_vbind)(int socket, struct sockaddr *skaddr) = NULL;
int (*_vaccept)(int socket, struct sockaddr *skaddr) = NULL;


#ifndef DISABLE_VT_SOCKET_LAYER
int (*vtIsNetDevInCallback)() = NULL;
void (*vtRunTCPStackRxLoop)() = NULL;
void (*vtInitializeTCPStack)(int stackID) = NULL;
void (*vtMarkTCPStackActive)() = NULL;
void (*vtMarkTCPStackInactive)() = NULL;
int (*vtHandleReadSyscall)(int ThreadID, int fd, void *buf, int count, int *redirect)   = NULL;
int (*vtHandleWriteSyscall)(int ThreadID, int fd, const void *buf, int count, int * redirect) = NULL;
void * (*vtStackThread)(void *arg) = NULL;
#endif

#ifndef DISABLE_LOOKAHEAD
/*** For lookahead handling ***/
s64 (*vtGetPacketEAT)() = NULL;
long (*vtGetBBLLookahead)(long bblNumber) = NULL; 
int (*vtSetLookahead)(s64 bulkLookaheadValue, int lookahead_anchor_type) = NULL;
void (*vtSetPktSendTime)(int payloadHash, int payloadLen, s64 send_tstamp) = NULL;
#endif


void LoadAllVtSocketLayerFunctions(void * lib_vt_lib_handle) {
    if (!lib_vt_lib_handle)
        return;

    _vsocket = dlsym(lib_vt_lib_handle, "_socket");
    if (!_vsocket) {
        printf("_socket not found !\n");
        fflush(stdout); abort();
    }

    _vconnect = dlsym(lib_vt_lib_handle, "_connect");
    if (!_vconnect) {
        printf("_connect not found !\n");
        fflush(stdout); abort();
    }

    _vwrite = dlsym(lib_vt_lib_handle, "_write");
    if (!_vwrite) {
        printf("_write not found !\n");
        fflush(stdout); abort();
    }

    _vread = dlsym(lib_vt_lib_handle, "_read");
    if (!_vread) {
        printf("_read not found !\n");
        fflush(stdout); abort();
    }

    _vclose = dlsym(lib_vt_lib_handle, "_close");
    if (!_vclose) {
        printf("_close not found !\n");
        fflush(stdout); abort();
    }

    _vpoll = dlsym(lib_vt_lib_handle, "_poll");
    if (!_vpoll) {
        printf("_poll not found !\n");
        fflush(stdout); abort();
    }

    _vgetsockopt = dlsym(lib_vt_lib_handle, "_getsockopt");
    if (!_vgetsockopt) {
        printf("_getsockopt not found !\n");
        fflush(stdout); abort();
    }

    _vsetsockopt = dlsym(lib_vt_lib_handle, "_setsockopt");
    if (!_vsetsockopt) {
        printf("_setsockopt not found !\n");
        fflush(stdout); abort();
    }

    _vgetpeername = dlsym(lib_vt_lib_handle, "_getpeername");
    if (!_vgetpeername) {
        printf("_getpeername not found !\n");
        fflush(stdout); abort();
    }

    _vgetsockname = dlsym(lib_vt_lib_handle, "_getsockname");
    if (!_vgetsockname) {
        printf("_getsockname not found !\n");
        fflush(stdout); abort();
    }

    _vlisten = dlsym(lib_vt_lib_handle, "_listen");
    if (!_vlisten) {
        printf("_listen not found !\n");
        fflush(stdout); abort();
    }

    _vbind = dlsym(lib_vt_lib_handle, "_bind");
    if (!_vbind) {
        printf("_bind not found !\n");
        fflush(stdout); abort();
    }

    _vaccept = dlsym(lib_vt_lib_handle, "_accept");
    if (!_vaccept) {
        printf("_accept not found !\n");
        fflush(stdout); abort();
    }


    #ifndef DISABLE_VT_SOCKET_LAYER
    vtIsNetDevInCallback = dlsym(lib_vt_lib_handle, "IsNetDevInCallback");
    if (!vtIsNetDevInCallback) {
        printf("IsNetDevInCallback not found !\n");
        fflush(stdout); abort();
    }

    vtRunTCPStackRxLoop = dlsym(lib_vt_lib_handle, "RunTCPStackRxLoop");
    if (!vtRunTCPStackRxLoop) {
        printf("RunTCPStackRxLoop not found !\n");
        fflush(stdout); abort();
    }

    vtInitializeTCPStack = dlsym(lib_vt_lib_handle, "InitializeTCPStack");
    if (!vtInitializeTCPStack) {
        printf("InitializeTCPStack not found !\n");
        fflush(stdout); abort();
    }

    vtMarkTCPStackActive = dlsym(lib_vt_lib_handle, "MarkTCPStackActive");
    if (!vtMarkTCPStackActive) {
        printf("MarkTCPStackActive not found !\n");
        fflush(stdout); abort();
    }

    vtMarkTCPStackInactive = dlsym(lib_vt_lib_handle, "MarkTCPStackInactive");
    if (!vtMarkTCPStackInactive) {
        printf("MarkTCPStackInactive not found !\n");
        fflush(stdout); abort();
    }

    vtHandleReadSyscall = dlsym(lib_vt_lib_handle, "HandleReadSyscall");
    if (!vtHandleReadSyscall) {
        printf("HandleReadSyscall not found !\n");
        fflush(stdout); abort();
    }

    vtHandleWriteSyscall = dlsym(lib_vt_lib_handle, "HandleWriteSyscall");
    if (!vtHandleWriteSyscall) {
        printf("HandleWriteSyscall not found !\n");
        fflush(stdout); abort();
    }

    vtStackThread = dlsym(lib_vt_lib_handle, "StackThread");
    if (!vtStackThread) {
        printf("StackThread not found !\n");
        fflush(stdout); abort();
    }

    #endif
}

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

    vtIsTCPSocket = dlsym(lib_vt_lib_handle, "IsTCPSocket");
    if (!vtIsTCPSocket) {
        printf ("IsTCPSocket not found !\n");
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


    LoadAllVtSocketLayerFunctions(lib_vt_lib_handle);

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

    vtGetBBLLookahead = dlsym(lib_vt_lib_handle, "GetBBLLookAhead");
    if (!vtGetBBLLookahead) {
        printf("GetBBLLookAhead not found !\n");
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
