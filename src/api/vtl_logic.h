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

typedef struct BasicBlockStruct {
    int ID;
    int BBLSize;
    int isMarked;
    llist out_neighbours;
    long long lookahead_value;
} BasicBlock;

typedef struct fdInfoStruct {
    int fd;
    int fdType;
    int isNonBlocking;

    // Following fields only used if fdType is FD_TYPE_TIMERFD
    s64 absExpiryTime;
    s64 intervalNS;
    int numExpiriesProcessed;

} fdInfo;

typedef struct ThreadStackStruct {
    long long currBBID;
    long long prevBBID;
    int currBBSize;
    long long totalBurstLength;
    int alwaysOn;
} ThreadStack;

typedef struct ThreadInfoStruct {
    int pid;
    int yielded;
    int in_force_completed;
    int in_callback;
    ThreadStack stack;
    llist bbl_list;
    llist special_fds;
    hashmap lookahead_map;
} ThreadInfo;

s64 GetCurrentTime();

#endif
