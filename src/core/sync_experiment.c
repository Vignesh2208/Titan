#include "vt_module.h"
#include "dilated_timer.h"
#include "utils.h"
#include "sync_experiment.h"
#include "common.h"

/** EXTERN VARIABLES **/

extern int tracer_num;
extern int EXP_CPUS;
extern int TOTAL_CPUS;
extern int total_num_timelines;
extern int total_completed_rounds;
extern int experiment_status;
extern int experiment_type;
extern int initialization_status;
extern s64 expected_time;
extern s64 virt_exp_start_time;
extern int current_progress_n_rounds;
extern int total_expected_tracers;

// pointers
extern int * per_timeline_chain_length;
extern llist * per_timeline_tracer_list;
extern struct task_struct ** chaintask;
extern timeline * timeline_info;
extern wait_queue_head_t *timeline_wqueue;
extern wait_queue_head_t *timeline_worker_wqueue;
extern wait_queue_head_t * syscall_wait_wqueue;


// locks
extern struct mutex exp_lock;

// hashmaps
extern hashmap get_tracer_by_id;
extern hashmap get_tracer_by_pid;
extern hashmap get_dilated_task_struct_by_pid;

// atomic variables
extern atomic_t n_waiting_tracers;
extern atomic_t n_workers_running;



/** LOCAL DECLARATIONS **/
int PerTimelineWorker(void *data);
int* values;
static wait_queue_head_t progress_call_proc_wqueue;
wait_queue_head_t progress_sync_proc_wqueue;



void InitiateExperimentStopOperation() {
    int i;
    BUG_ON(experiment_status != STOPPING);
    current_progress_n_rounds = 0;
    PDEBUG_I("InitiateExperimentStopOperation: Cleaning experiment via catchup task. "
                "Waiting for all timeline workers to exit...\n");
    BUG_ON(atomic_read(&n_workers_running) != 0);
    atomic_set(&n_workers_running, total_num_timelines);
    for (i = 0; i < total_num_timelines; i++) {
        /* chaintask refers to per_cpu_worker */
        timeline_info[i].nxt_round_burst_length = 0;
        if (wake_up_process(chaintask[i]) == 1) {
            PDEBUG_V("InitiateExperimentStopOperation: Sync thread %d wake up\n", i);
        } else {
            while (wake_up_process(chaintask[i]) != 1);
            PDEBUG_V(
                "InitiateExperimentStopOperation: Sync thread %d already running\n", i);
        }
    }
    wait_event_interruptible(progress_call_proc_wqueue,
                             atomic_read(&n_workers_running) == 0);
    PDEBUG_A("InitiateExperimentStopOperation: all timeline workers exited !\n");
    preempt_disable();
    local_irq_disable();
    for (i = 0; i < total_num_timelines; i++) {
        DilatedHrtimerRunQueuesFlush(i);
    }
    local_irq_enable();
    preempt_enable();
    CleanExp();
}

void WaitForAllProcessesResumedFromDilatedTimersOn(tracer * tracer_entry) {
    if (!tracer_entry)
        return;

    lxc_schedule_elem * curr_elem;
    llist_elem * head;
    head = tracer_entry->schedule_queue.head;

    while (head != NULL) {
        if (head) {
            curr_elem = (lxc_schedule_elem *)head->item;
            if (curr_elem && curr_elem->curr_task->resumed_by_dilated_timer) {
                
                if (curr_elem == tracer_entry->last_run) {
                    tracer_entry->last_run->quanta_left_from_prev_round = 0;
                    tracer_entry->last_run = NULL;
                }
                
                wait_event_interruptible(
                    curr_elem->curr_task->d_task_wqueue,
                    curr_elem->curr_task->ready == 1
                    && curr_elem->curr_task->syscall_waiting == 0);
                curr_elem->curr_task->resumed_by_dilated_timer = 0;
                PDEBUG_I("Resuming after waiting for process: %d resumed by dilated timer\n",
                    curr_elem->curr_task->pid);
            }
            head = head->next;
        }
    }
}

void WaitForAllProcessesResumedFromDilatedTimers(int timelineID) {
    llist_elem * head;
    llist * tracer_list;
    tracer * curr_tracer;
    s64 target_increment;

    tracer_list =  &per_timeline_tracer_list[timelineID];
    head = tracer_list->head;


    while (head != NULL) {
        curr_tracer = (tracer*)head->item;
        //GetTracerStructWrite(curr_tracer);
        WaitForAllProcessesResumedFromDilatedTimersOn(curr_tracer);
        //PutTracerStructWrite(curr_tracer);
        head = head->next;
    }
}


void RoundSynchronization() {

    int i;
    ktime_t duration;

    BUG_ON(experiment_type != EXP_CBE || total_num_timelines != EXP_CPUS);
    BUG_ON(experiment_status != RUNNING);	
    while(1) {

        BUG_ON(atomic_read(&n_workers_running) != 0);
        /* wait up each synchronization worker thread,
        then wait til they are all done */
        if (total_num_timelines > 0 && tracer_num  > 0) {

            PDEBUG_V("$$$$$$$$$$$$$$$$$$$$$$$ round_sync_task: "
                    "Round %d Starting. Waking up worker threads "
                    "$$$$$$$$$$$$$$$$$$$$$$$$$$\n", total_completed_rounds);
            atomic_set(&n_workers_running, total_num_timelines);

            for (i = 0; i < total_num_timelines; i++) {

                /* chaintask refers to per_cpu_worker */
                if (wake_up_process(chaintask[i]) == 1) {
                    PDEBUG_V("round_sync_task: Sync thread %d wake up\n", i);
                } else {
                    while (wake_up_process(chaintask[i]) != 1) { msleep(50); }
                    PDEBUG_V(
                        "round_sync_task: Sync thread %d already running\n", i);
                }
            }			
            PDEBUG_V(
                "round_sync_task: Waiting for PerTimelineWorkers to finish.\n");
            wait_event_interruptible(progress_call_proc_wqueue,
                                     atomic_read(&n_workers_running) == 0);
            PDEBUG_V("round_sync_task: All PerTimelineWorkers finished\n");
        }
        #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
        duration.tv64 = timeline_info[i].nxt_round_burst_length;
        #else
        duration = timeline_info[i].nxt_round_burst_length;
        #endif
        preempt_disable();
        local_irq_disable();
        for (i = 0; i < total_num_timelines; i++) {
            IncTimerbaseClock(i, duration); 
            DilatedHrtimerRunQueues(i);
        }
        local_irq_enable();
        preempt_enable();

        for (i = 0; i < total_num_timelines; i++) {
            WaitForAllProcessesResumedFromDilatedTimers(i);
        }
        

        if (current_progress_n_rounds <= 1) {
            current_progress_n_rounds = 0;
            total_completed_rounds++;
            PDEBUG_V("round_sync_task: Finished !\n");
            break;
        } else {
            current_progress_n_rounds --;
            total_completed_rounds++;
            continue;
        }
    }
}




/***
* Progress experiment for specified number of rounds
* write_buffer: <number of rounds>
***/
int ProgressBy(s64 progress_duration, int num_rounds) {

    
    int i = 0;


    if (experiment_status == NOTRUNNING || experiment_type != EXP_CBE)
        return FAIL;

    if (progress_duration <= 0 || num_rounds <= 0)
        return FAIL;

    for (i = 0; i < total_num_timelines; i++) {
        timeline_info[i].nxt_round_burst_length = progress_duration;
        timeline_info[i].status = 0;
    }
    current_progress_n_rounds = num_rounds;

    
    if (experiment_status == FROZEN) {
        experiment_status = RUNNING;
    }

    PDEBUG_V("ProgressBy for fixed windows initiated."
            "Window size = %lu, Num rounds = %d\n",
        progress_duration, current_progress_n_rounds);

    RoundSynchronization();
    PDEBUG_V("Previously initiated ProgressBy finished !\n");
    for (i = 0; i < total_num_timelines; i++) {
        timeline_info[i].nxt_round_burst_length = 0;
        timeline_info[i].status = 0;
    }
    current_progress_n_rounds = 0;
    return SUCCESS;
}


int ProgressTimelineBy(int timeline_id, s64 progress_duration) {

    timeline * curr_timeline;
    if (timeline_id < 0 || timeline_id >= total_num_timelines) {
        PDEBUG_A("Incorrect timeline ID: %d\n", timeline_id);
        return FAIL;
    }

    if (experiment_status == NOTRUNNING || experiment_type != EXP_CS)
        return FAIL;

    if (progress_duration <= 0)
        return FAIL;

    curr_timeline = &timeline_info[timeline_id];
    curr_timeline->nxt_round_burst_length = progress_duration;
    curr_timeline->status = 0;

    if (experiment_status == FROZEN) {
        experiment_status = RUNNING;
    }

    struct DilatedTimerTimelineBase * base = get_timeline_base(timeline_id);

    PDEBUG_V("$$$$$$$$$$$$$$$$$$$$$$ ProgressTimelineBy for fixed duration initiated. "
            "Duration= %lu, timeline_id = %d. Curr time: %llu $$$$$$$$$$$$$$$$$$$$$$\n",
        progress_duration, timeline_id, base->curr_virtual_time);

    if (wake_up_process(chaintask[timeline_id]) == 1) {
        PDEBUG_V("ProgressTimelineBy: Sync thread %d wake up\n", timeline_id);
    } else {
        while (wake_up_process(chaintask[timeline_id]) != 1) { msleep(50); }
        PDEBUG_V(
            "ProgressTimelineBy: Sync thread %d already running\n", timeline_id);
    }
    wait_event_interruptible(*curr_timeline->w_queue,
                 curr_timeline->status == 1);

    #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
    ktime_t duration = {.tv64 = progress_duration};
    #else
    ktime_t duration = progress_duration;
    #endif
    preempt_disable();
    local_irq_disable();
    IncTimerbaseClock(timeline_id, duration);
    DilatedHrtimerRunQueues(timeline_id);
    local_irq_enable();
    preempt_enable();

    WaitForAllProcessesResumedFromDilatedTimers(timeline_id);

    curr_timeline->status = 0;
    curr_timeline->nxt_round_burst_length = 0;

    PDEBUG_V("$$$$$$$$$$$$$$$$$$$$$$ ProgressTimelineBy finished for"
            " timeline_id = %d $$$$$$$$$$$$$$$$$$$$$$\n", timeline_id);
    return SUCCESS;
}


void FreeAllTracers() {
    int i;
    llist_elem * head;
    lxc_schedule_elem * curr_elem;
    PDEBUG_V("Freeing %d previous tracers and their children !\n", tracer_num);
    for (i = 1; i <= tracer_num; i++) {
        tracer * curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
        if (!curr_tracer)
            continue;

        head = curr_tracer->schedule_queue.head;
        while (head != NULL) {

            if (head) {
                curr_elem = (lxc_schedule_elem *)head->item;
                if (curr_elem) {
                    PDEBUG_V("Freeing dilated task %d\n", curr_elem->pid);
                    hmap_remove_abs(&get_dilated_task_struct_by_pid, curr_elem->pid);
                    kfree(curr_elem->curr_task);
                }
            }
            head = head->next;
        }
        
        if (curr_tracer) {
            PDEBUG_V("Removing Tracer-Pid = %d\n", curr_tracer->tracer_pid);

            hmap_remove_abs(&get_tracer_by_pid, curr_tracer->tracer_pid);

            PDEBUG_V("Freeing Tracer: %d\n", i);
            hmap_remove_abs(&get_tracer_by_id, i);

            PDEBUG_V("Freeing Tracer: %d. Pid = %d\n", i, curr_tracer->tracer_pid);
            FreeTracerEntry(curr_tracer);
        }
    }

    hmap_destroy(&get_tracer_by_id);
    hmap_destroy(&get_tracer_by_pid);
    hmap_destroy(&get_dilated_task_struct_by_pid);

    PDEBUG_V("Freeing previously allotted experiment components !\n");
    kfree(timeline_wqueue);
    kfree(timeline_worker_wqueue);
    kfree(syscall_wait_wqueue);
    kfree(timeline_info);

    FreeGlobalDilatedTimerTimelineBases(total_num_timelines);
}




int InitializeExperimentComponents(int exp_type, int num_timelines,
                                   int num_expected_tracers) {

    int i;
    int j;
    int num_required_pages;
    int num_prev_alotted_pages;

    PDEBUG_V("Entering Experiment Initialization\n");
    if (initialization_status == INITIALIZED) {
        PDEBUG_I("Experiment Already initialized !\n");
        return FAIL;
    }

    if (exp_type != EXP_CBE && exp_type != EXP_CS) {
        PDEBUG_I("Unknown Experiment Type !\n");
        return FAIL;
    }

    if (num_expected_tracers <= 0) {
        PDEBUG_I("Num expected tracers must be positive !\n");
        return FAIL;
    }

    if (exp_type == EXP_CS && num_timelines <= 0) {
        PDEBUG_I("Number of Timelines must be positive !\n");
        return FAIL;
    }

    if (tracer_num > 0) {
        // Free all tracer structs from previous experiment
        FreeAllTracers();        
    }

    tracer_num = 0;
    experiment_type = exp_type;

    if (experiment_type == EXP_CBE)
        total_num_timelines = EXP_CPUS;
    else
        total_num_timelines = num_timelines;
    
    total_completed_rounds = 0;

    timeline_wqueue = kmalloc(total_num_timelines * sizeof(wait_queue_head_t),
                              GFP_KERNEL);

    timeline_worker_wqueue = kmalloc(
        total_num_timelines * sizeof(wait_queue_head_t), GFP_KERNEL);

    syscall_wait_wqueue = kmalloc(
        total_num_timelines * sizeof(wait_queue_head_t), GFP_KERNEL);

    if (!timeline_wqueue || !timeline_worker_wqueue) {
        PDEBUG_E("Error: Could not allot memory for timeline wait queue\n");
        return -ENOMEM;
    }

    if (!syscall_wait_wqueue) {
        PDEBUG_E("Error: Could not allot memory for syscall wait queue\n");
        return -ENOMEM;
    }

    for (i = 0; i < total_num_timelines; i++) {
        init_waitqueue_head(&timeline_wqueue[i]);
        init_waitqueue_head(&timeline_worker_wqueue[i]);
        init_waitqueue_head(&syscall_wait_wqueue[i]);
    }

    InitGlobalDilatedTimerTimelineBases(total_num_timelines);

    per_timeline_chain_length =
        (int *) kmalloc(total_num_timelines * sizeof(int), GFP_KERNEL);
    per_timeline_tracer_list =
        (llist *) kmalloc(total_num_timelines * sizeof(llist), GFP_KERNEL);
    values = (int *)kmalloc(total_num_timelines * sizeof(int), GFP_KERNEL);
    chaintask = kmalloc(total_num_timelines * sizeof(struct task_struct*),
                        GFP_KERNEL);

    

    if (!per_timeline_tracer_list || !per_timeline_chain_length || !values ||
        !chaintask) {
        PDEBUG_E("Error Allocating memory for per timeline structures.\n");
        BUG();
    }

    timeline_info = (timeline *) kmalloc(total_num_timelines *sizeof(timeline),
                                         GFP_KERNEL);
    if (!timeline_info) {
        PDEBUG_E("Error Allocating memory for timeline info structures \n");
        BUG();
    }

    for (i = 0; i < total_num_timelines; i++) {
        llist_init(&per_timeline_tracer_list[i]);
        timeline_info[i].timeline_id = i;
        timeline_info[i].w_queue = &timeline_worker_wqueue[i];
        timeline_info[i].status = 0;
        timeline_info[i].nxt_round_burst_length = 0;
        timeline_info[i].stopping = 0;
        per_timeline_chain_length[i] = 0;
            values[i] = i;
    }


        virt_exp_start_time = 0;

    mutex_init(&exp_lock);
    hmap_init(&get_tracer_by_id, "int", 0);
    hmap_init(&get_tracer_by_pid, "int", 0);
    hmap_init(&get_dilated_task_struct_by_pid, "int", 0);

    init_waitqueue_head(&progress_call_proc_wqueue);
    init_waitqueue_head(&progress_sync_proc_wqueue);


    atomic_set(&n_workers_running, 0);
    atomic_set(&n_waiting_tracers, 0);

    PDEBUG_V("Init experiment components: Initialized Variables\n");

    
    for (i = 0; i < total_num_timelines; i++) {
        PDEBUG_A("Init experiment components: Adding Timeline Worker %d\n", i);
        chaintask[i] = kthread_create(&PerTimelineWorker, &values[i],
                                    "PerTimelineWorker");
        if (!IS_ERR(chaintask[i])) {
            kthread_bind(chaintask[i], i % EXP_CPUS);
            wake_up_process(chaintask[i]);
            PDEBUG_A("Timeline Worker %d: Pid = %d\n", i,
                        chaintask[i]->pid);
        }
    }


    experiment_status = NOTRUNNING;
    initialization_status = INITIALIZED;
        total_expected_tracers = num_expected_tracers;
        

    PDEBUG_V("Init experiment components: Finished\n");
    return SUCCESS;

}

int CleanupExperimentComponents() {

    int i = 0;

    if (initialization_status == NOT_INITIALIZED) {
        PDEBUG_I("Experiment Already Cleaned up ...\n");
        return FAIL;
    }

    PDEBUG_I("Cleaning up experiment components ...\n");

        virt_exp_start_time = 0;

    atomic_set(&n_workers_running, 0);
    atomic_set(&n_waiting_tracers, 0);


    for (i = 0; i < total_num_timelines; i++) {
        llist_destroy(&per_timeline_tracer_list[i]);
    }

    kfree(per_timeline_tracer_list);
    kfree(per_timeline_chain_length);
    kfree(values);
    kfree(chaintask);
    total_num_timelines = 0;
    initialization_status = NOT_INITIALIZED;
    experiment_status = NOTRUNNING;
    experiment_type = NOT_SET;
    return SUCCESS;
}

int SyncAndFreeze() {

    int i;
    int j;
    u32 flags;
    ktime_t now;
    tracer * curr_tracer;
    int ret;

    struct timeval ktv;


    if (initialization_status != INITIALIZED
        || experiment_status != NOTRUNNING || total_expected_tracers <= 0) {
        return FAIL;
    }


    PDEBUG_A("Sync And Freeze: ** Starting Experiment Synchronization **\n");
    PDEBUG_A("Sync And Freeze: expected number of tracers: %d\n",
             total_expected_tracers);

    wait_event_interruptible(progress_sync_proc_wqueue,
        atomic_read(&n_waiting_tracers) == total_expected_tracers);


    if (tracer_num <= 0) {
        PDEBUG_A("Sync And Freeze: Nothing added to experiment, dropping out\n");
        return FAIL;
    }


    if (tracer_num != total_expected_tracers) {
        PDEBUG_A("Sync And Freeze: Expected number of tracers: %d not present."
                 " Actual number of registered tracers: %d\n",
                 total_expected_tracers, tracer_num);
        return FAIL;
    }



    if (experiment_status != NOTRUNNING) {
        PDEBUG_A("Sync And Freeze: Trying to Sync Freeze "
                 "when an experiment is already running!\n");
        return FAIL;
    }

    //now = ktime_get_real();
    now = 100000000000;
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
    ktv = ns_to_timeval(now.tv64);
    now.tv64 = ktv.tv_sec*1000000000;
    virt_exp_start_time = now.tv64;
    #else
    ktv = ns_to_timeval(now);
    now = ktv.tv_sec*1000000000;
    virt_exp_start_time = now;
    #endif

    for (i = 1; i <= tracer_num; i++) {
        curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
        GetTracerStructWrite(curr_tracer);
        if (curr_tracer) {
            PDEBUG_A("Sync And Freeze: "
                     "Setting Virt time for Tracer %d and its children\n", i);
            #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
            SetChildrenTime(curr_tracer, now.tv64, 0);
            #else
            SetChildrenTime(curr_tracer, now, 0);
            #endif
            curr_tracer->round_start_virt_time = virt_exp_start_time;
            curr_tracer->curr_virtual_time = virt_exp_start_time;
            curr_tracer->main_task->virt_start_time = virt_exp_start_time;
            curr_tracer->main_task->curr_virt_time = virt_exp_start_time;
            curr_tracer->main_task->wakeup_time = 0;
            curr_tracer->main_task->burst_target = 0;
        }
        PutTracerStructWrite(curr_tracer);
    }

    for (i = 0; i < total_num_timelines; i++) {
        SetTimerbaseClock(i, now);
    }
    experiment_status = FROZEN;
    PDEBUG_A("Finished Sync and Freeze\n");


    return SUCCESS;

}


int PerTimelineWorker(void *data) {

    int round = 0;
    int timeline_id = *((int *)data);
    tracer * curr_tracer;
    llist_elem * head;
    llist * tracer_list;
    s64 now;
    struct timeval ktv;
    ktime_t ktime;
    timeline * curr_timeline = &timeline_info[timeline_id];

    set_current_state(TASK_INTERRUPTIBLE);

    PDEBUG_I("#### PerTimelineWorker: Started per timeline_id worker thread "
             "for tracers alotted to timeline_id = %d\n", timeline_id);
    tracer_list =  &per_timeline_tracer_list[timeline_id];

    /* if it is the very first round, don't try to do any work, just rest */
    if (round == 0) {
        PDEBUG_I("#### PerTimelineWorker: For tracers alotted to timeline %d."
                 " Waiting to be woken up !\n", timeline_id);
        goto startWork;
    }

    while (!kthread_should_stop()) {

        if (experiment_status == STOPPING) {
            
            atomic_dec(&n_workers_running);
            PDEBUG_I("#### PerTimelineWorker: Stopping. Sending wake up from "
                     "worker process for tracers on timeline %d.\n",
                     timeline_id);
            curr_timeline->status = 1;
            TriggerStackThreadExecutionsOn(timeline_id, 0, STACK_THREAD_EXP_EXIT);
            wake_up_interruptible(&progress_call_proc_wqueue);
            return 0;
        }

        //WakeUpProcessesWaitingOnSyscalls(timeline_id);
        head = tracer_list->head;

        
        while (head != NULL) {

            curr_tracer = (tracer *)head->item;
            GetTracerStructRead(curr_tracer);
            curr_tracer->nxt_round_burst_length 
                = curr_timeline->nxt_round_burst_length;

            
            ResetAllStackRxLoopStatus(curr_tracer);
            if (ScheduleListSize(curr_tracer) > 0
                && curr_tracer->nxt_round_burst_length) {

                // remove any unclaimed pkt information from previous progress
                CleanUpPktInfoQueue(curr_tracer);

                PDEBUG_V("PerTimelineWorker: Called "
                         "UnFreeze Proc Recurse for tracer: %d on "
                         "timeline: %d\n", curr_tracer->tracer_id, timeline_id);
                UnfreezeProcExpRecurse(curr_tracer);
                PDEBUG_V("PerTimelineWorker: Finished Unfreeze Proc Recurse "
                         "for tracer: %d on timeline: %d\n",
                         curr_tracer->tracer_id, timeline_id);

            } else if (ScheduleListSize(curr_tracer) == 0) {
                curr_tracer->curr_virtual_time += curr_tracer->nxt_round_burst_length;
            }
            PutTracerStructRead(curr_tracer);
            head = head->next;
        }

        UpdateAllTracersVirtualTime(timeline_id);
        WakeUpProcessesWaitingOnSyscalls(timeline_id, curr_timeline->nxt_round_burst_length);
        TriggerStackThreadExecutionsOn(timeline_id,  curr_timeline->nxt_round_burst_length, STACK_THREAD_CONTINUE);
        PDEBUG_V("PerTimelineWorker: Finished round for timeline %d\n",
                timeline_id);
        /* when the first task has started running, signal you are done working,
        and sleep */
        round++;
        set_current_state(TASK_INTERRUPTIBLE);

        if (experiment_type == EXP_CBE) 
            atomic_dec(&n_workers_running);
        
        curr_timeline->status = 1;
        
        PDEBUG_V(
            "#### PerTimelineWorker: Sending wake up from "
            "PerTimelineWorker on behalf of all Tracers on timeline %d\n",
            timeline_id);
        if (experiment_type == EXP_CBE)
            wake_up_interruptible(&progress_call_proc_wqueue);
        else
            wake_up_interruptible(curr_timeline->w_queue);

startWork:
        schedule();
        set_current_state(TASK_RUNNING);
        PDEBUG_V("#### PerTimelineWorker: Woken up for Tracers on "
                 "timeline %d\n", timeline_id);

    }
    return 0;
}




/**
 * Searches an tracer for the process with given pid. returns success if found
**/
struct task_struct * SearchTracer(struct task_struct * aTask, int pid) {


    struct list_head *list;
    struct task_struct *taskRecurse;
    struct task_struct *me;
    struct task_struct *t;


    if (aTask == NULL) {
        PDEBUG_E("Search lxc: Task does not exist\n");
        return NULL;
    }

    if (aTask->pid == 0) {
        PDEBUG_E("Search lxc: pid 0 error\n");
        return NULL;
    }

    me = aTask;
    t = me;

    if (t->pid == pid)
        return t;

    do {
        if (t->pid == pid) {
            return t;
        }
    } while_each_thread(me, t);

    list_for_each(list, &aTask->children) {
        taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
            continue;
        }
        t =  SearchTracer(taskRecurse, pid);
        if (t != NULL)
            return t;
    }

    return NULL;
}

void PruneTracerQueue(tracer * curr_tracer, int is_schedule_queue){
    struct pid *pid_struct;
    struct task_struct *task;
    llist_elem *head ;
    lxc_schedule_elem *curr_elem;
    llist * queue;
    int n_checked_processes = 0;
    int n_total_processes = 0;
    int n_run_queue_processes = 0;


    if (!curr_tracer)
        return;

    if (!is_schedule_queue)
        queue = &curr_tracer->run_queue;
    else
        queue = &curr_tracer->schedule_queue;

    curr_elem = NULL;
    PDEBUG_V("Clean up irrelevant processes: Entered.\n");

    n_total_processes = llist_size(queue);
    PDEBUG_V("Clean up irrelevant processes: "
             "Entered. n_total_processes: %d\n", n_total_processes);

    while (n_checked_processes < n_total_processes) {
        curr_elem = (lxc_schedule_elem *)llist_get(queue, 0);

        if (!curr_elem)
            return;
        task = SearchTracer(curr_tracer->main_task->base_task, curr_elem->pid);
        if (!task){
            PDEBUG_I("Clean up irrelevant processes: "
                     "Curr elem: %d. Task is dead\n", curr_elem->pid);
            PutTracerStructRead(curr_tracer);
            GetTracerStructWrite(curr_tracer);
            if (!is_schedule_queue)
                PopRunQueue(curr_tracer);
            else
                PopScheduleList(curr_tracer);

            PutTracerStructWrite(curr_tracer);

            // Inform any associated stack threads for this pid that the process has exited
            TriggerAllStackThreadExecutions(curr_tracer, 0, STACK_THREAD_PROCESS_EXIT, curr_elem->pid); 
            GetTracerStructRead(curr_tracer);
            
        } else {
            PutTracerStructRead(curr_tracer);
            GetTracerStructWrite(curr_tracer);

            
            BUG_ON(!curr_elem->curr_task);
            if (!is_schedule_queue) {

                if (curr_elem->curr_task->ready == 0) {
                    // This task was blocked in previous run.
                    // Remove from run queue
                    curr_elem->blocked = 1;
                    WARN_ON(curr_elem->curr_task->burst_target != 0);
                    curr_elem->curr_task->burst_target = 0;
                    curr_elem->quanta_curr_round = 0;
                    curr_elem->quanta_left_from_prev_round = 0;
                    PopRunQueue(curr_tracer);
                    
                } else {
                    WARN_ON(curr_elem->blocked != 0);
                    curr_elem->blocked = 0;
                    RequeueRunQueue(curr_tracer);
                }
            } else {
                RequeueScheduleList(curr_tracer);
            }
            PutTracerStructWrite(curr_tracer);
            GetTracerStructRead(curr_tracer);
        }
        n_checked_processes ++;
    }
}
/*
Assumes curr_tracer read lock is acquired prior to function call. Must return
with read lock still acquired.
*/
void CleanUpAllIrrelevantProcesses(tracer * curr_tracer) {
    if (curr_tracer) {
        PruneTracerQueue(curr_tracer, 0);
        PruneTracerQueue(curr_tracer, 1);
    }
}



/*
Assumes curr_tracer read lock is acquired prior to function call. Must return
with read lock still acquired.
*/
int UpdateAllRunnableTaskTimeslices(tracer * curr_tracer) {
    llist* schedule_queue = &curr_tracer->schedule_queue;
    llist * run_queue = &curr_tracer->run_queue;
    llist_elem* head = schedule_queue->head;
    llist_elem * tmp_head;
    lxc_schedule_elem* curr_elem;
    lxc_schedule_elem* last_run;
    lxc_schedule_elem* tmp_elem;
    lxc_schedule_elem* tmp;
    unsigned long flags;
    s64 total_quanta = curr_tracer->nxt_round_burst_length;
    s64 alotted_quanta = 0;
    int alteast_one_task_runnable = 0;
    int found = 0;

    PutTracerStructRead(curr_tracer);
    GetTracerStructWrite(curr_tracer);


    while (head != NULL) {
        curr_elem = (lxc_schedule_elem *)head->item;
        if (!curr_elem) {
            PDEBUG_V("Update all runnable task timeslices: "
                     "Curr elem is NULL\n");
            PutTracerStructWrite(curr_tracer);
            GetTracerStructRead(curr_tracer);
            return 0;
        }
        PDEBUG_V("Update all runnable task timeslices: "
                 "Processing Curr elem Left\n");
        PDEBUG_V("Update all runnable task timeslices: "
                 "Curr elem is %d. \n", curr_elem->pid);

    
        if (curr_elem->curr_task && curr_elem->curr_task->ready == 0) {
            curr_elem->blocked = 1;
        } else if (curr_elem->curr_task && curr_elem->curr_task->ready) {
            if (curr_elem->blocked == 1) {
                // If it was previously marked as blocked, add it to run_queue
                curr_elem->quanta_left_from_prev_round = 0;
                curr_elem->quanta_curr_round = 0;
                WARN_ON(curr_elem->curr_task->burst_target != 0);
                curr_elem->curr_task->burst_target = 0;
                curr_elem->blocked = 0;
                PDEBUG_V("Elem: %d previously marked as blocked !\n", curr_elem->pid);
            } 
            tmp_head = run_queue->head;
            found = 0;
            while (tmp_head != NULL) {
                tmp_elem = (lxc_schedule_elem *)tmp_head->item;
                if (tmp_elem->pid == curr_elem->pid) {
                    found = 1;
                    break;
                }
                tmp_head = tmp_head->next;
            }
            if (!found) {
                llist_append(&curr_tracer->run_queue, curr_elem);
            }
            alteast_one_task_runnable = 1;
        }
        head = head->next;
    }

    if (alteast_one_task_runnable) {
        PDEBUG_V("Atleast on task is runnable !\n");
    }	
    head = run_queue->head;

    if (curr_tracer->last_run != NULL
        && curr_tracer->last_run->quanta_left_from_prev_round) {

        last_run = curr_tracer->last_run;
        if (alotted_quanta + last_run->quanta_left_from_prev_round
            > total_quanta) {
            last_run->quanta_curr_round = total_quanta - alotted_quanta;
            last_run->quanta_left_from_prev_round =
                last_run->quanta_left_from_prev_round -
                    last_run->quanta_curr_round;
            alotted_quanta = total_quanta;
            curr_tracer->last_run = last_run;
            PutTracerStructWrite(curr_tracer);
            GetTracerStructRead(curr_tracer);
            return 1;
        } 
        
    }

    while (head != NULL && alotted_quanta < total_quanta) {
        curr_elem = (lxc_schedule_elem *)head->item;
        WARN_ON(curr_elem->blocked || curr_elem->curr_task->ready == 0);

        if (curr_elem->blocked || curr_elem->curr_task->ready == 0) {
            curr_elem->quanta_curr_round = 0;
            curr_elem->quanta_left_from_prev_round = 0;
            head = head->next;
            continue;
        }
        
        if (curr_elem->quanta_left_from_prev_round > 0) {
            // there should exist only one element like this

            if (alotted_quanta + curr_elem->quanta_left_from_prev_round
                > total_quanta) {
                curr_elem->quanta_curr_round = total_quanta - alotted_quanta;
                curr_elem->quanta_left_from_prev_round =
                    curr_elem->quanta_left_from_prev_round - curr_elem->quanta_curr_round;
                alotted_quanta = total_quanta;
                curr_tracer->last_run = curr_elem;
                PutTracerStructWrite(curr_tracer);
                GetTracerStructRead(curr_tracer);
                return alteast_one_task_runnable;
            } else {
                curr_elem->quanta_curr_round = curr_elem->quanta_left_from_prev_round;
                curr_elem->quanta_left_from_prev_round = 0;
                alotted_quanta += curr_elem->quanta_curr_round;
                PDEBUG_V("Finished 1 base quanta for : %d. "
                    "Base quanta length = %llu\n", curr_elem->pid,
                    curr_elem->base_quanta);
                
            }
            curr_tracer->last_run = curr_elem;
        } else {
            curr_elem->quanta_left_from_prev_round = 0;
            curr_elem->quanta_curr_round = 0;
        }
        head = head->next;
    }


    if (alotted_quanta < total_quanta && alteast_one_task_runnable) {
        if (curr_tracer->last_run == NULL)
            head = run_queue->head;
        else {
            head = run_queue->head;
            while (head != NULL) {
                tmp = (lxc_schedule_elem *)head->item;
                if (tmp == curr_tracer->last_run) {
                    head = head->next;
                    break;
                }
                head = head->next;
            }

        }

        if (head == NULL) {
            //last run task no longer exists in schedule queue
            //reset to head of schedule queue for now.
            head = run_queue->head;
        }

        while (alotted_quanta < total_quanta) {


            curr_elem = (lxc_schedule_elem *)head->item;
            if (!curr_elem) {
                return alteast_one_task_runnable;
            }

            WARN_ON(curr_elem->blocked || curr_elem->curr_task->ready == 0);

            if (curr_elem->blocked || curr_elem->curr_task->ready == 0) {
                curr_elem->quanta_curr_round = 0;
                curr_elem->quanta_left_from_prev_round = 0;
                head = head->next;
                continue;
            }

            
            if (alotted_quanta + curr_elem->base_quanta >= total_quanta) {
                curr_elem->quanta_curr_round += total_quanta - alotted_quanta;
                curr_elem->quanta_left_from_prev_round =
                    curr_elem->base_quanta - (total_quanta - alotted_quanta);
                alotted_quanta = total_quanta;
            } else {
                curr_elem->quanta_curr_round += curr_elem->base_quanta;
                curr_elem->quanta_left_from_prev_round = 0;
                alotted_quanta += curr_elem->base_quanta;
            }

            if (alotted_quanta == total_quanta) {
                curr_tracer->last_run = curr_elem;
                PDEBUG_V(
                    "Starting new base quanta for : %d. "
                    "Base quanta length = %llu. Run queue size = %d\n",
                    curr_elem->pid, curr_elem->base_quanta,
                    llist_size(run_queue));

                PutTracerStructWrite(curr_tracer);
                GetTracerStructRead(curr_tracer);
                return alteast_one_task_runnable;
            }
                
            head = head->next;
            if (head == NULL) {
                head = run_queue->head;
            }
        }
    }

    PutTracerStructWrite(curr_tracer);
    GetTracerStructRead(curr_tracer);

    return alteast_one_task_runnable;
}


/*
Assumes tracer read lock is acquired prior to call.
Must return with read lock acquired.
*/
lxc_schedule_elem * GetNextRunnableTask(tracer * curr_tracer) {
    lxc_schedule_elem * curr_elem = NULL;
    int n_checked_processes = 0;
    int n_scheduled_processes = 0;

    if (!curr_tracer)
        return NULL;

    llist* run_queue = &curr_tracer->run_queue;
    llist_elem* head = run_queue->head;


    /*if (curr_tracer->last_run != NULL 
        && curr_tracer->last_run->quanta_left_from_prev_round > 0) {
        return curr_tracer->last_run;
    }*/
    while (head != NULL) {
        curr_elem = (lxc_schedule_elem *)head->item;
        if (!curr_elem)
            return NULL;

        if (curr_elem->quanta_curr_round)
            return curr_elem;
        head = head->next;
    }

    return NULL; // all processes are blocked. nothing to run.

}

/*
Assumes that curr_tracer read lock is acquired before entry. Must return with
read lock still acquired.
*/
int UnfreezeProcExpRecurse(tracer * curr_tracer) {

    struct timeval now;
    s64 now_ns;
    s64 start_ns;
    int i = 0;
    unsigned long flags;
    s64 used_quanta = 0;
    s64 total_quanta = 0;
    s64 prev_thread_quanta;
    lxc_schedule_elem * curr_elem;
    int atleast_on_task_runnable;

    if (!curr_tracer)
        return FAIL;

    if (curr_tracer->nxt_round_burst_length == 0)
        return SUCCESS;
    
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
    now_ns = ktime_get_real().tv64;
    #else
    now_ns = ktime_get_real();
    #endif
    CleanUpAllIrrelevantProcesses(curr_tracer);
    

    total_quanta = curr_tracer->nxt_round_burst_length;

 
    atleast_on_task_runnable = UpdateAllRunnableTaskTimeslices(curr_tracer);
    
    while (used_quanta < total_quanta && ScheduleListSize(curr_tracer) > 0) {
        #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
        now_ns = ktime_get_real().tv64;
        #else
        now_ns = ktime_get_real();
        #endif

        curr_elem = GetNextRunnableTask(curr_tracer);
        if (!curr_elem)
            break;

        PutTracerStructRead(curr_tracer);
        GetTracerStructWrite(curr_tracer);
        used_quanta += curr_elem->quanta_curr_round;
        prev_thread_quanta = curr_elem->quanta_curr_round;
        curr_elem->curr_task->burst_target = curr_elem->quanta_curr_round;
        curr_elem->total_run_quanta += curr_elem->quanta_curr_round;
        curr_elem->quanta_curr_round = 0; // reset to zero
        PutTracerStructWrite(curr_tracer);

        #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
        curr_elem->curr_task->trigger_time = ktime_get_real().tv64;
        #else
        curr_elem->curr_task->trigger_time = ktime_get_real();
        #endif
        WaitForTaskCompletion(curr_tracer, curr_elem->curr_task);
        GetTracerStructRead(curr_tracer);
        curr_tracer->curr_virtual_time += prev_thread_quanta;
        
    }
    return SUCCESS;
}

/*
Assumes no tracer lock is acquired prior to call.
*/
void CleanExp() {

    int i;
    tracer * curr_tracer;

    PDEBUG_I("Clean exp: Cleaning up initiated ...");
    BUG_ON(experiment_status != STOPPING);
    mutex_lock(&exp_lock);
    for (i = 1; i <= tracer_num; i++) {

        curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
        if (curr_tracer) {
            GetTracerStructWrite(curr_tracer);
            CleanUpRunQueue(curr_tracer);
            CleanUpScheduleList(curr_tracer);
            PutTracerStructWrite(curr_tracer);
            wake_up_interruptible(curr_tracer->w_queue);
        }
    }
    mutex_unlock(&exp_lock);

    timeline_info[0].stopping = 1;
    wake_up_interruptible(timeline_info[0].w_queue);
    experiment_status = NOTRUNNING;
    PDEBUG_I("Clean exp: Cleaning up finished ...");

}

