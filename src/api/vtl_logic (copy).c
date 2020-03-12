#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "VT_functions.h"
#include "utility_functions.h"
#include "vtl_logic.h"

long long currBurstLength = 0;
long long currBBID = 0;
long long prevBBID = 0;
long long currBBSize = 0;
int expStopping = 0;
int vtInitializationComplete = 0;
int alwaysOn = 0;
int globalTracerID = -1;
int globalTimelineID = -1;
int globalThreadID = 0;
hashmap thread_info_map;
llist thread_info_list;

#define FLIP_ALWAYS_ON_THRESHOLD 100000000


extern void ns_2_timespec(s64 nsec, struct timespec * ts);
extern void ns_2_timeval(s64 nsec, struct timeval * tv);


void SleepForNS(int ThreadID, int64_t duration) {

    if (duration <= 0)
        return;

	/*
    int ret;
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    currThreadInfo->stack.currBBID = currBBID;
    currThreadInfo->stack.prevBBID = prevBBID;
    currThreadInfo->stack.currBBSize = currBBSize;
    currThreadInfo->stack.alwaysOn = alwaysOn;

    ret = vt_sleep_for(duration);

    if (ret < 0)
	HandleVTExpEnd(ThreadID);

    currBurstLength = mark_burst_complete(0);
    if (currBurstLength <= 0)
        HandleVTExpEnd(ThreadID);

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;

	*/
}

void GetCurrentTimespec(struct timespec *ts) {
    ns_2_timespec(get_current_vt_time(), ts);
}

void GetCurrentTimeval(struct timeval * tv) {
    ns_2_timeval(get_current_vt_time(), tv);
}

s64 GetCurrentTime() {
    return get_current_vt_time();
}

void HandleVTExpEnd(int ThreadID) {
    printf("Process: %d exiting VT experiment !\n", ThreadID);
    expStopping = 1;
    exit(0);
}

void CleanupThreadInfo(ThreadInfo * relevantThreadInfo) {
    if (!relevantThreadInfo)
        return;

	/*
    while(llist_size(&relevantThreadInfo->bbl_list)) {
        BasicBlock * currBBL = llist_pop(&relevantThreadInfo->bbl_list);
        if (currBBL) {
            hmap_remove_abs(&relevantThreadInfo->lookahead_map, currBBL->ID);
            while(llist_size(&currBBL->out_neighbours)) {
                llist_pop(&currBBL->out_neighbours);
            }
            llist_destroy(&currBBL->out_neighbours);
            free(currBBL);
        }
        
    }
    hmap_destroy(&relevantThreadInfo->lookahead_map);
	*/
}

ThreadInfo * AllotThreadInfo(int ThreadID) {
    ThreadInfo * currThreadInfo = NULL;
    if (!hmap_get_abs(&thread_info_map, ThreadID)) {
        currThreadInfo = (ThreadInfo *) malloc(sizeof(ThreadInfo));
        assert(currThreadInfo != NULL);
        currThreadInfo->pid = ThreadID;
        currThreadInfo->stack.totalBurstLength = 0;
        currThreadInfo->stack.prevBBID = 0;
        currThreadInfo->stack.currBBID = 0;
        currThreadInfo->stack.currBBSize = 0;
        currThreadInfo->stack.alwaysOn = 0;
        llist_init(&currThreadInfo->bbl_list);
        hmap_init(&currThreadInfo->lookahead_map, 1000);
        hmap_put_abs(&thread_info_map, ThreadID, currThreadInfo);
        llist_append(&thread_info_list, currThreadInfo);
    } else {
        return hmap_get_abs(&thread_info_map, ThreadID);
    }

    return currThreadInfo;
}

BasicBlock * AllotBasicBlockEntry(ThreadInfo * currThreadInfo, int ID) {

    BasicBlock * new_bbl = hmap_get_abs(&currThreadInfo->lookahead_map, ID);
    if (!new_bbl) {
        new_bbl = (BasicBlock *) malloc(sizeof(BasicBlock));
        assert(new_bbl != NULL);
        new_bbl->ID = ID;
        new_bbl->lookahead_value = -1;
        new_bbl->BBLSize = 0;
        new_bbl->isMarked = 0;
        llist_init(&new_bbl->out_neighbours);
        llist_append(&currThreadInfo->bbl_list, new_bbl);
        hmap_put_abs(&currThreadInfo->lookahead_map, ID, new_bbl);
    }
    return new_bbl;
}

void YieldVTBurst(int ThreadID, int save) {

    //ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    //assert(currThreadInfo != NULL);

    printf("Yielding VT Burst !\n");
    fflush(stdout);
	/*
    if (save) {
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;
    }

    assert(release_worker() >= 0);
	*/
}

void ForceCompleteBurst(int ThreadID, int save) {
    //ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    //assert(currThreadInfo != NULL);
    printf("Force Complete Burst !\n");
    fflush(stdout);

	/*
    if (save) {
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;
    }

    currBurstLength = mark_burst_complete(0);

    if (currBurstLength <= 0)
        HandleVTExpEnd(ThreadID);

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;
	*/
}

void SignalBurstCompletion(ThreadInfo * currThreadInfo, int save) {

    
    printf("Signal Complete Burst !\n");
    fflush(stdout);
    // save globals

    if (save) {  
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;  
    }

    currBurstLength = finish_burst();
    //currBurstLength = 1000;
    if (currBurstLength <= 0)
        HandleVTExpEnd(currThreadInfo->pid);

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;
	printf("Leaving Complete Burst !\n");
    fflush(stdout);
}

void AfterForkInChild(int ThreadID) {
    currBurstLength = 0;
    currBBID = 0;
    prevBBID = 0;
    currBBSize = 0;
    alwaysOn = 0;
    // destroy existing hmap
    ThreadInfo * currThreadInfo;

    printf("Adding new child\n");
    fflush(stdout);
    while(llist_size(&thread_info_list)) {
        currThreadInfo = llist_pop(&thread_info_list);
        if (currThreadInfo) {
            CleanupThreadInfo(currThreadInfo);
            hmap_remove_abs(&thread_info_map, currThreadInfo->pid);
            free(currThreadInfo);
        }
    }

    currThreadInfo = AllotThreadInfo(ThreadID);

    assert(globalTracerID != -1);
    add_to_tracer_sq(globalTracerID);
    globalThreadID = syscall(SYS_gettid);
    SignalBurstCompletion(currThreadInfo, 1);
    printf("Resuming Burst of length: %llu\n", currBurstLength);
    fflush(stdout);
}

void ThreadStart(int ThreadID) {

    //AllotThreadInfo(ThreadID);
    assert(globalTracerID != -1);
    printf("Adding new Thread: %d\n", ThreadID);
    fflush(stdout);
	/*

    add_to_tracer_sq(globalTracerID);
    SignalBurstCompletion(ThreadID, 0);
	*/
}

void ThreadFini(int ThreadID) {

    //ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    //assert(currThreadInfo != NULL);
    printf("Finishing new Thread: %d\n", ThreadID);
    fflush(stdout);
	/*
    llist_remove(&thread_info_list, currThreadInfo);
    CleanupThreadInfo(currThreadInfo);
    hmap_remove_abs(&thread_info_map, ThreadID);
    free(currThreadInfo);
    if (!expStopping)
    	finish_burst_and_discard();   
	*/ 
}

void AppFini(int ThreadID) {

    //ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);

    printf("Finishing Application: %d\n", ThreadID);
    fflush(stdout);

	/*
    if (currThreadInfo) {
        while(llist_size(&thread_info_list)) {
            currThreadInfo = llist_pop(&thread_info_list);
            if (currThreadInfo) {
                CleanupThreadInfo(currThreadInfo);
                hmap_remove_abs(&thread_info_map, currThreadInfo->pid);
                free(currThreadInfo);
            }
            
        }

	if (!expStopping)
        	finish_burst_and_discard();  
        llist_destroy(&thread_info_list);
        hmap_destroy(&thread_info_map);
    }
	*/
}

void TriggerSyscallWait(int ThreadID, int save) {
    //ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    //assert(currThreadInfo != NULL);

        printf("Trigger Syscall Wait: %d\n", ThreadID);
    fflush(stdout);
	/*
    if  (save) {
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;
    }

    if (trigger_syscall_wait() <= 0)
        HandleVTExpEnd(ThreadID);*/
}

void TriggerSyscallFinish(int ThreadID) {

    //ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    //assert(currThreadInfo != NULL);

    printf("Trigger Syscall Finish: %d\n", ThreadID);
    fflush(stdout);

	/*
    currBurstLength = mark_burst_complete(1);

    if (currBurstLength <= 0)
        HandleVTExpEnd(ThreadID);

    currThreadInfo->stack.totalBurstLength += currBurstLength;
    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;*/
}

void UpdateLookAhead(ThreadInfo * currThreadInfo, int64_t prev_BBID,
                    int64_t curr_BBID, int curr_BBSize) {

	/*
    BasicBlock * prevBBL;
    BasicBlock * currBBL;

    prevBBL = AllotBasicBlockEntry(currThreadInfo, prev_BBID);
    currBBL = AllotBasicBlockEntry(currThreadInfo, curr_BBID);

    llist_elem * head = prevBBL->out_neighbours.head;
    int skip = 0;

    while (head != NULL) {
        BasicBlock * tmp = (BasicBlock *)head->item;

        if (tmp && tmp->ID == currBBL->ID) {
            skip = 1;
            break;
        }
        head = head->next;
    }

    if (!skip) {
        llist_append(&prevBBL->out_neighbours, currBBL);
        currBBL->BBLSize = curr_BBSize;
    }*/
}

void markCurrBBL(int ThreadID) {
    ThreadInfo * currThreadInfo;
    //currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    //assert(currThreadInfo != NULL);

    printf("mark Curr BBL: %d\n", ThreadID);
    fflush(stdout);

	/*
    if (currBBID != 0) {
        BasicBlock * relevantBBL = AllotBasicBlockEntry(currThreadInfo,
                                                        currBBID);
        assert(relevantBBL != NULL);
        relevantBBL->isMarked = 1;
        relevantBBL->BBLSize = currBBSize;
        relevantBBL->lookahead_value = currBBSize;
    }*/

}

void BasicBlockCallback(ThreadInfo * currThreadInfo, int bblID) {

    BasicBlock * bbl;

    //bbl = AllotBasicBlockEntry(currThreadInfo, bblID);
    //bbl->BBLSize = currBBSize;

    if (!alwaysOn) { 
        // set lookahead here for this Thread ...
    }

    // call finish_burst
    //printf("Signalling finish burst completion !\n");
    //fflush(stdout);
    SignalBurstCompletion(currThreadInfo, 1);
    //printf("Started next burst !\n");
    //fflush(stdout);
}

void vtCallbackFn() {

    int ThreadID;
    ThreadInfo * currThreadInfo;
    
    
	
    
    if (alwaysOn) {
	//ThreadID = syscall(SYS_gettid);
        //currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
        //UpdateLookAhead(ThreadID, prevBBID, currBBID, currBBSize);
	//if (currThreadInfo->stack.totalBurstLength >= FLIP_ALWAYS_ON_THRESHOLD)
        //    alwaysOn = 0;
    }

    
    //printf("Always On = %d, Left Burst Length = %llu, Curr BBID = %llu, Curr BBSize = %d !\n", alwaysOn, currBurstLength, currBBID, currBBSize);
    //fflush(stdout);
    if (currBurstLength <= 0) {


	ThreadID = syscall(SYS_gettid);
	//printf("Finished Burst for Thread: %d !\n", ThreadID);
	//fflush(stdout);
	currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
        assert(currThreadInfo != NULL);
        //BasicBlockCallback(currThreadInfo, currBBID);
	
    }
    prevBBID = currBBID;

    currBurstLength = 1000;
    
}


void initialize_vt_management() {

    char * tracer_id_str = getenv("VT_TRACER_ID");
    char * timeline_id_str = getenv("VT_TIMELINE_ID");
    char * exp_type_str = getenv("VT_EXP_TYPE");
    int tracer_id;
    int timeline_id;
    int exp_type, ret;
    int my_pid = syscall(SYS_gettid);

    printf("Starting VT-initialization !\n");

    if (!tracer_id_str) {
        printf("Missing Tracer ID !\n");
        exit(EXIT_FAILURE);
    }

    if (!exp_type_str){
        printf("Missing Exp Type\n");
        exit(EXIT_FAILURE);
    }

    printf ("Tracer-ID String: %s\n", tracer_id_str);
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
        ret = register_tracer(tracer_id, EXP_CBE, 0);
	printf("Tracer registration EXP_CBE complete. Return = %d\n", ret);
    } else {
        timeline_id = atoi(timeline_id_str);
        if (timeline_id < 0) {
            printf("Timeline ID must be >= 0. Received Value: %d\n",
                    timeline_id);
            exit(EXIT_FAILURE);
        }
        ret = register_tracer(tracer_id, EXP_CS, timeline_id);
        globalTimelineID = timeline_id;
	printf("Tracer registration EXP_CS complete. Return = %d\n", ret);
    }
    printf("Tracer Adding to SQ. Tracer ID = %d\n", tracer_id);
    fflush(stdout);

    if (add_to_tracer_sq(tracer_id) < 0) {
        printf("Failed To add tracee to tracer schedule queue for pid: %d\n",
                my_pid);
    }

    globalTracerID = tracer_id;
    printf("VT initialization successfull !\n");
    fflush(stdout);

    // Initializing helper data structures
    hmap_init(&thread_info_map, 1000);
    llist_init(&thread_info_list);
} 
