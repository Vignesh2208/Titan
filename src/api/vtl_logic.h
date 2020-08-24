#ifndef __VT_LIB_H
#define __VT_LIB_H

#include "utils/linkedlist.h"
#include "utils/hashmap.h"
#include "utility_functions.h"


#define EXIT_FAILURE -1

#define FALSE 0
#define TRUE 1

#define FD_TYPE_DEFAULT 0x0
#define FD_TYPE_SOCKET 0x1
#define FD_TYPE_TIMERFD 0x2
#define MAX_ASSIGNABLE_FD 1024

typedef long long s64;

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
    long long currBBID;
    int currBBSize;
    long long totalBurstLength;
    int alwaysOn;
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

/*** For setting lookaheads ***/
int SetLookahead(s64 bulkLookaheadValue, long spLookaheadValue);

/*** InsnCache callback ***/
void insCacheCallback();

/*** Invoked before read accesses to memory ***/
void dataReadCacheCallback(unsigned long long address, int size_bytes);

/*** Invoked before write accesses to memory ***/
void dataWriteCacheCallback(unsigned long long address, int size_bytes);

#endif
