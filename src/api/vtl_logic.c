#include <stdio.h>
#include <linkedlist.h>
#include <hashmap.h>
#include <stdlib.h>
#include <time.h>
#include "VT_functions.h"
#include "utility_functions.h"
#include "vtl_logic.h"

long long currBurstLength = 0;
long long currBBID = 0;
long long prevBBID = 0;
long long currBBSize = 0;
int alwaysOn = 1;
int globalTracerID = -1;
int globalTimelineID = -1;
hashmap thread_info_map;
llist thread_info_list;

#define FLIP_ALWAYS_ON_THRESHOLD 100000000


extern void ns_2_timespec(s64 nsec, struct timespec * ts);
extern void ns_2_timeval(s64 nsec, struct timeval * tv);


void SleepForNS(int ThreadID, int64_t duration) {

    if (duration <= 0)
        return;

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    currThreadInfo->stack.currBBID = currBBID;
    currThreadInfo->stack.prevBBID = prevBBID;
    currThreadInfo->stack.currBBSize = currBBSize;
    currThreadInfo->stack.alwaysOn = alwaysOn;

    currBurstLength = vt_sleep_for(duration);

    if (currBurstLength <= 0)
        HandleVTExpEnd();

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;

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

void HandleVTExpEnd() {

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
    hmap_destroy(&relevantThreadInfo->lookahead_map);
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
        currThreadInfo->stack.alwaysOn = 1;
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

    assert(currThreadInfo != NULL);
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

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    if (save) {
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;
    }

    assert(release_worker() >= 0);
}

void ForceCompleteBurst(int ThreadID, int save) {
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    if (save) {
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;
    }

    currBurstLength = mark_burst_complete(0);

    if (currBurstLength <= 0)
        HandleVTExpEnd();

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;
}

void SignalBurstCompletion(ThreadInfo * currThreadInfo, int save) {

    
    assert(currThreadInfo != NULL);

    // save globals

    if (save) {  
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;  
    }

    currBurstLength = finish_burst();

    if (currBurstLength <= 0)
        HandleVTExpEnd();

    currThreadInfo->stack.totalBurstLength += currBurstLength;

    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;
}

void AfterForkInChild(int ThreadID) {
    currBurstLength = 0;
    currBBID = 0;
    prevBBID = 0;
    currBBSize = 0;
    alwaysOn = 1;
    // destroy existing hmap
    ThreadInfo * currThreadInfo;
    while(llist_size(&thread_info_list)) {
        currThreadInfo = llist_pop(&thread_info_list);
        if (currThreadInfo) {
            CleanupThreadInfo(currThreadInfo);
            hmap_remove_abs(&thread_info_map, currThreadInfo->pid);
            free(currThreadInfo);
        }
    }

    currThreadInfo = AllocThreadInfo(ThreadID);

    assert(globalTracerID != -1);
    add_to_tracer_sq(globalTracerID);
    SignalBurstCompletion(currThreadInfo, 1);
}

void ThreadStart(int ThreadID) {

    AllocThreadInfo(ThreadID);
    assert(globalTracerID != -1);
    add_to_tracer_sq(globalTracerID);
    SignalBurstCompletion(ThreadID, 0);
}

void ThreadFini(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);
    llist_remove(&thread_info_list, currThreadInfo);
    CleanupThreadInfo(currThreadInfo);
    hmap_remove_abs(&thread_info_map, ThreadID);
    free(currThreadInfo);
    finish_burst_and_discard();    
}

void AppFini(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);

    if (currThreadInfo) {
        while(llist_size(&thread_info_list)) {
            currThreadInfo = llist_pop(&thread_info_list);
            if (currThreadInfo) {
                CleanupThreadInfo(currThreadInfo);
                hmap_remove_abs(&thread_info_map, currThreadInfo->pid);
                free(currThreadInfo);
            }
            
        }
        finish_burst_and_discard();  
        llist_destroy(&thread_info_list);
        hmap_destroy(&thread_info_map);
    }
}

void TriggerSyscallWait(int ThreadID, int save) {
    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    if  (save) {
        currThreadInfo->stack.currBBID = currBBID;
        currThreadInfo->stack.prevBBID = prevBBID;
        currThreadInfo->stack.currBBSize = currBBSize;
        currThreadInfo->stack.alwaysOn = alwaysOn;
    }

    if (trigger_syscall_wait() <= 0)
        HandleVTExpEnd();
}

void TriggerSyscallFinish(int ThreadID) {

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    currBurstLength = mark_burst_complete(1);

    if (currBurstLength <= 0)
        HandleVTExpEnd();

    currThreadInfo->stack.totalBurstLength += currBurstLength;
    // restore globals
    alwaysOn = currThreadInfo->stack.alwaysOn;
    currBBID = currThreadInfo->stack.currBBID;
    prevBBID = currThreadInfo->stack.prevBBID;
    currBBSize = currThreadInfo->stack.currBBSize;
}

void UpdateLookAhead(ThreadInfo * currThreadInfo, int64_t prev_BBID,
                    int64_t curr_BBID, int curr_BBSize) {

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
    }
}

void markCurrBBL(int ThreadID) {
    ThreadInfo * currThreadInfo;
    currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    if (currBBID != 0) {
        BasicBlock * relevantBBL = AllotBasicBlockEntry(currThreadInfo,
                                                        currBBID);
        assert(relevantBBL != NULL);
        relevantBBL->isMarked = 1;
        relevantBBL->BBLSize = currBBSize;
        relevantBBL->lookahead_value = currBBSize;
    }

}

void BasicBlockCallback(ThreadInfo * currThreadInfo, int bblID) {

    BasicBlock * bbl;

    bbl = AllotBasicBlockEntry(currThreadInfo, bblID);
    bbl->BBLSize = currBBSize;

    if (!alwaysOn) { 
        // set lookahead here for this Thread ...
    }

    // call finish_burst
    SignalBurstCompletion(currThreadInfo, 1);
}

void vtCallbackFn() {

    int ThreadID = syscall(SYS_gettid);
    ThreadInfo * currThreadInfo;
    
    if (alwaysOn) {
        currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
        UpdateLookAhead(ThreadID, prevBBID, currBBID, currBBSize);
    }

    if (currBurstLength <= 0) {

        currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
        assert(currThreadInfo != NULL);

        if (currThreadInfo->stack.totalBurstLength >= FLIP_ALWAYS_ON_THRESHOLD)
            alwaysOn = 0;

        BasicBlockCallback(currThreadInfo, currBBID);
    }
    prevBBID = currBBID;
}


void initialize_vt_management() {

    char * tracer_id_str = getenv("VT_TRACER_ID");
    char * timeline_id_str = getenv("VT_TIMELINE_ID");
    char * exp_type_str = getenv("VT_EXP_TYPE");
    int tracer_id;
    int timeline_id;
    int exp_type;
    int my_pid = syscall(SYS_gettid);

    if (!tracer_id_str) {
        printf("Missing Tracer ID !\n");
        exit(EXIT_FAILURE);
    }

    if (!exp_type_str){
        printf("Missing Exp Type\n");
        exit(EXIT_FAILURE);
    }

    tracer_id = atoi(tracer_id_str);
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

    if (!timeline_id_str) {
        register_tracer(tracer_id, EXP_CBE, 0);
    } else {
        timeline_id = atoi(timeline_id_str);
        if (timeline_id < 0) {
            printf("Timeline ID must be >= 0. Received Value: %d\n",
                    timeline_id);
            exit(EXIT_FAILURE);
        }
        register_tracer(tracer_id, EXP_CS, timeline_id);
        globalTimelineID = timeline_id;
    }

    if (add_to_tracer_sq(tracer_id) < 0) {
        printf("Failed To add tracee to tracer schedule queue for pid: %d\n",
                my_pid);
    }

    globalTracerID = tracer_id;

    // Initializing helper data structures
    hmap_init(&thread_info_map, 1000);
    llist_init(&thread_info_list);
} 