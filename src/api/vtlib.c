#include <stdio.h>
#include <hashmap.h>
#include "VT_functions.h"

long long currBurstLength = 1000;
long long currBBID = 0;
long long prevBBID = 0;
long long currBBSize = 0;
int alwaysOn = 1;
hashmap thread_info;


void AfterForkInChild(int ThreadID) {

}

void ThreadStart(int ThreadID) {

}

void ThreadFini(int ThreadID) {

}

void AppFini(int ThreadID) {

}

void UpdateLookAhead(int ThreadID, int64_t prev_BBID, int64_t curr_BBID,
                     int curr_BBSize) {

}

void BasicBlockCallback(int ThreadID) {

    // set lookahead here for this Thread ...

    // call finish_burst
    currBurstLength = finish_burst();
    // load alwaysOn from this thread specific data structure

    // nxtBurstLength will be set when this returns.
}

void vtCallbackFn() {

    int ThreadID = syscall(SYS_gettid);
    
    if (alwaysOn) {
        UpdateLookAhead(ThreadID, prevBBID, currBBID, currBBSize);
    }

    if (currBurstLength <= 0) {
        BasicBlockCallback(ThreadID);
    }
    prevBBID = currBBID;
}