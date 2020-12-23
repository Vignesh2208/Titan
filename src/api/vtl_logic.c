#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "VT_functions.h"
#include "utility_functions.h"
#include "vtl_logic.h"
#include "fd_handling.h"
#include "cache_simulation.h"

#include "socket_layer/netdev.h"
#include "socket_layer/socket.h"

#ifndef DISABLE_LOOKAHEAD
#include "lookahead_parsing.h"
#endif

#define	fxrstor(addr)		__asm __volatile("fxrstor %0" : : "m" (*(addr)))
#define	fxsave(addr)		__asm __volatile("fxsave %0" : "=m" (*(addr)))

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })


s64 currBurstCyclesLeft = 0;
s64 specifiedBurstCycles = 0;

#ifdef COLLECT_STATS
s64 totalExecBinaryInstructions = 0;
s64 totalExecVTLInstructions = 0;
#endif

#ifndef DISABLE_LOOKAHEAD
s64 currBBID = 0;
s64 currLoopID = 0;
struct lookahead_info bblLookAheadInfo = {.mapped_memblock = NULL, .mapped_memblock_length = 0, .lmap = NULL};
struct lookahead_info loopLookAheadInfo = {.mapped_memblock = NULL, .mapped_memblock_length = 0, .lmap = NULL};
#endif

#ifndef DISABLE_INSN_CACHE_SIM
s64 currBBInsCacheMissPenalty = 0;
#endif

int expStopping = 0;
int vtInitializationComplete = 0;

#ifndef DISABLE_VT_SOCKET_LAYER
uint32_t tracerSrcIpAddr = 0;
float modelledNicSpeedMbps = 1000.0;
#endif
int globalTracerID = -1;
int globalTimelineID = -1;
int currActiveThreadID = 0;
float cpuCyclesPerNs = 1.0;
hashmap thread_info_map;
llist thread_info_list;

#define FLIP_ALWAYS_ON_THRESHOLD 100000000


extern void ns_2_timespec(s64 nsec, struct timespec * ts);
extern void ns_2_timeval(s64 nsec, struct timeval * tv);

#ifndef DISABLE_LOOKAHEAD
//! Returns loop lookahead for a specific loop number
s64 GetLoopLookAhead(long loopNumber) {
    if (!loopLookAheadInfo.lmap.number_of_values ||
        (loopNumber < loopLookAheadInfo.lmap.start_offset) ||
        (loopNumber > loopLookAheadInfo.lmap.finish_offset))
        return 0;
    return loopLookAheadInfo.lmap.lookahead_values[loopNumber - loopLookAheadInfo.lmap.start_offset];
}

//! Returns lookahead from a specific basic block
s64 GetBBLLookAhead(long bblNumber) {
    if (!bblLookAheadInfo.lmap.number_of_values ||
        (bblNumber < bblLookAheadInfo.lmap.start_offset) ||
        (bblNumber > bblLookAheadInfo.lmap.finish_offset))
        return 0;
    return bblLookAheadInfo.lmap.lookahead_values[bblNumber - bblLookAheadInfo.lmap.start_offset];
    //return 0;
}

//! Sets the lookahead for a specific loop by estimating the number of iterations it would make
void SetLoopLookahead(int initValue, int finalValue, int stepValue, int loopDepth) {
    //printf("SetLoopLookahead, return address: %p, currBBID: %llu, currLoopID: %llu\n",
    //    __builtin_return_address(0), currBBID, currLoopID);
    
    int origLoopDepth = loopDepth;

    if (!vtInitializationComplete)
        return;

    //printf("Init called SetLoopLookahead: init: %d, final: %d, step: %d, loopDepth: %d\n",
    //    initValue, finalValue, stepValue, origLoopDepth);

    ThreadInfo * currThreadInfo;
    int ThreadID;
    if (!currActiveThreadID) {
        ThreadID = syscall(SYS_gettid);
        currActiveThreadID = ThreadID;
    } else {
        ThreadID = currActiveThreadID;
    }

    currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);

    if (!currThreadInfo)
	    return;

    s64 numIterations = 0;
    s64 origNumIterations = 0;
    s64 cumulativeIterations = 1;
    if ((finalValue > initValue && stepValue > 0) ||
        (finalValue < initValue && stepValue < 0)) {
        numIterations = (s64)(finalValue - initValue)/stepValue;
	    origNumIterations = numIterations;
    }

    if (loopDepth == 0) {
	    for (int i = 0; i < MAX_LOOP_DEPTH; i++) {
		    currThreadInfo->loopRunTime[i].entered = 0;
		    currThreadInfo->loopRunTime[i].numIterations = 0;
        }
	
    } else if (loopDepth > 0 && loopDepth < MAX_LOOP_DEPTH) {	
        /*for (int i = 0; i < loopDepth; i++) {
            if (!currThreadInfo->loopRunTime[i].entered || 
                !currThreadInfo->loopRunTime[i].numIterations) {
                cumulativeIterations = 1;
                loopDepth = -1; // we implicitly play it safe and don't try to cumulate for this loop or any other child loop
                break;
            } else {
                cumulativeIterations = cumulativeIterations * currThreadInfo->loopRunTime[i].numIterations;
            }
        }

	    numIterations = cumulativeIterations * numIterations;
        */

       numIterations = currThreadInfo->loopRunTime[loopDepth-1].numIterations * numIterations;
    }

    if (loopDepth < 0 || loopDepth >= MAX_LOOP_DEPTH || 
        !currThreadInfo->loopRunTime[loopDepth].entered) {
    	s64 loop_lookahead_ns = GetLoopLookAhead((long)currLoopID)*numIterations;
        loop_lookahead_ns = (s64)((loop_lookahead_ns)/cpuCyclesPerNs);

        printf("Called SetLoopLookahead: init: %d, final: %d, step: %d, "
               "loopDepth: %d, cumulative-num-iterations: %d, look-ahead-ns: %lld\n",
               initValue, finalValue, stepValue, origLoopDepth, numIterations,
               loop_lookahead_ns);

        if (loop_lookahead_ns)
    	    SetLookahead(loop_lookahead_ns + GetCurrentTime(), LOOKAHEAD_ANCHOR_NONE);

    }

    if (loopDepth >= 0 && loopDepth < MAX_LOOP_DEPTH && 
        !currThreadInfo->loopRunTime[loopDepth].entered) {
    	currThreadInfo->loopRunTime[loopDepth].entered = 1;
    	//currThreadInfo->loopRunTime[loopDepth].numIterations = origNumIterations;
        currThreadInfo->loopRunTime[loopDepth].numIterations = numIterations;
    }

}



//! Sets the lookahead at a specific basic block
void SetBBLLookAhead(long bblNumber) {
    if (!vtInitializationComplete)
        return;
    SetLookahead(GetBBLLookAhead(bblNumber), LOOKAHEAD_ANCHOR_CURR_TIME);
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

    if (ret < 0) {
	printf ("VT-sleep-for exiting !\n");
	fflush(stdout);
	HandleVTExpEnd(ThreadID);
    }


    currBurstCyclesLeft = MarkBurstComplete(1);
    currBurstCyclesLeft = (s64)(currBurstCyclesLeft*cpuCyclesPerNs);
    int myThreadID = syscall(SYS_gettid);

    if (currActiveThreadID != myThreadID) { // There was a context switch
        #ifndef DISABLE_DATA_CACHE_SIM
        flushDataCache();
        #endif

        #ifndef DISABLE_INSN_CACHE_SIM
        flushInsCache();
        #endif
    }
    currActiveThreadID = myThreadID;
    
    
    specifiedBurstCycles = currBurstCyclesLeft;
    if (currBurstCyclesLeft <= 0) {
	printf ("VT-sleep-for exiting after resume !\n");
	fflush(stdout);
	HandleVTExpEnd(ThreadID);
    }

    currThreadInfo->stack.totalBurstLength += currBurstCyclesLeft;

    // restore globals
    #ifndef DISABLE_LOOKAHEAD
    currBBID = currThreadInfo->stack.currBBID;
    #endif
    currThreadInfo->in_callback = FALSE;
}


//! Returns current virtual time as nanoseconds
s64 GetCurrentTime() {
    if (currBurstCyclesLeft && specifiedBurstCycles
        && specifiedBurstCycles >= currBurstCyclesLeft)
        return GetCurrentVtTime() + (s64)(
            (specifiedBurstCycles - currBurstCyclesLeft)/cpuCyclesPerNs);
    else
        return GetCurrentVtTime();
}

//! Returns current virtual time as timespec
void GetCurrentTimespec(struct timespec *ts) {
    ns_2_timespec(GetCurrentTime(), ts);
}

//! Returns current virtual time as timeval
void GetCurrentTimeval(struct timeval * tv) {
    ns_2_timeval(GetCurrentTime(), tv);
}

#ifndef DISABLE_LOOKAHEAD
//! Sets packet send time for a specific packet
void SetPktSendTime(int payloadHash, int payloadLen, s64 pktSendTimeStamp) {
    SetPktSendTimeAPI(globalTracerID, payloadHash, payloadLen, pktSendTimeStamp);
 
}

//! Returns earliest arrival time of any packet intended for this node 
s64 GetPacketEAT() {
    //return GetEarliestArrivalTime();
    return GetCurrentTime();
}

//! Common wrapper to set a lookahead for this node
int SetLookahead(s64 bulkLookaheadValue, int  lookahead_anchor_type) {
    //if (bulkLookaheadValue)
    return SetProcessLookahead(bulkLookaheadValue, lookahead_anchor_type);
    //return 0;
}
#endif


//! Called when the virtual time experiment is about to end
void HandleVTExpEnd(int ThreadID) {
    printf("\n");
    printf("------------------------- ACTUAL STDOUT FROM EXECUTABLE ENDED -----------------------------\n");
    printf("\n");
    
    #ifndef DISABLE_DATA_CACHE_SIM
    printDataCacheStats();
    cleanupDataCache();
    #endif

    #ifdef COLLECT_STATS
    if (totalExecBinaryInstructions > 0) {
	    printf ("VTL-Overhead: %f\n",  
            (float)totalExecBinaryInstructions/(float)totalExecVTLInstructions); 
        fflush(stdout);
    }
    printf ("totalExecBinaryInstructions = %llu\n", totalExecBinaryInstructions);
    printf ("totalExecVTLInstructions = %llu\n", totalExecVTLInstructions);
    fflush(stdout);
    #endif

    printf("Process: %d exiting VT experiment !\n", ThreadID);
    fflush(stdout);
    expStopping = 1;

    #ifndef DISABLE_LOOKAHEAD
    if (loopLookAheadInfo.lmap.number_of_values)
        CleanLookaheadInfo(&loopLookAheadInfo);

    if (bblLookAheadInfo.lmap.number_of_values)
        CleanLookaheadInfo(&bblLookAheadInfo);
    #endif

    #ifndef DISABLE_INSN_CACHE_SIM
    printInsnCacheStats();
    cleanupInsCache();
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
    free(relevantThreadInfo->fpuState);
    
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
        currThreadInfo->fpuState = (char *) malloc(512 * sizeof(char));
        currThreadInfo->stack.totalBurstLength = 0;

	#ifndef DISABLE_VT_SOCKET_LAYER
	currThreadInfo->stackThread = -1;
	#endif

        #ifndef DISABLE_LOOKAHEAD
        currThreadInfo->stack.currBBID = 0;
        #endif

        assert(currThreadInfo->fpuState != NULL);
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

    currBurstCyclesLeft = MarkBurstComplete(0);
    int myThreadID = syscall(SYS_gettid);

    if (currActiveThreadID != myThreadID) { // There was a context switch
        #ifndef DISABLE_DATA_CACHE_SIM
        flushDataCache();
        #endif

        #ifndef DISABLE_INSN_CACHE_SIM
        flushInsCache();
        #endif
    }
    currActiveThreadID = myThreadID;

    if (currBurstCyclesLeft)
        currBurstCyclesLeft = max((s64)(currBurstCyclesLeft*cpuCyclesPerNs), 1);
    specifiedBurstCycles = currBurstCyclesLeft;

    //printf ("Resuming from force-complete burst with: %llu cycles\n", specifiedBurstCycles);
    //fflush(stdout); 

    if (currBurstCyclesLeft <= 0)
        HandleVTExpEnd(ThreadID);

    if (currThreadInfo) {

        currThreadInfo->stack.totalBurstLength += currBurstCyclesLeft;

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
//__attribute__((ms_abi))
void SignalBurstCompletion(ThreadInfo * currThreadInfo, int save) {

    fxsave(currThreadInfo->fpuState);
    
    #ifndef DISABLE_LOOKAHEAD
    // save globals
    if (save) currThreadInfo->stack.currBBID = currBBID;
    #endif
    
    currBurstCyclesLeft = FinishBurst();
    int myThreadID = syscall(SYS_gettid);
    if (currActiveThreadID != myThreadID) { // There was a context switch
        #ifndef DISABLE_DATA_CACHE_SIM
        flushDataCache();
        #endif

        #ifndef DISABLE_INSN_CACHE_SIM
        flushInsCache();
        #endif
    }
    currActiveThreadID = myThreadID;

    if (currBurstCyclesLeft)
        currBurstCyclesLeft = max((s64)(currBurstCyclesLeft*cpuCyclesPerNs), 1);

    /*FinishBurst();
    currBurstCyclesLeft = max((s64)(1000000*cpuCyclesPerNs), 1);*/

    specifiedBurstCycles = currBurstCyclesLeft;

    if (currBurstCyclesLeft <= 0) HandleVTExpEnd(currThreadInfo->pid);

    currThreadInfo->stack.totalBurstLength += currBurstCyclesLeft;

    #ifndef DISABLE_LOOKAHEAD
    // restore globals
    currBBID = currThreadInfo->stack.currBBID;
    #endif

    fxrstor(currThreadInfo->fpuState);
    
}


//! Called in the forked child process as soon as it starts

#ifndef DISABLE_VT_SOCKET_LAYER
void AfterForkInChild(int ThreadID, int ParentProcessID, pthread_t stackThread) {
#else
void AfterForkInChild(int ThreadID, int ParentProcessID) {
#endif

    // destroy existing hmap
    ThreadInfo * currThreadInfo;
    ThreadInfo * tmp;


    currThreadInfo = AllotThreadInfo(ThreadID, ThreadID);     
    assert(currThreadInfo != NULL);
    currBurstCyclesLeft = 0;

    #ifndef DISABLE_VT_SOCKET_LAYER
    currThreadInfo->stackThread = stackThread;
    #endif

    #ifndef DISABLE_LOOKAHEAD
    currBBID = 0;
    #endif

    specifiedBurstCycles = currBurstCyclesLeft;

    currThreadInfo->in_callback = TRUE; 

    //printf("Adding new child process\n");
    //fflush(stdout);

    AddToTracerSchQueue(globalTracerID);


    
    currActiveThreadID = syscall(SYS_gettid);
    // There was a context switch
    #ifndef DISABLE_DATA_CACHE_SIM
    loadDataCacheParams();
    flushDataCache();
    #endif

    #ifndef DISABLE_INSN_CACHE_SIM
    loadInsnCacheParams();
    flushInsCache();
    #endif
    
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
    //printf("Resuming new process with Burst of length: %llu\n", currBurstCyclesLeft);
    //fflush(stdout);
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
    printf("Resuming new Thread with Burst of length: %llu\n", currBurstCyclesLeft);
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

    printf("Finished Thread: %d\n", ThreadID);
    fflush(stdout);
    #ifndef DISABLE_LOOKAHEAD
    if (!llist_size(&thread_info_list)) {
        // there is no other thread. Clean Lookahead Informations
        CleanLookaheadInfo(&bblLookAheadInfo);
        CleanLookaheadInfo(&loopLookAheadInfo);
    }
    #endif
}

//! Called when the whole application is about to finish
void AppFini(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;

    currThreadInfo->in_callback = TRUE;
    #ifndef DISABLE_VT_SOCKET_LAYER
    pthread_t stackThread = currThreadInfo->stackThread;
    #endif

    printf("Finishing Process: %d\n", ThreadID);
    fflush(stdout);

    #ifndef DISABLE_VT_SOCKET_LAYER
    MarkNetDevExiting();
    #endif

    #ifndef DISABLE_INSN_CACHE_SIM
    printInsnCacheStats();
    #endif

    #ifndef DISABLE_DATA_CACHE_SIM
    printDataCacheStats();
    #endif

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

    #ifndef DISABLE_VT_SOCKET_LAYER
    printf ("Waiting for stack-thread to exit ...\n");
    fflush(stdout);
    pthread_join(stackThread, NULL);
    printf ("Exiting ...\n");
    fflush(stdout);
    #endif



    #ifndef DISABLE_LOOKAHEAD
    CleanLookaheadInfo(&bblLookAheadInfo);
    CleanLookaheadInfo(&loopLookAheadInfo);
    #endif

    #ifdef COLLECT_STATS
    if (totalExecBinaryInstructions > 0) {
	    printf ("VTL-Overhead: %f\n", 
            (float)totalExecBinaryInstructions/(float)totalExecVTLInstructions); 
        fflush(stdout);
    }
    printf ("totalExecBinaryInstructions = %llu\n", totalExecBinaryInstructions);
    printf ("totalExecVTLInstructions = %llu\n", totalExecVTLInstructions);
    fflush(stdout);
    #endif
}

//! Called before a syscall which may involve some sleeping/timed wait like select, poll etc
s64 TriggerSyscallWait(int ThreadID, int save) {
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;
    
    currThreadInfo->in_callback = TRUE;

    #ifndef DISABLE_LOOKAHEAD
    if  (save) currThreadInfo->stack.currBBID = currBBID;
    #endif
    s64 ret = TriggerSyscallWaitAPI();
    if (ret <= 0) HandleVTExpEnd(ThreadID);

    currThreadInfo->in_callback = FALSE;

    return ret;
}

//! Called upon return from a syscall which may have involved timed wait like select, poll etc
void TriggerSyscallFinish(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if(!currThreadInfo) return;

    if (currThreadInfo->in_callback) return;
 
    currThreadInfo->in_callback = TRUE;
    
    currBurstCyclesLeft = MarkBurstComplete(1);

    int myThreadID = syscall(SYS_gettid);
    if (currActiveThreadID != myThreadID) { // There was a context switch
        #ifndef DISABLE_DATA_CACHE_SIM
        flushDataCache();
        #endif

        #ifndef DISABLE_INSN_CACHE_SIM
        flushInsCache();
        #endif
    }
    currActiveThreadID = myThreadID;

    if (currBurstCyclesLeft)
        currBurstCyclesLeft = max((s64)(currBurstCyclesLeft*cpuCyclesPerNs), 1);

    specifiedBurstCycles = currBurstCyclesLeft;

    if (currBurstCyclesLeft <= 0) HandleVTExpEnd(ThreadID);

    currThreadInfo->stack.totalBurstLength += currBurstCyclesLeft;

    #ifndef DISABLE_LOOKAHEAD
    // restore globals
    currBBID = currThreadInfo->stack.currBBID;
    #endif

    currThreadInfo->in_callback = FALSE;
}

void handleVtCallback() {
    int ThreadID;
    ThreadInfo * currThreadInfo;
    ThreadID = syscall(SYS_gettid);
    currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);

    if (currThreadInfo != NULL) {
        currThreadInfo->in_callback = TRUE;

        #ifndef DISABLE_LOOKAHEAD
        SetBBLLookAhead((long)currBBID);
        #endif

        #ifndef DISABLE_VT_SOCKET_LAYER
        double quantaNs = (double)specifiedBurstCycles / (double)cpuCyclesPerNs;
        SetNetDevCurrTsliceQuantaUs((long)quantaNs/NSEC_PER_US);
        RunTCPStackRxLoop();
        #endif

        
        SignalBurstCompletion(currThreadInfo, 1);
        currThreadInfo->in_callback = FALSE;
    }
    

}

//! Called after the current execution burst is finished
void vtCallbackFn() {
    if (!vtInitializationComplete) {

        if (currBurstCyclesLeft <= 0) {	
            currBurstCyclesLeft = 100000000;
            currActiveThreadID = syscall(SYS_gettid);
            specifiedBurstCycles = currBurstCyclesLeft;
        }
        return;
    }	
    
    if (currBurstCyclesLeft <= 0) {
	    handleVtCallback();
    } 
}

/*** For Socket Handling ***/
//! Notes a particular filedescriptor as a socket in internal book-keeping
void AddSocket(int ThreadID, int sockFD, int sockFdProtoType, int isNonBlocking) {
    AddFd(ThreadID, sockFD, FD_TYPE_SOCKET, sockFdProtoType, isNonBlocking);
}

//! Returns a positive number if a particular filedescriptor is a socket
int IsSocketFd(int ThreadID, int sockFD) {
    return IsFdTypeMatch(ThreadID, sockFD, FD_TYPE_SOCKET);
}

//! Returns a positive number if a particular filedescriptor is a tcp-socket
int IsTCPSocket(int ThreadID, int sockFD) {
    return IsTCPSockFd(ThreadID, sockFD);
}

//! Returns a positive number if a particular filedescriptor is a non-blocking socket
int IsSocketFdNonBlocking(int ThreadID, int sockFD) {
    return IsFdNonBlocking(ThreadID, sockFD);
}

/*** For TimerFd Handlng ***/
//! Notes a particular filedescriptor as a timerfd in internal book-keeping
void  AddTimerFd(int ThreadID, int fd, int isNonBlocking) {
    AddFd(ThreadID, fd, FD_TYPE_TIMERFD, FD_PROTO_TYPE_NONE, isNonBlocking);
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

#ifndef DISABLE_VT_SOCKET_LAYER
void InitializeTCPStack(int stackID) {
    SetNetDevStackID(globalTracerID, stackID, modelledNicSpeedMbps);
}

void MarkTCPStackActive() {
    MarkNetworkStackActive(tracerSrcIpAddr);
}

void RunTCPStackRxLoop() {
    NetDevRxLoop();
}
void MarkTCPStackInactive() {
    MarkNetworkStackInactive();
}

int HandleReadSyscall(int ThreadID, int fd, void *buf, int count, int *redirect) {
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo || currThreadInfo->in_callback || IsNetDevInCallback()) {
        *redirect = 1;
        return 0;
    }
    int did_block;
    int ret;
    currThreadInfo->in_callback = TRUE;

    if (!IsTCPSockFd(ThreadID, fd)) {
        *redirect = 1;
        currThreadInfo->in_callback = FALSE;
        return 0;
    }

    ret = _read(fd, buf, count, &did_block);

    if (did_block)
        TriggerSyscallFinish(ThreadID);
    currThreadInfo->in_callback = FALSE;
    *redirect = 0;
    return ret;
}

int HandleWriteSyscall(int ThreadID, int fd, const void *buf, int count, int *redirect) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo || currThreadInfo->in_callback || IsNetDevInCallback()) {
        *redirect = 1;
        return 0;
    }

    int did_block;
    int ret;
    currThreadInfo->in_callback = TRUE;

    if (!IsTCPSockFd(ThreadID, fd)) {
        *redirect = 1;
        currThreadInfo->in_callback = FALSE;
        return 0;
    }

    ret = _write(fd, buf, count, &did_block);

    if (did_block)
        TriggerSyscallFinish(ThreadID);
    currThreadInfo->in_callback = FALSE;
    *redirect = 0;
    return ret;

}
#endif

//! Registers as a new tracer and loads any available lookahead information
void InitializeVtManagement() {

    int tracer_id;
    int timeline_id;
    int exp_type, ret;

    timeline_id = -1;

    printf ("Extracting env variables !\n");
    fflush(stdout);
    if (!GetIntEnvVariable("VT_TRACER_ID", &tracer_id)) {
        printf("Missing Tracer ID !\n");
        exit(EXIT_FAILURE);
    }

    printf ("Extracted tracer-id: %d\n", tracer_id);
    fflush(stdout);

    if (!GetIntEnvVariable("VT_EXP_TYPE", &exp_type)) {
        printf("Missing Exp Type\n");
        exit(EXIT_FAILURE);
    }


    printf ("Extracted exp-type: %d\n", exp_type);
    fflush(stdout);

    if (exp_type != EXP_CBE && exp_type != EXP_CS) {
        printf("Unknown Exp Type. Exp Type must be one of EXP_CBE or EXP_CS\n");
        exit(EXIT_FAILURE);
    }

    if (exp_type == EXP_CS && !GetIntEnvVariable("VT_TIMELINE_ID", &timeline_id)) {
        printf("No timeline specified for EXP_CS experiment\n");
        exit(EXIT_FAILURE);
    }
    if (tracer_id <= 0) {
        printf("Tracer ID must be positive: Received Value: %d\n", tracer_id);
        exit(EXIT_FAILURE);
    }

    if (!GetFloatEnvVariable("VT_CPU_CYLES_NS", &cpuCyclesPerNs))
        printf ("Failed to parse cpu cycles per ns. Using default value: %f\n",
            cpuCyclesPerNs);

    printf ("Extracted cpu-cycles-ns: %f\n", cpuCyclesPerNs);
    fflush(stdout);

    #ifndef DISABLE_VT_SOCKET_LAYER
        if (!GetIntEnvVariable("VT_SOCKET_LAYER_IP", (int *)&tracerSrcIpAddr)) {
            printf("Missing Src-IP-addr\n");
            exit(EXIT_FAILURE);
        }

        if (!GetFloatEnvVariable("VT_NIC_SPEED_MBPS", (float *)&modelledNicSpeedMbps)) {
            printf("Missing nic speed specification. Using default 1Gbps.\n");
            modelledNicSpeedMbps = 1000.0;
        }

        printf ("Extracted socket-layer-ip: %d\n", tracerSrcIpAddr);
        fflush(stdout);
    #endif

    if (cpuCyclesPerNs <= 0)
        cpuCyclesPerNs = 1.0;

    #ifndef DISABLE_LOOKAHEAD
    char * bbl_lookahead_file = getenv("VT_BBL_LOOKAHEAD_FILE");
    char * loop_lookahead_file = getenv("VT_LOOP_LOOKAHEAD_FILE");
    #endif

    
    int my_pid = syscall(SYS_gettid);

    printf("Starting VT-initialization !\n");
    fflush(stdout);


    if (timeline_id >= 0)
        printf ("Tracer-ID: %d, Timeline-ID: %d\n", tracer_id, timeline_id);
    else
        printf ("Tracer-ID: %d\n", tracer_id);
    fflush(stdout);
    

    if (exp_type == EXP_CBE) {
        ret = RegisterTracer(tracer_id, EXP_CBE, 0);
        printf("Tracer registration for EXP_CBE complete. Return = %d\n", ret);
        fflush(stdout);
    } else {
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
    if (bbl_lookahead_file && !LoadLookahead(
        bbl_lookahead_file, &bblLookAheadInfo)) {
        printf("Failed to parse BBL Lookaheads. Ignoring ...\n");
    } else {
        printf("Loaded BBL lookaheads for %lu basic blocks ...\n",
            bblLookAheadInfo.lmap.number_of_values);
    }
    if (loop_lookahead_file && !LoadLookahead(
        loop_lookahead_file, &loopLookAheadInfo)) {
        printf("Failed to parse Loop Lookaheads. Ignoring ...\n");
    } else {
        printf("Loaded Loop lookaheads for %lu loops ...\n",
            loopLookAheadInfo.lmap.number_of_values);
    }
    #endif

    #ifndef DISABLE_INSN_CACHE_SIM
    printf("Parsing any insn cache params ...\n");
    loadInsnCacheParams();
    #endif

    #ifndef DISABLE_DATA_CACHE_SIM
    printf("Parsing any data cache params ...\n");
    loadDataCacheParams();
    #endif

    printf("VT initialization successfull !\n");
    fflush(stdout);

    // Initializing helper data structures
    hmap_init(&thread_info_map, 1000);
    llist_init(&thread_info_list);
} 


