#ifndef __VT_LIB_H
#define __VT_LIB_H

#include "utils/linkedlist.h"
#include "utils/hashmap.h"
#include "utility_functions.h"
#include "cache_simulation.h"


#define EXIT_FAILURE -1

#define FALSE 0
#define TRUE 1

#define FD_TYPE_DEFAULT 0x0
#define FD_TYPE_SOCKET 0x1
#define FD_TYPE_TIMERFD 0x2
#define MAX_ASSIGNABLE_FD 1024

typedef long long s64;
typedef unsigned long long u64;

typedef struct fdInfoStruct {
    int fd;
    int fdType;
    int isNonBlocking;

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

typedef struct ThreadInfoStruct {
    int pid;
    int yielded;
    int in_force_completed;
    int in_callback;
    int processPID; // pid of the holder process. May differ from pid if this is a Thread
    ThreadStack stack;
    llist special_fds;
} ThreadInfo;

s64 GetCurrentTime();
void HandleVTExpEnd(int ThreadID);

/*** For Socket Handling ***/
void AddSocket(int ThreadID, int sockFD, int isNonBlocking);
int IsSocketFd(int ThreadID, int sockFD);
int IsSocketFdNonBlocking(int ThreadID, int sockFD);

/*** For TimerFd Handlng ***/
void  AddTimerFd(int ThreadID, int fd, int isNonBlocking);
int IsTimerFd(int ThreadID, int fd);
int IsTimerFdNonBlocking(int ThreadID, int fd);
int IsTimerArmed(int ThreadID, int fd);

#ifndef DISABLE_LOOKAHEAD
/*** For setting lookaheads ***/
int SetLookahead(s64 bulkLookaheadValue, long spLookaheadValue);
#endif



#endif
