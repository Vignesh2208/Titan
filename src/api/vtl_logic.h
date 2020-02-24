#ifndef __VT_LIB_H
#define __VT_LIB_H

#include <linkedlist.h>
#include <hashmap.h>

#define EXIT_FAILURE -1

typedef struct BasicBlockStruct {
    int ID;
    int BBLSize;
    int isMarked;
    llist out_neighbours;
    long long lookahead_value;
} BasicBlock;

typedef struct ThreadStackStruct {
    long long currBBID;
    long long prevBBID;
    int currBBSize;
    long long totalBurstLength;
    int alwaysOn;
} ThreadStack;

typedef struct ThreadInfoStruct {
    int pid;
    ThreadStack stack;
    llist bbl_list;
    hashmap lookahead_map;
} ThreadInfo;

#endif