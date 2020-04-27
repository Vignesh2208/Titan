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
    currThreadInfo->stack.currBBSize = currBBSize;
    currThreadInfo->stack.alwaysOn = alwaysOn;

    ret = vt_sleep_for(duration);

    if (ret < 0)
	    HandleVTExpEnd(ThreadID);

    currBurstLength = mark_burst_complete(1);
    specifiedBurstLength = currBurstLength;
    if (currBurstLength <= 0)
        HandleVTExpEnd(ThreadID);

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    currBBSize = currThreadInfo->stack.currBBSize;
    currThreadInfo->in_callback = FALSE;

	
}

void GetCurrentTimespec(struct timespec *ts) {
    ns_2_timespec(get_current_vt_time(), ts);
}

void GetCurrentTimeval(struct timeval * tv) {
    ns_2_timeval(get_current_vt_time(), tv);
}


s64 getPacketEAT() {
    // returns earliest arrival time of any packet intended for this tracer
    return 0;
}

long getBBLLookahead() {
    // returns shortest path lookahead from BBL
    return 0;
}

int setLookahead(s64 bulkLookaheadValue, long spLookaheadValue) {
    return 0;
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


    while(llist_size(&relevantThreadInfo->special_fds)){
        fdInfo * currfdInfo = llist_pop(
            &relevantThreadInfo->special_fds);
        if (currfdInfo)
            free(currfdInfo);
    }

    llist_destroy(&relevantThreadInfo->special_fds);
	
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
        currThreadInfo->stack.currBBID = 0;
        currThreadInfo->stack.currBBSize = 0;
        currThreadInfo->stack.alwaysOn = 1;
        llist_init(&currThreadInfo->special_fds);
        hmap_put_abs(&thread_info_map, ThreadID, currThreadInfo);
        llist_append(&thread_info_list, currThreadInfo);
    } else {
        return hmap_get_abs(&thread_info_map, ThreadID);
    }

    return currThreadInfo;
}


void YieldVTBurst(int ThreadID, int save, long syscall_number) {


    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo) 
	    return;

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

    //printf("Force Complete Burst. Paused ThreadID: %d. "
    //       "Syscall number: %d!\n", ThreadID, syscall_number);
    //fflush(stdout);

    

    if (save && currThreadInfo) {
        currThreadInfo->stack.currBBID = currBBID;
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
    	currBBSize = currThreadInfo->stack.currBBSize;
    }

    //printf("Force Complete Burst. Resumed ThreadID: %d. "
    //       "Syscall number: %d!\n", ThreadID, syscall_number);
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
    currBBSize = currThreadInfo->stack.currBBSize;
}

void AfterForkInChild(int ThreadID, int ParentProcessID) {

    // destroy existing hmap
    ThreadInfo * currThreadInfo;
    ThreadInfo * tmp;
    


    currThreadInfo = AllotThreadInfo(ThreadID, ThreadID);
     
    assert(currThreadInfo != NULL);

    currBurstLength = 0;
    currBBID = 0;
    currBBSize = 0;
    alwaysOn = 1;
    specifiedBurstLength = currBurstLength;

    currThreadInfo->in_callback = TRUE; 

    printf("Adding new child process\n");
    fflush(stdout);

    add_to_tracer_sq(globalTracerID);
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
    currBBSize = currThreadInfo->stack.currBBSize;
    currThreadInfo->in_callback = FALSE;
}


void vtCallbackFn() {

    int ThreadID;
    ThreadInfo * currThreadInfo;

    if (!vtInitializationComplete) {
	    alwaysOn = 0;
        if (currBurstLength <= 0) {	
            currBurstLength = 1000000;
            specifiedBurstLength = currBurstLength;
        }
        return;
    }	
     
    alwaysOn = 0;
    
    if (currBurstLength <= 0) {

        ThreadID = syscall(SYS_gettid);
        currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);

        if (currThreadInfo != NULL) {
            currThreadInfo->in_callback = TRUE;
            SignalBurstCompletion(currThreadInfo, 1);
            currThreadInfo->in_callback = FALSE;
        }
	
    } 
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
        ret = register_tracer(tracer_id, EXP_CBE, 0);
	printf("Tracer registration for EXP_CBE complete. Return = %d\n", ret);
        fflush(stdout);
    } else {
        timeline_id = atoi(timeline_id_str);
        if (timeline_id < 0) {
            printf("Timeline ID must be >= 0. Received Value: %d\n",
                    timeline_id);
            exit(EXIT_FAILURE);
        }
        ret = register_tracer(tracer_id, EXP_CS, timeline_id);
        globalTimelineID = timeline_id;
	printf("Tracer registration for EXP_CS complete. TimelineID = %d, Return = %d\n", timeline_id, ret);
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


/*** For Socket Handling ***/
void addSocket(int ThreadID, int sockFD, int isNonBlocking) {
    addFd(ThreadID, sockFD, FD_TYPE_SOCKET, isNonBlocking);
}


int isSocketFd(int ThreadID, int sockFD) {
    return isFdTypeMatch(ThreadID, sockFD, FD_TYPE_SOCKET);
}

int isSocketFdNonBlocking(int ThreadID, int sockFD) {
    return isFdNonBlocking(ThreadID, sockFD);
}

/*** For TimerFd Handlng ***/
void  addTimerFd(int ThreadID, int fd, int isNonBlocking) {
    addFd(ThreadID, fd, FD_TYPE_TIMERFD, isNonBlocking);
}

int isTimerFd(int ThreadID, int fd) {
    return isFdTypeMatch(ThreadID, fd, FD_TYPE_TIMERFD);
}

int isTimerFdNonBlocking(int ThreadID, int fd) {
    return isFdNonBlocking(ThreadID, fd);
}

int isTimerArmed(int ThreadID, int fd) {
    if(!isFdTypeMatch(ThreadID, fd, FD_TYPE_TIMERFD))
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
