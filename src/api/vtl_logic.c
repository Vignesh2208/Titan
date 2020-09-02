#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "VT_functions.h"
#include "utility_functions.h"
#include "vtl_logic.h"
#include "fd_handling.h"
#ifndef DISABLE_LOOKAHEAD
#include "lookahead_parsing.h"
#endif


s64 currBurstLength = 0;
s64 specifiedBurstLength = 0;

#ifndef DISABLE_LOOKAHEAD
s64 currBBID = 0;
s64 currLoopID = 0;
struct lookahead_map * bblLookAheadMap = NULL;
struct lookahead_map * loopLookAheadMap = NULL;
#endif

#ifndef DISABLE_INSN_CACHE_SIM
s64 currBBInsCacheMissPenalty = 0;
#endif

int expStopping = 0;
int vtInitializationComplete = 0;
int globalTracerID = -1;
int globalTimelineID = -1;
int globalThreadID = 0;
hashmap thread_info_map;
llist thread_info_list;

#define FLIP_ALWAYS_ON_THRESHOLD 100000000


extern void ns_2_timespec(s64 nsec, struct timespec * ts);
extern void ns_2_timeval(s64 nsec, struct timeval * tv);

#ifndef DISABLE_LOOKAHEAD
//! Returns loop lookahead for a specific loop number
s64 GetLoopLookAhead(long loopNumber) {
    if (!loopLookAheadMap ||
        (loopNumber < loopLookAheadMap->start_offset) ||
        (loopNumber > loopLookAheadMap->finish_offset))
        return 0;
    return loopLookAheadMap->lookaheads[loopNumber - loopLookAheadMap->start_offset];
}

//! Returns lookahead from a specific basic block
s64 GetBBLLookAhead(long bblNumber) {
    if (!bblLookAheadMap ||
        (bblNumber < bblLookAheadMap->start_offset) ||
        (bblNumber > bblLookAheadMap->finish_offset))
        return 0;
    return bblLookAheadMap->lookaheads[bblNumber - bblLookAheadMap->start_offset];
}

//! Sets the lookahead for a specific loop by estimating the number of iterations it would make
void SetLoopLookahead(int initValue, int finalValue, int stepValue) {
	printf("SetLoopLookahead, return address: %p, currBBID: %llu, currLoopID: %llu\n",
        __builtin_return_address(0), currBBID, currLoopID);
	printf("Called SetLoopLookahead: init: %d, final: %d, step: %d\n",
		initValue, finalValue, stepValue);
    if (!vtInitializationComplete)
        return;
    int numIterations = 0;
    if ((finalValue > initValue && stepValue > 0) ||
        (finalValue < initValue && stepValue < 0))
        numIterations = (finalValue - initValue)/stepValue;
    s64 loop_lookahead = GetLoopLookAhead((long)currLoopID)*numIterations;
    SetLookahead(loop_lookahead, 0);
}

//! Sets the lookahead at a specific basic block
void SetBBLLookAhead(long bblNumber) {
    if (!vtInitializationComplete)
        return;
    SetLookahead(0, GetBBLLookAhead(bblNumber));
}
#endif

#ifndef DISABLE_INSN_CACHE_SIM
//! Instruction Cache Callback function
void insCacheCallback() {
    // currBBCacheMissPenalty would be set by the compiler.
    // if there is a cache miss, currBBCacheMissPenalty would indicate the
    // size in bytes of the basic block. This is converted to number of cache
    // lines to fill

}
#endif 

#ifndef DISABLE_DATA_CACHE_SIM
//! Invoked before read accesses to memory. The read memory address is passed
// as argument. The size of read memory in bytes is also provided. This
// function is invoked before instructions which read bytes, words, or
// double words from memory
void dataReadCacheCallback(u64 address, int size_bytes) {
    printf("DataReadCacheCallback: address: %p, size: %d\n", (void *)address,
        size_bytes);
}

//! Invoked before write accesses to memory. The write memory address is passed
// as argument. The size of written memory in bytes is also provided. This
// function is invoked before instructions which write to bytes, words, or
// double words to memory
void dataWriteCacheCallback(u64 address, int size_bytes) {
    printf("DataWriteCacheCallback: address: %p, size: %d\n", (void *)address,
        size_bytes);
}
#endif


void CopyAllSpecialFds(int FromProcessID, int ToProcessID) {
    ThreadInfo * fromProcess = hmap_get_abs(&thread_info_map, FromProcessID);
    ThreadInfo * toProcess = hmap_get_abs(&thread_info_map, ToProcessID);
    assert(toProcess);

    if (!fromProcess)
        return;

    llist_elem * head;
    fdInfo * currFdInfo;
    fdInfo * currFdCopy;
	head = fromProcess->special_fds.head;
	while (head != NULL) {
        currFdInfo = (fdInfo *)head->item;

        if (currFdInfo) {
            currFdCopy = (fdInfo *)malloc(sizeof(fdInfo));
            memcpy(currFdCopy, currFdInfo, sizeof(fdInfo));
            llist_append(&fromProcess->special_fds, currFdCopy);
        }
		head = head->next;
	}

}

//! Makes the specific thread sleep for a duration (nanoseconds) in virtual time
void SleepForNS(int ThreadID, int64_t duration) {

    if (duration <= 0)
        return;
	
    int ret;
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;


    currThreadInfo->in_callback = TRUE;

    #ifndef DISABLE_LOOKAHEAD
    currThreadInfo->stack.currBBID = currBBID;
    #endif

    ret = VtSleepFor(duration);

    if (ret < 0) HandleVTExpEnd(ThreadID);

    currBurstLength = MarkBurstComplete(1);
    specifiedBurstLength = currBurstLength;
    if (currBurstLength <= 0) HandleVTExpEnd(ThreadID);

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    #ifndef DISABLE_LOOKAHEAD
    currBBID = currThreadInfo->stack.currBBID;
    #endif
    currThreadInfo->in_callback = FALSE;
}

//! Returns current virtual time as timespec
void GetCurrentTimespec(struct timespec *ts) {
    ns_2_timespec(GetCurrentVtTime(), ts);
}

//! Returns current virtual time as timeval
void GetCurrentTimeval(struct timeval * tv) {
    ns_2_timeval(GetCurrentVtTime(), tv);
}

//! Returns current virtual time as nanoseconds
s64 GetCurrentTime() {
    if (currBurstLength > 0 && specifiedBurstLength > 0)
    	return GetCurrentVtTime() + specifiedBurstLength - currBurstLength;
    else
	    return GetCurrentVtTime();
}

#ifndef DISABLE_LOOKAHEAD
//! Sets packet send time for a specific packet
void SetPktSendTime(int payloadHash, int payloadLen, s64 pktSendTimeStamp) {
	SetPktSendTimeAPI(payloadHash, payloadLen, pktSendTimeStamp);
 
}

//! Returns earliest arrival time of any packet intended for this node 
s64 GetPacketEAT() {
    return GetEarliestArrivalTime();
}

//! Common wrapper to set a lookahead for this node
int SetLookahead(s64 bulkLookaheadValue, long spLookaheadValue) {
    if (bulkLookaheadValue || spLookaheadValue)
        return SetProcessLookahead(bulkLookaheadValue, spLookaheadValue);
    return 0;
}
#endif


//! Called when the virtual time experiment is about to end
void HandleVTExpEnd(int ThreadID) {
    printf("Process: %d exiting VT experiment !\n", ThreadID);
    expStopping = 1;

    #ifndef DISABLE_LOOKAHEAD
    if (loopLookAheadMap) {
        free(loopLookAheadMap->lookaheads);
        free(loopLookAheadMap);
        loopLookAheadMap = NULL;
    }

    if (bblLookAheadMap) {
        free(bblLookAheadMap->lookaheads);
        free(bblLookAheadMap);
        bblLookAheadMap = NULL;
    }
    #endif

    exit(0);
}

//! Deallocates thread specific information
void CleanupThreadInfo(ThreadInfo * relevantThreadInfo) {
    if (!relevantThreadInfo)
        return;


    while(llist_size(&relevantThreadInfo->special_fds)){
        fdInfo * currfdInfo = llist_pop(
            &relevantThreadInfo->special_fds);
        if (currfdInfo)
            free(currfdInfo);
    }

    llist_destroy(&relevantThreadInfo->special_fds);
	
}

//! Allots thread specific information
ThreadInfo * AllotThreadInfo(int ThreadID, int processPID) {
    ThreadInfo * currThreadInfo = NULL;
    if (!hmap_get_abs(&thread_info_map, ThreadID)) {
        currThreadInfo = (ThreadInfo *) malloc(sizeof(ThreadInfo));
        assert(currThreadInfo != NULL);
        currThreadInfo->pid = ThreadID;
        currThreadInfo->processPID = processPID;
        currThreadInfo->yielded = 0;
        currThreadInfo->in_force_completed = 0;
        currThreadInfo->in_callback = 0;
        currThreadInfo->stack.totalBurstLength = 0;

        #ifndef DISABLE_LOOKAHEAD
        currThreadInfo->stack.currBBID = 0;
        #endif

        llist_init(&currThreadInfo->special_fds);
        hmap_put_abs(&thread_info_map, ThreadID, currThreadInfo);
        llist_append(&thread_info_list, currThreadInfo);
    } else {
        return hmap_get_abs(&thread_info_map, ThreadID);
    }

    return currThreadInfo;
}

//! Called by a thread when it is about to yield its CPU
void YieldVTBurst(int ThreadID, int save, long syscall_number) {


    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo)  return;

    if (currThreadInfo->in_callback) return;


    if (currThreadInfo->in_force_completed) return;
    
    currThreadInfo->in_callback = TRUE;

    if (currThreadInfo->yielded == 0) {
	    currThreadInfo->yielded = TRUE;

        #ifndef DISABLE_LOOKAHEAD
	    if (save)
            currThreadInfo->stack.currBBID = currBBID;
        #endif

	    ReleaseWorker();
            
    }
    currThreadInfo->in_callback = FALSE;
	
}

//! Called by a thread to indicate that it has completed its previous execution burst
void ForceCompleteBurst(int ThreadID, int save, long syscall_number) {
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;

    if (currThreadInfo->in_force_completed) return;

    currThreadInfo->in_force_completed = TRUE;

    #ifndef DISABLE_LOOKAHEAD
    if (save && currThreadInfo)
        currThreadInfo->stack.currBBID = currBBID; 
    #endif

    currBurstLength = MarkBurstComplete(0);
    specifiedBurstLength = currBurstLength;

    if (currBurstLength <= 0)
        HandleVTExpEnd(ThreadID);

    if (currThreadInfo) {

    	currThreadInfo->stack.totalBurstLength += currBurstLength;

        #ifndef DISABLE_LOOKAHEAD
    	// restore globals
    	currBBID = currThreadInfo->stack.currBBID;
        #endif
    }

    if (currThreadInfo->yielded == TRUE)
    	currThreadInfo->yielded = FALSE;

    if (currThreadInfo->in_force_completed == TRUE)
    	currThreadInfo->in_force_completed = FALSE;
}


//! Called by a thread to indicate that it has completed its previous execution burst
void SignalBurstCompletion(ThreadInfo * currThreadInfo, int save) {

    
    #ifndef DISABLE_LOOKAHEAD
    // save globals
    if (save) currThreadInfo->stack.currBBID = currBBID;
    #endif

    currBurstLength = FinishBurst();
    specifiedBurstLength = currBurstLength;

    if (currBurstLength <= 0) HandleVTExpEnd(currThreadInfo->pid);

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    #ifndef DISABLE_LOOKAHEAD
    // restore globals
    currBBID = currThreadInfo->stack.currBBID;
    #endif
}


//! Called in the forked child process as soon as it starts
void AfterForkInChild(int ThreadID, int ParentProcessID) {

    // destroy existing hmap
    ThreadInfo * currThreadInfo;
    ThreadInfo * tmp;


    currThreadInfo = AllotThreadInfo(ThreadID, ThreadID);     
    assert(currThreadInfo != NULL);
    currBurstLength = 0;

    #ifndef DISABLE_LOOKAHEAD
    currBBID = 0;
    #endif

    specifiedBurstLength = currBurstLength;

    currThreadInfo->in_callback = TRUE; 

    printf("Adding new child process\n");
    fflush(stdout);

    AddToTracerSchQueue(globalTracerID);
    globalThreadID = syscall(SYS_gettid);
    SignalBurstCompletion(currThreadInfo, 1);

    if (ParentProcessID != ThreadID) {
        CopyAllSpecialFds(ParentProcessID, ThreadID);
    }

    while(llist_size(&thread_info_list) - 1) {
        tmp = llist_pop(&thread_info_list);
        if (tmp && tmp->pid != ThreadID) {
            CleanupThreadInfo(tmp);
            hmap_remove_abs(&thread_info_map, tmp->pid);
            free(tmp);
        } else if(tmp && tmp->pid == ThreadID){
            llist_append(&thread_info_list, tmp);
        }
    }
    printf("Resuming new process with Burst of length: %llu\n", currBurstLength);
    fflush(stdout);
    currThreadInfo->in_callback = FALSE;
}

//! Called at the start of thread spawned by this process
void ThreadStart(int ThreadID) {

    ThreadInfo * currThreadInfo;
    int processPID = syscall(SYS_getpid);
    currThreadInfo = AllotThreadInfo(ThreadID, processPID);
    if (!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;

    currThreadInfo->in_callback = TRUE;
    printf("Adding new Thread: %d\n", ThreadID);
    fflush(stdout);
    AddToTracerSchQueue(globalTracerID);
    SignalBurstCompletion(currThreadInfo, 0);
    printf("Resuming new Thread with Burst of length: %llu\n", currBurstLength);
    fflush(stdout);
    currThreadInfo->in_callback = FALSE;
}

//! Called when a thread is about to finish
void ThreadFini(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;

    currThreadInfo->in_callback = TRUE;

    printf("Finishing Thread: %d\n", ThreadID);
    fflush(stdout);
	
    llist_remove(&thread_info_list, currThreadInfo);
    CleanupThreadInfo(currThreadInfo);
    hmap_remove_abs(&thread_info_map, ThreadID);
    
    if (!expStopping) FinishBurstAndDiscard(); 
    currThreadInfo->in_callback = FALSE;
    free(currThreadInfo);  
}

//! Called when the whole application is about to finish
void AppFini(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;

    currThreadInfo->in_callback = TRUE;
    printf("Finishing Process: %d\n", ThreadID);
    fflush(stdout);

    if (!expStopping) FinishBurstAndDiscard();  

    while(llist_size(&thread_info_list)) {
       currThreadInfo = llist_pop(&thread_info_list);
       if (currThreadInfo) {
           CleanupThreadInfo(currThreadInfo);
           hmap_remove_abs(&thread_info_map, currThreadInfo->pid);
	   if (currThreadInfo->pid == ThreadID)
		    currThreadInfo->in_callback = FALSE;
            free(currThreadInfo);
       }
    }

    llist_destroy(&thread_info_list);
    hmap_destroy(&thread_info_map);
}

//! Called before a syscall which may involve some sleeping/timed wait like select, poll etc
void TriggerSyscallWait(int ThreadID, int save) {
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;
    
    currThreadInfo->in_callback = TRUE;

    #ifndef DISABLE_LOOKAHEAD
    if  (save) currThreadInfo->stack.currBBID = currBBID;
    #endif

    if (TriggerSyscallWaitAPI() <= 0) HandleVTExpEnd(ThreadID);

    currThreadInfo->in_callback = FALSE;
}

//! Called upon return from a syscall which may have involved timed wait like select, poll etc
void TriggerSyscallFinish(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if(!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;
 
    currThreadInfo->in_callback = TRUE;
	
    currBurstLength = MarkBurstComplete(1);
    specifiedBurstLength = currBurstLength;

    if (currBurstLength <= 0) HandleVTExpEnd(ThreadID);

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    #ifndef DISABLE_LOOKAHEAD
    // restore globals
    currBBID = currThreadInfo->stack.currBBID;
    #endif

    currThreadInfo->in_callback = FALSE;
}

//! Called after the current execution burst is finished
void vtCallbackFn() {

    int ThreadID;
    ThreadInfo * currThreadInfo;

    if (!vtInitializationComplete) {
        if (currBurstLength <= 0) {	
            currBurstLength = 1000000;
            specifiedBurstLength = currBurstLength;
        }
        return;
    }	
    
    if (currBurstLength <= 0) {

        ThreadID = syscall(SYS_gettid);
        currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);

        if (currThreadInfo != NULL) {
            currThreadInfo->in_callback = TRUE;

            #ifndef DISABLE_LOOKAHEAD
            SetBBLLookAhead((long)currBBID);
            #endif

            SignalBurstCompletion(currThreadInfo, 1);
            currThreadInfo->in_callback = FALSE;
        }
	
    } 
}

/*** For Socket Handling ***/
//! Notes a particular filedescriptor as a socket in internal book-keeping
void AddSocket(int ThreadID, int sockFD, int isNonBlocking) {
    AddFd(ThreadID, sockFD, FD_TYPE_SOCKET, isNonBlocking);
}

//! Returns a positive number if a particular filedescriptor is a socket
int IsSocketFd(int ThreadID, int sockFD) {
    return IsFdTypeMatch(ThreadID, sockFD, FD_TYPE_SOCKET);
}

//! Returns a positive number if a particular filedescriptor is a non-blocking socket
int IsSocketFdNonBlocking(int ThreadID, int sockFD) {
    return IsFdNonBlocking(ThreadID, sockFD);
}

/*** For TimerFd Handlng ***/
//! Notes a particular filedescriptor as a timerfd in internal book-keeping
void  AddTimerFd(int ThreadID, int fd, int isNonBlocking) {
    AddFd(ThreadID, fd, FD_TYPE_TIMERFD, isNonBlocking);
}

//! Returns a positive number if a particular filedescriptor is a timerfd
int IsTimerFd(int ThreadID, int fd) {
    return IsFdTypeMatch(ThreadID, fd, FD_TYPE_TIMERFD);
}

//! Returns a positive number if a particular filedescriptor is a non-blocking timerfd
int IsTimerFdNonBlocking(int ThreadID, int fd) {
    return IsFdNonBlocking(ThreadID, fd);
}

//! Returns a positive number if a filedescriptor is of type timerfd and if it has been armed
int IsTimerArmed(int ThreadID, int fd) {
    if(!IsFdTypeMatch(ThreadID, fd, FD_TYPE_TIMERFD))
        return FALSE;

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    assert(!currThreadInfo->in_callback);

    if (currThreadInfo->processPID != ThreadID) {
        // need to get processPID's threadInfo
        currThreadInfo = hmap_get_abs(&thread_info_map,
                                      currThreadInfo->processPID);
        assert(currThreadInfo != NULL);
    }

    llist_elem * head;
    fdInfo * currFdInfo;
	head = currThreadInfo->special_fds.head;
    
	while (head != NULL) {
        currFdInfo = (fdInfo *)head->item;

        if (currFdInfo && currFdInfo->fd == fd) {
            if (currFdInfo->absExpiryTime || currFdInfo->intervalNS) {
                // Atleast one of them needs to be positive
                return TRUE;
            } 
            return FALSE;
        }
		head = head->next;
	}

    assert(FALSE);
    return FALSE;

}


//! Registers as a new tracer and loads any available lookahead information
void InitializeVtManagement() {

    char * tracer_id_str = getenv("VT_TRACER_ID");
    char * timeline_id_str = getenv("VT_TIMELINE_ID");
    char * exp_type_str = getenv("VT_EXP_TYPE");

    #ifndef DISABLE_LOOKAHEAD
    char * bbl_lookahead_file = getenv("VT_BBL_LOOKAHEAD_FILE");
    char * loop_lookahead_file = getenv("VT_LOOP_LOOKAHEAD_FILE");
    #endif

    int tracer_id;
    int timeline_id;
    int exp_type, ret;
    int my_pid = syscall(SYS_gettid);

    printf("Starting VT-initialization !\n");
    fflush(stdout);

 	
    if (!tracer_id_str) {
        printf("Missing Tracer ID !\n");
        exit(EXIT_FAILURE);
    }

    if (!exp_type_str){
        printf("Missing Exp Type\n");
        exit(EXIT_FAILURE);
    }



    if (tracer_id_str && timeline_id_str)
    	printf ("Tracer-ID: %s, Timeline-ID: %s\n", tracer_id_str, timeline_id_str);
    else if(tracer_id_str)
	printf ("Tracer-ID: %s\n", tracer_id_str);
    tracer_id = atoi(tracer_id_str);
    fflush(stdout);
    if (tracer_id <= 0) {
        printf("Tracer ID must be positive: Received Value: %d\n", tracer_id);
        exit(EXIT_FAILURE);
    }

    exp_type = atoi(exp_type_str);
	
    if (exp_type != EXP_CBE && exp_type != EXP_CS) {
        printf("Exp type must be one of EXP_CBE or EXP_CS\n");
        exit(EXIT_FAILURE);
    }

    if (exp_type == EXP_CS && !timeline_id_str) {
        printf("No timeline specified for EXP_CS experiment\n");
        exit(EXIT_FAILURE);
    }

    if (exp_type == EXP_CBE) {
        ret = RegisterTracer(tracer_id, EXP_CBE, 0);
	    printf("Tracer registration for EXP_CBE complete. Return = %d\n", ret);
        fflush(stdout);
    } else {
        timeline_id = atoi(timeline_id_str);
        if (timeline_id < 0) {
            printf("Timeline ID must be >= 0. Received Value: %d\n",
                    timeline_id);
            exit(EXIT_FAILURE);
        }
        ret = RegisterTracer(tracer_id, EXP_CS, timeline_id);
        globalTimelineID = timeline_id;
	    printf(
            "Tracer registration for EXP_CS complete. TimelineID = %d, Return = %d\n",
            timeline_id, ret);
    }
    printf("Tracer Adding to SQ. Tracer ID = %d\n", tracer_id);
    fflush(stdout);

    if (AddToTracerSchQueue(tracer_id) < 0) {
        printf("Failed To add tracee to tracer schedule queue for pid: %d\n",
                my_pid);
    }

    globalTracerID = tracer_id;

    #ifndef DISABLE_LOOKAHEAD
    printf("Parsing any provided lookahead information ...\n");
    if (bbl_lookahead_file && !ParseLookaheadJsonFile(
        bbl_lookahead_file, &bblLookAheadMap)) {
        printf("Failed to parse BBL Lookaheads. Ignoring ...\n");
        bblLookAheadMap = NULL;
    } else {
        printf("Loaded lookaheads for %lu basic blocks ...\n",
            bblLookAheadMap->num_entries);
    }
    if (loop_lookahead_file && !ParseLookaheadJsonFile(
        loop_lookahead_file, &loopLookAheadMap)) {
        printf("Failed to parse Loop Lookaheads. Ignoring ...\n");
        loopLookAheadMap = NULL;
    } else {
        printf("Loaded lookaheads for %lu loops ...\n",
            loopLookAheadMap->num_entries);
    }
    #endif

    printf("VT initialization successfull !\n");
    fflush(stdout);

    // Initializing helper data structures
    hmap_init(&thread_info_map, 1000);
    llist_init(&thread_info_list);
} 


