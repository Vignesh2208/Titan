#include "includes.h"
#include "vt_module.h"
#include "sync_experiment.h"
#include "utils.h"
#include "common.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
#include <uapi/linux/sched/types.h>
#endif

extern int tracer_num;
extern int EXP_CPUS;
extern int TOTAL_CPUS;
extern int total_num_timelines;
extern struct task_struct ** chaintask;
extern int experiment_status;
extern int initialization_status;
extern s64 virt_exp_start_time;

extern struct mutex exp_lock;
extern int *per_timeline_chain_length;
extern llist * per_timeline_tracer_list;

extern hashmap get_tracer_by_id;
extern hashmap get_tracer_by_pid;
extern hashmap get_dilated_task_struct_by_pid;

extern wait_queue_head_t * timeline_wqueue;
extern wait_queue_head_t * syscall_wait_wqueue;


extern int run_usermode_tracer_spin_process(char *path, char **argv,
        char **envp, int wait);
extern void SignalCpuWorkerResume(tracer * curr_tracer);
extern s64 GetCurrentVirtualTime(struct task_struct * task);
extern int CleanupExperimentComponents(void);



struct task_struct* GetTaskNs(pid_t pid, struct task_struct * parent) {
    if (!parent)
        return NULL;
    int num_children = 0;
    struct list_head *list;
    struct task_struct *me;
    struct task_struct *t;
    me = parent;
    t = me;
    
    do {
        if (task_pid_nr_ns(t, task_active_pid_ns(t)) == pid)
            return t; 
    } while_each_thread(me, t);
    

    list_for_each(list, &parent->children) {
        struct task_struct * taskRecurse = list_entry(list,
            struct task_struct, sibling);
        if (task_pid_nr_ns(taskRecurse, task_active_pid_ns(taskRecurse)) == pid)
            return taskRecurse;
        t =  GetTaskNs(pid, taskRecurse);
        if (t != NULL)
            return t;
    }
    return NULL;
}



/***
Remove head of schedule queue and return the pid of the head element.
Assumes tracer write lock is acquired prior to call.
***/
int PopScheduleList(tracer * tracer_entry) {
    if (!tracer_entry)
        return 0;
    lxc_schedule_elem * head;
    head = llist_pop(&tracer_entry->schedule_queue);
    int pid;
    if (head != NULL) {
        pid = head->pid;
        kfree(head);
        return pid;
    }
    return 0;
}

/***
Requeue schedule queue, i.e pop from head and add to tail. Assumes tracer
write lock is acquired prior to call
***/
void RequeueScheduleList(tracer * tracer_entry) {
    if (tracer_entry == NULL)
        return;
    llist_requeue(&tracer_entry->schedule_queue);
}


/***
Remove head of schedule queue and return the task_struct of the head element.
Assumes tracer write lock is acquired prior to call.
***/
lxc_schedule_elem * PopRunQueue(tracer * tracer_entry) {
    if (!tracer_entry)
        return 0;
    return llist_pop(&tracer_entry->run_queue);
}

/***
Requeue schedule queue, i.e pop from head and add to tail. Assumes tracer
write lock is acquired prior to call
***/
void RequeueRunQueue(tracer * tracer_entry) {
    if (!tracer_entry)
        return;
    llist_requeue(&tracer_entry->run_queue);
}

/*
Assumes tracer write lock is acquired prior to call. Must return with lock
still held
*/
void CleanUpScheduleList(tracer * tracer_entry) {

    int pid = 1;
    struct pid *pid_struct;
    struct task_struct * task;
    struct dilated_task_struct * d_task;

    BUG_ON(!tracer_entry);
    
    lxc_schedule_elem* curr_elem;
    llist* schedule_queue = &tracer_entry->schedule_queue;
    llist_elem* head = schedule_queue->head;


    while (head != NULL) {

        if (head) {
            curr_elem = (lxc_schedule_elem *)head->item;
            if (curr_elem) {
                curr_elem->curr_task->associated_tracer_id = -1;
                curr_elem->curr_task->virt_start_time = 0;
                curr_elem->curr_task->curr_virt_time = 0;
                curr_elem->curr_task->wakeup_time = 0;
                curr_elem->curr_task->burst_target = 0;
            }
        }
        head = head->next;
    }

    while ( pid != 0) {
        pid = PopScheduleList(tracer_entry);
        if (pid != 0) {
            d_task = hmap_get_abs(&get_dilated_task_struct_by_pid, pid);
            if (d_task != NULL) {
                wake_up_interruptible(&d_task->d_task_wqueue);
            }
        }
    }
    tracer_entry->main_task = NULL;
}

void CleanUpRunQueue(tracer * tracer_entry) {
    if (!tracer_entry)
        return;

    while (llist_size(&tracer_entry->run_queue)) {
        PopRunQueue(tracer_entry);
    }
}





/*
Assumes tracer read lock is acquired before function call
*/
int ScheduleListSize(tracer * tracer_entry) {
    if (!tracer_entry)
        return 0;
    return llist_size(&tracer_entry->schedule_queue);
}


/*
Assumes tracer read lock is acquired before function call
*/
int RunQueueSize(tracer * tracer_entry) {
    if (!tracer_entry)
        return 0;
    return llist_size(&tracer_entry->run_queue);
}



/*
Assumes tracer write lock is acquired prior to call. Must return with lock still
acquired. It is assumed that tracee_pid is according to the outermost namespace
*/
void AddToTracerScheduleQueue(tracer * tracer_entry,
                              struct task_struct * tracee) {

    lxc_schedule_elem * new_elem;
    //represents base time allotted to each process by the Kronos scheduler
    //Only useful in single core mode.
    s64 base_time_quanta;
    s64 base_quanta_n_insns = 0;
    s32 rem = 0;
    int tracee_pid;
    struct dilated_task_struct * tracee_dilated_task_struct;
    struct sched_param sp;

    
    BUG_ON(!tracee);
    tracee_pid = tracee->pid;


    if (hmap_get_abs(&get_dilated_task_struct_by_pid, tracee_pid)) {
        PDEBUG_I("Tracee : %d already exists !\n", tracee_pid);
        return;
    }
    
    if (experiment_status == STOPPING) {
        PDEBUG_I("AddToTracerScheduleQueue: cannot add when experiment is "
                 "stopping !\n");
        return;
    }

    PDEBUG_I("AddToTracerScheduleQueue: "
             "Adding new tracee %d to tracer-id: %d\n",
             tracee->pid, tracer_entry->tracer_id);

    new_elem = (lxc_schedule_elem *)kmalloc(sizeof(lxc_schedule_elem),
                GFP_KERNEL);
    
    if (!new_elem) {
        PDEBUG_E("Tracer %d, tracee: %d. Failed to alot Memory to add tracee\n",
                 tracer_entry->tracer_id, tracee->pid);
    }
    //memset(new_elem, 0, sizeof(lxc_schedule_elem));

    tracee_dilated_task_struct = (struct dilated_task_struct *)kmalloc(
        sizeof(struct dilated_task_struct), GFP_KERNEL);
    if (!tracee_dilated_task_struct) {
        PDEBUG_E("Tracer %d, tracee: %d. Failed to alot Memory to add tracee\n",
                 tracer_entry->tracer_id, tracee->pid);
    }


    new_elem->pid = tracee->pid;
    new_elem->curr_task = tracee_dilated_task_struct;
    new_elem->base_quanta = PROCESS_MIN_QUANTA_NS;
    new_elem->quanta_left_from_prev_round = 0;
    new_elem->quanta_curr_round = 0;
    new_elem->blocked = 0;
    new_elem->total_run_quanta = 0;

    tracee_dilated_task_struct->associated_tracer_id = tracer_entry->tracer_id;
    tracee_dilated_task_struct->base_task = tracee;
    tracee_dilated_task_struct->bulk_lookahead_expiry_time = 0;
    tracee_dilated_task_struct->lookahead_anchor_type = 0;
    tracee_dilated_task_struct->pid = tracee->pid;
    tracee_dilated_task_struct->virt_start_time = virt_exp_start_time;
    tracee_dilated_task_struct->curr_virt_time = tracer_entry->curr_virtual_time;
    tracee_dilated_task_struct->wakeup_time = 0;
    tracee_dilated_task_struct->burst_target = 0;
    tracee_dilated_task_struct->buffer_window_len = 0;
    tracee_dilated_task_struct->lookahead = 0;
    tracee_dilated_task_struct->syscall_waiting = 0;
    tracee_dilated_task_struct->resumed_by_dilated_timer = 0;
    tracee_dilated_task_struct->syscall_wait_return = 0;
    init_waitqueue_head(&tracee_dilated_task_struct->d_task_wqueue);
    
    BUG_ON(!chaintask[tracer_entry->timeline_assignment]);
    BUG_ON(tracer_entry->vt_exec_task 
        != chaintask[tracer_entry->timeline_assignment]);
    tracee_dilated_task_struct->vt_exec_task = tracer_entry->vt_exec_task;
    tracee_dilated_task_struct->ready = 0;
    tracee_dilated_task_struct->associated_tracer = tracer_entry;
    
    llist_append(&tracer_entry->schedule_queue, new_elem);
    //bitmap_zero((&tracee->cpus_allowed)->bits, 8);
    //cpumask_set_cpu(tracer_entry->cpu_assignment, &tracee->cpus_allowed);

    BindToCpu(tracee, tracer_entry->cpu_assignment);

    hmap_put_abs(&get_dilated_task_struct_by_pid, tracee->pid,
             tracee_dilated_task_struct);
    PDEBUG_I("AddToTracerScheduleQueue:  Tracee %d added successfully to "
            "tracer-id: %d. schedule list size: %d\n", tracee->pid,
            tracer_entry->tracer_id, ScheduleListSize(tracer_entry));

        sp.sched_priority = 99;
    sched_setscheduler(tracee, SCHED_RR, &sp);

    if (!tracer_entry->main_task)
        tracer_entry->main_task = tracee_dilated_task_struct;
}


void AddToPktInfoQueue(tracer * tracer_entry,
                           int payload_hash,
               int payload_len,
               s64 pkt_send_tstamp) {

    pkt_info * new_pkt_info;
    
    new_pkt_info = (pkt_info *)kmalloc(sizeof(pkt_info), GFP_KERNEL);
    
    if (!new_pkt_info) {
        PDEBUG_E("Tracer %d. Failed to alot Memory to add pkt_info\n",
                 tracer_entry->tracer_id);
    }


    new_pkt_info->payload_hash = payload_hash;
    new_pkt_info->payload_len = payload_len;
    new_pkt_info->pkt_send_tstamp = pkt_send_tstamp;
    llist_append(&tracer_entry->pkt_info_queue, new_pkt_info);

}


void CleanUpPktInfoQueue(tracer * tracer_entry) {

    BUG_ON(!tracer_entry);
    /*if (llist_size(&tracer_entry->pkt_info_queue))
        PDEBUG_I("Cleaning up pkt info queue for tracer: %d, at time: %llu\n",
            tracer_entry->tracer_id, tracer_entry->curr_virtual_time);*/

    while(llist_size(&tracer_entry->pkt_info_queue)) {
        pkt_info * head;
        head = llist_pop(&tracer_entry->pkt_info_queue);
        if (head) kfree(head);
    }
}

int GetNumEnqueuedBytes(tracer * tracer_entry) {
    if (!tracer_entry)
        return 0;
    llist_elem * head = tracer_entry->pkt_info_queue.head;
    int total_enqueued_bytes = 0;
    while (head != NULL) {
        pkt_info * curr_pkt_info = (pkt_info *) head->item;
        if (curr_pkt_info) {
            total_enqueued_bytes += curr_pkt_info->payload_len;
        }
        head = head->next;
    }
    
    return total_enqueued_bytes;
}

s64 GetPktSendTstamp(tracer * tracer_entry, int payload_hash) {
    BUG_ON(!tracer_entry);

    llist_elem * head = tracer_entry->pkt_info_queue.head;
    llist_elem * removed_elem;
    pkt_info * curr_pkt_info;
    int pos = 0;
    s64 pkt_send_tstamp;
    while (head != NULL) {
        curr_pkt_info = (pkt_info *) head->item;
        if (curr_pkt_info && curr_pkt_info->payload_hash == payload_hash) {
            pkt_send_tstamp = curr_pkt_info->pkt_send_tstamp;
            //PDEBUG_I("Found a matching packet for hash: %d, send-timestamp = %llu\n",
            //     payload_hash, pkt_send_tstamp);
            removed_elem = llist_remove_at(&tracer_entry->pkt_info_queue, pos);
            if (removed_elem) kfree(removed_elem);
            return pkt_send_tstamp;
        }
        pos ++;
        head = head->next;
    }
    
    return tracer_entry->curr_virtual_time;
}



/*
Assumes tracer write lock is acquired prior to call. Must return with lock still
acquired. It is assumed that tracee_pid is according to the outermost namespace
*/
void RemoveFromTracerScheduleQueue(tracer * tracer_entry, int tracee_pid) {

    lxc_schedule_elem * curr_elem;
    lxc_schedule_elem * removed_elem;
    struct task_struct * tracee = FindTaskByPid(tracee_pid);

    llist_elem * head_1;
    llist_elem * head_2;
    int pos = 0;

    PDEBUG_I("Removing tracee %d from tracer-id: %d\n",
             tracee_pid, tracer_entry->tracer_id);

    head_1 = tracer_entry->schedule_queue.head;
    head_2 = tracer_entry->run_queue.head;


    while (head_1 != NULL || head_2 != NULL) {

        if (head_1) {
            curr_elem = (lxc_schedule_elem *)head_1->item;
            if (curr_elem && curr_elem->pid == tracee_pid) {
                removed_elem = llist_remove_at(&tracer_entry->schedule_queue,
                    pos);
            }
            head_1 = head_1->next;
        }

        if (head_2) {
            curr_elem = (lxc_schedule_elem *)head_2->item;
            if (curr_elem && curr_elem->pid == tracee_pid) {
                llist_remove_at(&tracer_entry->run_queue, pos);
            }
            head_2 = head_2->next;
        }
        pos ++;
    }

    if (removed_elem) {
        //hmap_remove_abs(&get_dilated_task_struct_by_pid, removed_elem->pid);
        //kfree(removed_elem->curr_task);
        if (removed_elem == tracer_entry->last_run)
            tracer_entry->last_run = NULL;
        removed_elem->curr_task->associated_tracer_id = -1;
        kfree(removed_elem);
    }
    PDEBUG_I("Tracer %d schedule queue size is %d after removing tracee: %d\n",
        tracer_entry->tracer_id, llist_size(&tracer_entry->schedule_queue),
        tracee_pid);
}


/**
* write_buffer: <tracer_id>,<registration_type>,<optional_timeline_id>
**/
int RegisterTracerProcess(char * write_buffer) {

    tracer * new_tracer;
    uint32_t tracer_id;
    int i;
    int best_cpu = 0;
    int registration_type = 0;
    int assigned_timeline_id = 0;
    int assigned_cpu = 0;
    int api_args[MAX_API_ARGUMENT_SIZE];
    int num_args;


    num_args = ConvertStringToArray(write_buffer, api_args,
                      MAX_API_ARGUMENT_SIZE);

    BUG_ON(num_args < 2);

    tracer_id = api_args[0];
    registration_type = api_args[1];


    if (registration_type == EXP_CS) {
        BUG_ON(num_args != 3);
        assigned_timeline_id  = api_args[2];
        if (assigned_timeline_id < 0) {
            PDEBUG_E("assigned_timeline_id pid must be >=0 zero. "
                     "Received value: %d\n", assigned_timeline_id);
            return FAIL;
        }
        assigned_cpu = (assigned_timeline_id % EXP_CPUS);

    } else {
        registration_type = 0;
        for (i = 0; i < EXP_CPUS; i++) {
            if (per_timeline_chain_length[i]
                < per_timeline_chain_length[best_cpu])
                best_cpu = i;
        }
        assigned_timeline_id = best_cpu;
        assigned_cpu = best_cpu;
    }

    if (registration_type == 0)
        PDEBUG_I("Register Tracer for EXP_CBE: Starting for tracer: %d, assigned-cpu: %d\n",
        tracer_id, assigned_timeline_id);
    else
        PDEBUG_I("Register Tracer for EXP_CS: Starting for tracer: %d, assigned-timeline: %d\n",
        tracer_id, assigned_timeline_id);

    new_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);

    if (!new_tracer) {
        new_tracer = AllocTracerEntry(tracer_id);
        hmap_put_abs(&get_tracer_by_id, tracer_id, new_tracer);
        hmap_put_abs(&get_tracer_by_pid, current->pid, new_tracer);
    } else {
        InitializeTracerEntry(new_tracer, tracer_id);
        hmap_put_abs(&get_tracer_by_pid, current->pid, new_tracer);
    }

    if (!new_tracer)
        return FAIL;

    

    mutex_lock(&exp_lock);
    
    if (registration_type == 0)
        per_timeline_chain_length[best_cpu] ++;

    new_tracer->tracer_id = tracer_id;
    new_tracer->timeline_assignment = assigned_timeline_id;
    new_tracer->cpu_assignment = assigned_cpu;
    new_tracer->w_queue = &timeline_wqueue[new_tracer->timeline_assignment];
    new_tracer->tracer_pid = current->pid;
    new_tracer->vt_exec_task = chaintask[new_tracer->timeline_assignment];

    BUG_ON(!chaintask[new_tracer->timeline_assignment]);
    
    ++tracer_num;
    llist_append(&per_timeline_tracer_list[assigned_timeline_id], new_tracer);
    mutex_unlock(&exp_lock);


    GetTracerStructWrite(new_tracer);
    BindToCpu(current, new_tracer->cpu_assignment);
    AddToTracerScheduleQueue(new_tracer, current);
    PutTracerStructWrite(new_tracer);
    PDEBUG_I("Register Tracer: Finished for tracer: %d\n", tracer_id);

    //return the allotted timeline_id back to the tracer.
    return new_tracer->timeline_assignment;	
}



/*
Assumes tracer write lock is acquired prior to call. Must return with write lock
still acquired.
*/
void UpdateAllChildrenVirtualTime(tracer * tracer_entry, s64 time_increment) {
    if (tracer_entry && tracer_entry->main_task) {

        tracer_entry->round_start_virt_time += time_increment;
        tracer_entry->curr_virtual_time = tracer_entry->round_start_virt_time;

        if (ScheduleListSize(tracer_entry) > 0)
            SetChildrenTime(tracer_entry, tracer_entry->curr_virtual_time, 0);
    }
}



/*
Assumes no tracer lock is acquired prior to call
*/
void UpdateAllTracersVirtualTime(int timelineID) {
    llist_elem * head;
    llist * tracer_list;
    tracer * curr_tracer;
    s64 target_increment;

    tracer_list =  &per_timeline_tracer_list[timelineID];
    head = tracer_list->head;


    while (head != NULL) {

        curr_tracer = (tracer*)head->item;
        GetTracerStructWrite(curr_tracer);
        target_increment = curr_tracer->nxt_round_burst_length;

        if (target_increment > 0) {
            UpdateAllChildrenVirtualTime(curr_tracer, target_increment);
        } else {
            PDEBUG_E("Tracer nxt round burst length is 0\n");
        }
        curr_tracer->nxt_round_burst_length = 0;
        PutTracerStructWrite(curr_tracer);
        head = head->next;
    }
}


void wakeUpSyscallWaits(tracer * tracer_entry, s64 syscall_wait_return) {
    if (!tracer_entry)
        return;

    lxc_schedule_elem * curr_elem;
    llist_elem * head;
    int timelineID = tracer_entry->timeline_assignment;
    head = tracer_entry->schedule_queue.head;

    while (head != NULL) {
        if (head) {
            curr_elem = (lxc_schedule_elem *)head->item;
            if (curr_elem && curr_elem->curr_task->syscall_waiting) {
                PDEBUG_V("Waking syscall waiting process: %d\n",
                         curr_elem->curr_task->pid);
                curr_elem->curr_task->syscall_waiting = 0;
                curr_elem->curr_task->syscall_wait_return = syscall_wait_return;
                if (curr_elem == tracer_entry->last_run) {
                    tracer_entry->last_run->quanta_left_from_prev_round = 0;
                    tracer_entry->last_run = NULL;
                }
                BUG_ON(curr_elem->curr_task->ready != 0);
                wake_up_interruptible(&syscall_wait_wqueue[timelineID]);
                wait_event_interruptible(
                    syscall_wait_wqueue[timelineID],
                    (curr_elem->curr_task->syscall_waiting == 1
                     && curr_elem->curr_task->ready == 0) ||
                    (curr_elem->curr_task->ready == 1
                    && curr_elem->curr_task->syscall_waiting == 0));
                curr_elem->curr_task->syscall_wait_return = 0;
                PDEBUG_V("Resuming after waking syscall waiting process: %d\n",
                         curr_elem->curr_task->pid);
            }
            head = head->next;
        }
    }
}

void WakeUpProcessesWaitingOnSyscalls(int timelineID, s64 syscall_wait_return) {
    llist_elem * head;
    llist * tracer_list;
    tracer * curr_tracer;
    s64 target_increment;

    tracer_list =  &per_timeline_tracer_list[timelineID];
    head = tracer_list->head;


    while (head != NULL) {
        curr_tracer = (tracer*)head->item;
        GetTracerStructWrite(curr_tracer);
        wakeUpSyscallWaits(curr_tracer, syscall_wait_return);
        PutTracerStructWrite(curr_tracer);
        head = head->next;
    }

}

void TriggerStackThreadExecutionsOn(int timelineID, s64 curr_tslice_quanta, int exit_status) {
    llist_elem * head;
    llist * tracer_list;
    tracer * curr_tracer;
    s64 target_increment;

    tracer_list =  &per_timeline_tracer_list[timelineID];
    head = tracer_list->head;


    while (head != NULL) {
        curr_tracer = (tracer*)head->item;
        GetTracerStructWrite(curr_tracer);
        TriggerAllStackThreadExecutions(curr_tracer, curr_tslice_quanta, exit_status, -1);
        PutTracerStructWrite(curr_tracer);
        head = head->next;
    }

}



/**
* write_buffer: result specifies process-IDs to remove from tracer schedule
  and run queues. These process-IDs are no longer a part of the VT experiment.
**/

int HandleTracerResults(tracer * curr_tracer, int * api_args, int num_args) {

    struct pid *pid_struct;
    struct task_struct * task;
    struct dilated_task_struct * dilated_task = hmap_get_abs(
        &get_dilated_task_struct_by_pid, current->pid);
    int i, pid_to_remove;

    if (!curr_tracer)
        return FAIL;

    BUG_ON(!curr_tracer->main_task);

    GetTracerStructWrite(curr_tracer);
    for (i = 0; i < num_args; i++) {
        pid_to_remove = api_args[i];
        if (pid_to_remove <= 0)
            break;

        PDEBUG_I("Handle tracer results: Tracer ID: %d, "
                 "Ignoring Process: %d\n", curr_tracer->tracer_id,
                 pid_to_remove);
        struct task_struct * mappedTask = GetTaskNs(pid_to_remove,
            curr_tracer->main_task->base_task);

        WARN_ON(mappedTask && mappedTask->pid != current->pid);
        
        if (mappedTask != NULL) {
            RemoveFromTracerScheduleQueue(curr_tracer, mappedTask->pid);
        }
    }

    PutTracerStructWrite(curr_tracer);

    if (dilated_task && dilated_task->burst_target > 0) {
        dilated_task->burst_target = 0;
        SignalCpuWorkerResume(curr_tracer);

    }
    return SUCCESS;
}

void WaitForTaskCompletion(tracer * curr_tracer,
                  struct dilated_task_struct * relevant_task) {
    if (!curr_tracer || !relevant_task) {
        return;
    }
    
    int ret;
    PDEBUG_V("Waiting for Tracer completion for Tracer ID: %d\n",
             curr_tracer->tracer_id);
    curr_tracer->w_queue_wakeup_pid = relevant_task->pid;
    BUG_ON(relevant_task->burst_target == 0);
    wake_up_interruptible(&relevant_task->d_task_wqueue);
    
    wait_event_interruptible(*curr_tracer->w_queue,
                 curr_tracer->w_queue_wakeup_pid == 1);
    PDEBUG_V("Resuming from Tracer completion for Tracer ID: %d\n",
             curr_tracer->tracer_id);
    
}

void SignalCpuWorkerResume(tracer * curr_tracer) {

    if (!curr_tracer)
        return;

    
    PDEBUG_V("Signalling resume of cpu worker. Tracer ID: %d\n",
            curr_tracer->tracer_id);
    curr_tracer->w_queue_wakeup_pid = 1;
    wake_up_interruptible(curr_tracer->w_queue);
}

int HandleStopExpCmd() {
    experiment_status = STOPPING;
    InitiateExperimentStopOperation();
    return CleanupExperimentComponents();
}

int HandleInitializeExpCmd(int exp_type, int num_timelines,
                  int num_expected_tracers) {

    if (num_expected_tracers) {
        return InitializeExperimentComponents(exp_type, num_timelines,
                            num_expected_tracers);
    }
    return FAIL;
}



s64 GetCurrentVirtualTime(struct task_struct * task) {
    tracer * associated_tracer;
    lxc_schedule_elem * curr_elem;
    llist_elem * head;
    struct dilated_task_struct * dilated_task 
        = hmap_get_abs(&get_dilated_task_struct_by_pid, task->pid);

    ktime_t now = ktime_get_real();

    if (!dilated_task)
        #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
        return now.tv64;
        #else
        return now;
        #endif

    associated_tracer = dilated_task->associated_tracer;

    if (associated_tracer && associated_tracer->curr_virtual_time) {
        return associated_tracer->curr_virtual_time;
    }
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
    return now.tv64;
    #else
    return now;
    #endif

}

/**
* write_buffer: <pid> of process
**/
s64 HandleGettimepid(char * write_buffer) {

    struct pid *pid_struct;
    struct task_struct * task;
    int pid;

    pid = atoi(write_buffer);
    task = FindTaskByPid(pid);
    if (!task)
        return 0;
    return GetCurrentVirtualTime(task);
}

void TriggerAllStackThreadExecutions(tracer * curr_tracer, s64 curr_tslice_quanta, int exit_status, int optional_stack_id) {
    process_tcp_stack * curr_stack;
    llist_elem * head;
    
    head = curr_tracer->process_tcp_stacks.head;
  
    while (head != NULL) {
        curr_stack = (process_tcp_stack *)head->item;
        head = head->next;  

        if (optional_stack_id > 0 && curr_stack->id != optional_stack_id)
            continue; 

        if (!curr_stack->active && exit_status == STACK_THREAD_CONTINUE)
            // consider only active stacks.
            continue;

        if (curr_stack->rx_loop_complete && exit_status == STACK_THREAD_CONTINUE) {
            // No need to trigger stack thread if rx-loop is already complete for this round.
            continue;
        }

        if (curr_stack->stack_thread_waiting) {
            curr_stack->stack_thread_waiting = 0;
            curr_stack->exit_status = exit_status;
            //PDEBUG_I("Timeline Worker: Trigger stack thread exec: Tracer: %d, Stack-Thread: %d Triggering Stack Thread. Exit status = %d !\n",
            //         curr_tracer->tracer_id, curr_stack->id, exit_status);
            curr_stack->stack_return_tslice_quanta = curr_tslice_quanta;
            wake_up_interruptible(&curr_tracer->stack_w_queue);
            
        }

        
        if (curr_stack->exit_status != STACK_THREAD_EXP_EXIT) {
            //PDEBUG_I("Timeline Worker: Trigger stack thread exec: Tracer: %d, Stack-Thread: %d Entering Wait !\n",
            //        curr_tracer->tracer_id, curr_stack->id);
            wait_event_interruptible(curr_tracer->stack_w_queue,
                curr_stack->stack_thread_waiting == 1);
            //PDEBUG_I("Timeline Worker: Trigger stack thread exec: Tracer: %d, Stack-Thread: %d Resuming from Wait !\n",
            //        curr_tracer->tracer_id, curr_stack->id);
        } else {
            // this stack-thread is exiting permanently
            curr_stack->active = 0; 
            curr_stack->rx_loop_complete = 0;
        }
        
        if (optional_stack_id)
            break;
    }
}

void ResetAllStackRxLoopStatus(tracer * curr_tracer) {

    process_tcp_stack * curr_stack;
    llist_elem * head;
    
    head = curr_tracer->process_tcp_stacks.head;
  
    while (head != NULL) {
        curr_stack = (process_tcp_stack *)head->item;
        curr_stack->rx_loop_complete = 0;
        curr_stack->stack_rtx_send_time = 0;
        head = head->next;
    }
}


