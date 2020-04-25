#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "VT_functions.h"
#include "utility_functions.h"
#include "vtl_logic.h"
#include "fd_handling.h"



long long currBurstLength = 0;
long long specifiedBurstLength = 0;
long long currBBID = 0;
long long prevBBID = 0;
long long currBBSize = 0;
int expStopping = 0;
int interesting = 0;
int vtInitializationComplete = 0;
int alwaysOn = 1;
int globalTracerID = -1;
int globalTimelineID = -1;
int globalThreadID = 0;
hashmap thread_info_map;
llist thread_info_list;

#define FLIP_ALWAYS_ON_THRESHOLD 100000000


extern void ns_2_timespec(s64 nsec, struct timespec * ts);
extern void ns_2_timeval(s64 nsec, struct timeval * tv);


void SetPktSendTime(int payloadHash, int payloadLen, s64 pktSendTimeStamp) {
	set_pkt_send_time(payloadHash, payloadLen, pktSendTimeStamp);
}



void SleepForNS(int ThreadID, int64_t duration) {

    if (duration <= 0)
        return;
	
    int ret;
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo)
	return;

    if (currThreadInfo->in_callback)
	return;


    currThreadInfo->in_callback = TRUE;
    currThreadInfo->stack.currBBID = currBBID;
    currThreadInfo->stack.prevBBID = prevBBID;
    currThreadInfo->stack.currBBSize = currBBSize;
    currThreadInfo->stack.alwaysOn = alwaysOn;

    ret = vt_sleep_for(duration);

    if (ret < 0)
	HandleVTExpEnd(ThreadID);

    currBurstLength = mark_burst_complete(0);
    specifiedBurstLength = currBurstLength;
    if (currBurstLength <= 0)
        HandleVTExpEnd(ThreadID);

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;
    currThreadInfo->in_callback = FALSE;

	
}

void GetCurrentTimespec(struct timespec *ts) {
    ns_2_timespec(get_current_vt_time(), ts);
}

void GetCurrentTimeval(struct timeval * tv) {
    ns_2_timeval(get_current_vt_time(), tv);
}

s64 GetCurrentTime() {
    if (currBurstLength > 0 && specifiedBurstLength > 0)
    	return get_current_vt_time() + specifiedBurstLength - currBurstLength;
    else
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

    while(llist_size(&relevantThreadInfo->special_fds)){
        fdInfo * currfdInfo = llist_pop(
            &relevantThreadInfo->special_fds);
        if (currfdInfo)
            free(currfdInfo);
    }

    llist_destroy(&relevantThreadInfo->special_fds);

    hmap_destroy(&relevantThreadInfo->lookahead_map);
	
}

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
        currThreadInfo->stack.prevBBID = 0;
        currThreadInfo->stack.currBBID = 0;
        currThreadInfo->stack.currBBSize = 0;
        currThreadInfo->stack.alwaysOn = 1;
        llist_init(&currThreadInfo->bbl_list);
        llist_init(&currThreadInfo->special_fds);
        hmap_init(&currThreadInfo->lookahead_map, 1000);
        hmap_put_abs(&thread_info_map, ThreadID, currThreadInfo);
        llist_append(&thread_info_list, currThreadInfo);
    } else {
        return hmap_get_abs(&thread_info_map, ThreadID);
    }

    return currThreadInfo;
}

BasicBlock * AllotBasicBlockEntry(ThreadInfo * currThreadInfo, int ID) {

    if (ID < 0)
	ID = -1*ID;

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

void YieldVTBurst(int ThreadID, int save, long syscall_number) {


    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo) {
	return;
    }

    if (currThreadInfo->in_callback)
	return;


    if (currThreadInfo->in_force_completed)
	return;
    
    currThreadInfo->in_callback = TRUE;

    if (currThreadInfo->yielded == 0) {
	    //printf("Yielding VT Burst. ThreadID: %d. Syscall number: %d!. In_Callback = %d\n", ThreadID, syscall_number, currThreadInfo->in_callback);
    	    //fflush(stdout);

	    currThreadInfo->yielded = TRUE;
	    if (save) {
		currThreadInfo->stack.currBBID = currBBID;
		currThreadInfo->stack.prevBBID = prevBBID;
		currThreadInfo->stack.currBBSize = currBBSize;
		currThreadInfo->stack.alwaysOn = alwaysOn;
	    }

	    release_worker();
            
    }
    currThreadInfo->in_callback = FALSE;
	
}

void ForceCompleteBurst(int ThreadID, int save, long syscall_number) {
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo)
	return;

    if (currThreadInfo->in_callback)
	return;

    if (currThreadInfo->in_force_completed)
	return;

    currThreadInfo->in_force_completed = TRUE;

    //printf("Force Complete Burst. Paused ThreadID: %d. Syscall number: %d!\n", ThreadID, syscall_number);
    //fflush(stdout);

    

    if (save && currThreadInfo) {
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;
    }

    

    currBurstLength = mark_burst_complete(0);
    specifiedBurstLength = currBurstLength;

    if (currBurstLength <= 0)
        HandleVTExpEnd(ThreadID);

    if (currThreadInfo) {

    	currThreadInfo->stack.totalBurstLength += currBurstLength;

    	// restore globals
    	alwaysOn = currThreadInfo->stack.alwaysOn;
    	currBBID = currThreadInfo->stack.currBBID;
    	prevBBID = currThreadInfo->stack.prevBBID;
    	currBBSize = currThreadInfo->stack.currBBSize;
    }

    //printf("Force Complete Burst. Resumed ThreadID: %d. Syscall number: %d!\n", ThreadID, syscall_number);
    //fflush(stdout);
    
    if (currThreadInfo->yielded == TRUE)
    	currThreadInfo->yielded = FALSE;

    if (currThreadInfo->in_force_completed == TRUE)
    	currThreadInfo->in_force_completed = FALSE;
}

void SignalBurstCompletion(ThreadInfo * currThreadInfo, int save) {

    
    // save globals
    if (save) {  
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;  
    }

    currBurstLength = finish_burst();
    specifiedBurstLength = currBurstLength;
    if (currBurstLength <= 0)
        HandleVTExpEnd(currThreadInfo->pid);

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;
}

void AfterForkInChild(int ThreadID) {

    // destroy existing hmap
    ThreadInfo * currThreadInfo;
    ThreadInfo * tmp;
    
    while(llist_size(&thread_info_list)) {
        tmp = llist_pop(&thread_info_list);
        if (tmp) {
            CleanupThreadInfo(tmp);
            hmap_remove_abs(&thread_info_map, tmp->pid);
            free(tmp);
        }
    }

    currThreadInfo = AllotThreadInfo(ThreadID, ThreadID);
     
    if (!currThreadInfo)
	return;

    currBurstLength = 0;
    currBBID = 0;
    prevBBID = 0;
    currBBSize = 0;
    alwaysOn = 1;
    specifiedBurstLength = currBurstLength;

    currThreadInfo->in_callback = TRUE; 

    printf("Adding new child process\n");
    fflush(stdout);

    add_to_tracer_sq(globalTracerID);
    globalThreadID = syscall(SYS_gettid);
    SignalBurstCompletion(currThreadInfo, 1);
    printf("Resuming new process with Burst of length: %llu\n", currBurstLength);
    fflush(stdout);
    currThreadInfo->in_callback = FALSE;
}

void ThreadStart(int ThreadID) {

    ThreadInfo * currThreadInfo;
    int processPID = syscall(SYS_getpid);
    currThreadInfo = AllotThreadInfo(ThreadID, processPID);
    if (!currThreadInfo)
	return;

    if (currThreadInfo->in_callback)
	return;

    currThreadInfo->in_callback = TRUE;
    printf("Adding new Thread: %d\n", ThreadID);
    fflush(stdout);
    add_to_tracer_sq(globalTracerID);
    SignalBurstCompletion(currThreadInfo, 0);
    printf("Resuming new Thread with Burst of length: %llu\n", currBurstLength);
    fflush(stdout);
    currThreadInfo->in_callback = FALSE;
}

void ThreadFini(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo)
	return;

    if (currThreadInfo->in_callback)
	return;

    currThreadInfo->in_callback = TRUE;

    printf("Finishing Thread: %d\n", ThreadID);
    fflush(stdout);
	
    llist_remove(&thread_info_list, currThreadInfo);
    CleanupThreadInfo(currThreadInfo);
    hmap_remove_abs(&thread_info_map, ThreadID);
    
    if (!expStopping)
    	finish_burst_and_discard(); 
    currThreadInfo->in_callback = FALSE;
    free(currThreadInfo);  
	 
}

void AppFini(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo)
	return;

    if (currThreadInfo->in_callback)
	return;

    currThreadInfo->in_callback = TRUE;
    printf("Finishing Process: %d\n", ThreadID);
    fflush(stdout);

    if (!expStopping)
	finish_burst_and_discard();  

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

void TriggerSyscallWait(int ThreadID, int save) {
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo)
	return;

    if (currThreadInfo->in_callback)
	return;
    
    currThreadInfo->in_callback = TRUE;

    //printf("Trigger Syscall Wait: %d\n", ThreadID);
    //fflush(stdout);
	
    if  (save) {
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;
    }

    if (trigger_syscall_wait() <= 0)
        HandleVTExpEnd(ThreadID);

    currThreadInfo->in_callback = FALSE;
}

void TriggerSyscallFinish(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if(!currThreadInfo)
	return;

    if (currThreadInfo->in_callback)
	return;
 
    currThreadInfo->in_callback = TRUE;

    //printf("Trigger Syscall Finish: %d\n", ThreadID);
    //fflush(stdout);

	
    currBurstLength = mark_burst_complete(1);
    specifiedBurstLength = currBurstLength;

    if (currBurstLength <= 0)
        HandleVTExpEnd(ThreadID);

    currThreadInfo->stack.totalBurstLength += currBurstLength;
    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;

    currThreadInfo->in_callback = FALSE;
}

void UpdateLookAhead(ThreadInfo * currThreadInfo, int64_t prev_BBID,
                    int64_t curr_BBID, int curr_BBSize) {

	
    BasicBlock * prevBBL;
    BasicBlock * currBBL;

    if (curr_BBID < 0)
	curr_BBID = -1 * curr_BBID;

    if (prev_BBID < 0)
	prev_BBID = -1 * prev_BBID;

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
    }
}

void markCurrBBL(int ThreadID) {
    ThreadInfo * currThreadInfo;
    currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo)
	return;

    if (currThreadInfo->in_callback)
	return;

    currThreadInfo->in_callback = TRUE;
    //printf("mark Curr BBL: %d\n", ThreadID);
    //fflush(stdout);

	
    if (currBBID != 0) {
        BasicBlock * relevantBBL = AllotBasicBlockEntry(currThreadInfo,
                                                        currBBID);
        relevantBBL->isMarked = 1;
        relevantBBL->BBLSize = currBBSize;
        relevantBBL->lookahead_value = currBBSize;
    }

    currThreadInfo->in_callback = FALSE;

}

void BasicBlockCallback(ThreadInfo * currThreadInfo, int bblID) {

    BasicBlock * bbl;

    bbl = AllotBasicBlockEntry(currThreadInfo, bblID);
    bbl->BBLSize = currBBSize;

    if (!alwaysOn) { 
        // set lookahead here for this Thread ...
    }    
    SignalBurstCompletion(currThreadInfo, 1);
}

void vtCallbackFn() {

    int ThreadID;
    ThreadInfo * currThreadInfo;
    //return;
    if (!vtInitializationComplete) {
	// operating without vt management
	alwaysOn = 0;
	if (currBurstLength <= 0) {	
		currBurstLength = 1000;
		specifiedBurstLength = currBurstLength;
		//printf("Here Now blah blah ...\n");
	}
        return;
    }	
     
    if (alwaysOn) {
        //ThreadID = syscall(SYS_gettid);
        //currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
        //UpdateLookAhead(ThreadID, prevBBID, currBBID, currBBSize);
        //if (currThreadInfo->stack.totalBurstLength >= FLIP_ALWAYS_ON_THRESHOLD)
        //	alwaysOn = 0;
	    alwaysOn = 0;
    }

  
    
    if (currBurstLength <= 0) {

	ThreadID = syscall(SYS_gettid);
	currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);

	if (currThreadInfo != NULL) {
		currThreadInfo->in_callback = TRUE;
		
        	BasicBlockCallback(currThreadInfo, currBBID);
		currThreadInfo->in_callback = FALSE;
	}
	
    }
	

    prevBBID = currBBID;  
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

    printf ("Tracer-ID String: %s, Timeline-ID String %s\n", tracer_id_str, timeline_id_str);
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
	printf("Tracer registration EXP_CS complete. TimelineID = %d, Return = %d\n", timeline_id, ret);
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
