#ifndef __VT_LIB_H
#define __VT_LIB_H

#include "utils/linkedlist.h"
#include "utils/hashmap.h"
#include "utility_functions.h"
#include "cache_simulation.h"
#include <pthread.h>



#define EXIT_FAILURE -1

#define FALSE 0
#define TRUE 1
#define MAX_LOOP_DEPTH 10

#define FD_TYPE_DEFAULT 0x0
#define FD_TYPE_SOCKET 0x1
#define FD_TYPE_TIMERFD 0x2
#define MAX_ASSIGNABLE_FD 1024

#define FD_PROTO_TYPE_TCP 0x1
#define FD_PROTO_TYPE_OTHER 0x2
#define FD_PROTO_TYPE_NONE 0

typedef long long s64;
typedef unsigned long long u64;

typedef struct fdInfoStruct {
    int fd;
    int fdType;
    int isNonBlocking;

    // Used only if fdType is FD_TYPE_SOCKET
    int fdProtoType;

    // Following fields only used if fdType is FD_TYPE_TIMERFD
    s64 relativeExpiryDuration;
    s64 absExpiryTime;
    s64 intervalNS;
    int numExpiriesProcessed;

} fdInfo;

typedef struct ThreadStackStruct {
    #ifndef DISABLE_LOOKAHEAD
    s64 currBBID;
    #endif
    s64 totalBurstLength;
} ThreadStack;

typedef struct LoopRunTimeStruct {
    int entered;
    s64 numIterations;
} LoopRunTime;

typedef struct ThreadInfoStruct {
    int pid;
    int yielded;
    int in_force_completed;
    int in_callback;
    int processPID; // pid of the holder process. May differ from pid if this is a Thread
    #ifndef DISABLE_VT_SOCKET_LAYER
    pthread_t stackThread;
    #endif
    char * fpuState;
    #ifndef DISABLE_LOOKAHEAD
    LoopRunTime loopRunTime[MAX_LOOP_DEPTH];
    #endif
    ThreadStack stack;
    llist special_fds;
} ThreadInfo;



s64 GetCurrentTime();
void HandleVTExpEnd(int ThreadID);

void TriggerSyscallWait(int ThreadID, int save);

/*** For Socket Handling ***/
void AddSocket(int ThreadID, int sockFD, int sockFdProtoType, int isNonBlocking);
int IsSocketFd(int ThreadID, int sockFD);
int IsTCPSocket(int ThreadID, int sockFD);
int IsSocketFdNonBlocking(int ThreadID, int sockFD);

/*** For TimerFd Handlng ***/
void  AddTimerFd(int ThreadID, int fd, int isNonBlocking);
int IsTimerFd(int ThreadID, int fd);
int IsTimerFdNonBlocking(int ThreadID, int fd);
int IsTimerArmed(int ThreadID, int fd);

/*** For virtual socket layer handling ***/
#ifndef DISABLE_VT_SOCKET_LAYER
void InitializeTCPStack(int stackID);
void MarkTCPStackActive();
void RunTCPStackRxLoop();
void MarkTCPStackInactive();
int HandleReadSyscall(int ThreadID, int fd, void *buf, int count, int *redirect);
int HandleWriteSyscall(int ThreadID, int fd, const void *buf, int count, int * redirect);
#endif

#ifndef DISABLE_LOOKAHEAD
/*** For setting lookaheads ***/
int SetLookahead(s64 bulkLookaheadValue, int  lookahead_anchor_type);
#endif



#endif
