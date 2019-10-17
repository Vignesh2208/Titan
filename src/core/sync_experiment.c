#include "module.h"
#include "utils.h"

/** EXTERN VARIABLES **/

extern int tracer_num;
extern int EXP_CPUS;
extern int TOTAL_CPUS;
extern int experiment_status;
extern int initialization_status;
extern s64 expected_time;
extern s64 virt_exp_start_time;
extern s64 current_progress_duration;
extern int total_expected_tracers;

// pointers
extern struct task_struct * loop_task;
extern struct task_struct * round_task;
extern int * per_cpu_chain_length;
extern llist * per_cpu_tracer_list;
extern struct task_struct ** chaintask;
extern s64 * tracer_clock_array;
extern s64 * aligned_tracer_clock_array;


// locks
extern struct mutex exp_lock;

// hashmaps
extern hashmap get_tracer_by_id;
extern hashmap get_tracer_by_pid;

// atomic variables
extern atomic_t n_waiting_tracers;
extern atomic_t n_workers_running;
extern atomic_t progress_n_enabled;
extern atomic_t experiment_stopping;


/** LOCAL DECLARATIONS **/
int per_cpu_worker(void *data);
int round_sync_task(void *data);
int* values;
static wait_queue_head_t sync_worker_wqueue;
static wait_queue_head_t progress_call_proc_wqueue;
wait_queue_head_t progress_sync_proc_wqueue;



/***
* Progress experiment for specified number of rounds
* write_buffer: <number of rounds>
***/
int progress_by(s64 progress_duration) {

	int progress_rounds = 0;
	int ret = 0;


	if (experiment_status == NOTRUNNING)
		return FAIL;


	current_progress_duration = progress_duration;

	atomic_set(&progress_n_enabled, 1);

	if (experiment_status == FROZEN) {
		experiment_status = RUNNING;
		PDEBUG_A("progress_by: Waking up round_sync_task\n");
		while (wake_up_process(round_task) != 1);
		PDEBUG_A("progress_by: Woke up round_sync_task\n");
	}

	PDEBUG_V("progress_by for fixed duration initiated."
	        " Progress duration = %lu\n", current_progress_duration);

	wake_up_interruptible(&progress_sync_proc_wqueue);
	experiment_status = RUNNING;
	do {
		ret = wait_event_interruptible_timeout(
		          progress_call_proc_wqueue,
		          current_progress_duration == -1, HZ);
		if (ret == 0)
			set_current_state(TASK_INTERRUPTIBLE);
		else
			set_current_state(TASK_RUNNING);

	} while (ret == 0);

	return SUCCESS;
}



void free_all_tracers() {
    int i;

    for (i = 1; i <= tracer_num; i++) {
        tracer * curr_tracer = hmap_get_abs(&get_next_value, i);
        if (curr_tracer) {
            hmap_remove_abs(&get_tracer_by_pid, curr_tracer->tracer_pid);
            hmap_remove_abs(&get_tracer_by_id, i);
            free_tracer_entry(curr_tracer);
        }
    }
    hmap_destroy(&get_tracer_by_id);
	hmap_destroy(&get_tracer_by_pid);
}


int initialize_experiment_components(int num_expected_tracers) {

	int i;
	int j;
    int num_required_pages;

	PDEBUG_V("Entering Experiment Initialization\n");
	if (initialization_status == INITIALIZED) {
		PDEBUG_E("Experiment Already initialized !\n");
		return FAIL;
	}

    if (num_expected_tracers <= 0) {
        PDEBUG_E("Num expected tracers must be positive !\n");
		return FAIL;
    }

    if (tracer_num > 0) {
        // Free all tracer structs from previous experiment
        free_all_tracers();
        tracer_num = 0;
    }

    WARN_ON(tracer_clock_array != NULL);
    WARN_ON(aligned_tracer_clock_array != NULL);

    num_required_pages = (PAGE_SIZE * sizeof(s64))/num_expected_tracers;
    num_required_pages ++;

    tracer_clock_array = alloc_mmap_pages(num_required_pages);

    if (!tracer_clock_array) {
        PDEBUG_E("Failed to allot memory for tracer clock array !\n");
        return FAIL;
    }

    aligned_tracer_clock_array = PAGE_ALIGN((unsigned long)tracer_clock_array);
	per_cpu_chain_length =
	    (int *) kmalloc(EXP_CPUS * sizeof(int), GFP_KERNEL);
	per_cpu_tracer_list =
	    (llist *) kmalloc(EXP_CPUS * sizeof(llist), GFP_KERNEL);
	values = (int *)kmalloc(EXP_CPUS * sizeof(int), GFP_KERNEL);
	chaintask = kmalloc(EXP_CPUS * sizeof(struct task_struct*), GFP_KERNEL);

	if (!per_cpu_tracer_list || !per_cpu_chain_length || !values || !chaintask) {
		PDEBUG_E("Error Allocating memory for per cpu structures.\n");
		BUG();
	}

	for (i = 0; i < EXP_CPUS; i++) {
		llist_init(&per_cpu_tracer_list[i]);
		per_cpu_chain_length[i] = 0;
        values[i] = i;
	}


	expected_time = 0;
    current_progress_duration = 0;
    virt_exp_start_time = 0;
    init_task.curr_virt_time = 0;

	mutex_init(&exp_lock);

	hmap_init(&get_tracer_by_id, "int", 0);
	hmap_init(&get_tracer_by_pid, "int", 0);

	init_waitqueue_head(&progress_call_proc_wqueue);
	init_waitqueue_head(&progress_sync_proc_wqueue);
	init_waitqueue_head(&sync_worker_wqueue);


	atomic_set(&progress_n_enabled, 0);
	atomic_set(&experiment_stopping, 0);
	atomic_set(&n_workers_running, 0);
	atomic_set(&n_waiting_tracers, 0);

	PDEBUG_V("Init experiment components: Initialized Variables\n");


	if (!round_task) {
		round_task = kthread_create(&round_sync_task, NULL, "round_sync_task");
		if (!IS_ERR(round_task)) {
			kthread_bind(round_task, 0);
			wake_up_process(round_task);
		} else {
			PDEBUG_E("Error Starting Round Sync Task\n");
			return -EFAULT;
		}

	}

	experiment_status = NOTRUNNING;
	initialization_status = INITIALIZED;


	PDEBUG_V("Init experiment components: Finished\n");
	return SUCCESS;

}

int cleanup_experiment_components() {

	int i = 0;
    int num_alotted_pages;

	if (initialization_status == NOT_INITIALIZED) {
		PDEBUG_E("Experiment Already Cleaned up ...\n");
		return FAIL;
	}

	PDEBUG_I("Cleaning up experiment components ...\n");


    expected_time = 0;
    current_progress_duration = 0;
    virt_exp_start_time = 0;
    init_task.curr_virt_time = 0;

	atomic_set(&progress_n_enabled, 0);
	atomic_set(&experiment_stopping, 0);
	atomic_set(&n_workers_running, 0);
	atomic_set(&n_waiting_tracers, 0);


	for (i = 0; i < EXP_CPUS; i++) {
		llist_destroy(&per_cpu_tracer_list[i]);
	}

	kfree(per_cpu_tracer_list);
	kfree(per_cpu_chain_length);
	kfree(values);
	kfree(chaintask);

    if (tracer_clock_array && tracer_num > 0) {
        num_alotted_pages = (PAGE_SIZE * sizeof(s64))/tracer_num;
        num_alotted_pages ++;
        free_mmap_pages(tracer_clock_array, num_alotted_pages);
    }

    tracer_clock_array = NULL;
    aligned_tracer_clock_array = NULL;

	experiment_status = NOTRUNNING;
	initialization_status = NOT_INITIALIZED;
	return SUCCESS;
}

/***
*	write_buffer: <expected number of registered tracers>
**/
int sync_and_freeze() {

	int i;
	int j;
	u32 flags;
	s64 now;
	struct timeval now_timeval;
	struct sched_param sp;
	tracer * curr_tracer;
	int ret;



	if (initialization_status != INITIALIZED || experiment_status != NOTRUNNING || total_expected_tracers <= 0) {
		return FAIL;
	}


	PDEBUG_A("Sync And Freeze: ** Starting Experiment Synchronization **\n");
	PDEBUG_A("Sync And Freeze: N expected tracers: %d\n", total_expected_tracers);

	wait_event_interruptible(progress_sync_proc_wqueue,
	    atomic_read(&n_waiting_tracers) == total_expected_tracers);


	if (tracer_num <= 0) {
		PDEBUG_E("Sync And Freeze: Nothing added to experiment, dropping out\n");
		return FAIL;
	}


	if (tracer_num != total_expected_tracers) {
		PDEBUG_E("Sync And Freeze: Expected number of tracers: %d not present."
		         " Actual number of registered tracers: %d\n",
		         total_expected_tracers, tracer_num);
		return FAIL;
	}



	if (experiment_status != NOTRUNNING) {
		PDEBUG_A("Sync And Freeze: Trying to Sync Freeze "
		         "when an experiment is already running!\n");
		return FAIL;
	}

	if (!round_task) {
		PDEBUG_A("Sync And Freeze: Round sync task not started error !\n");
		return FAIL;
	}
	PDEBUG_A("Round Sync Task Pid = %d\n", round_task->pid);

	for (i = 0; i < EXP_CPUS; i++) {
		PDEBUG_A("Sync And Freeze: Adding Worker Thread %d\n", i);
		chaintask[i] = kthread_create(&per_cpu_worker, &values[i],
		                              "per_cpu_worker");
		if (!IS_ERR(chaintask[i])) {
			kthread_bind(chaintask[i], i % (TOTAL_CPUS - EXP_CPUS));
			wake_up_process(chaintask[i]);
			PDEBUG_A("Chain Task %d: Pid = %d\n", i, chaintask[i]->pid);
		}
	}

	do_gettimeofday(&now_timeval);
	now = timeval_to_ns(&now_timeval);
	expected_time = now;
	init_task.curr_virt_time = now;
    virt_exp_start_time = now;

	for (i = 1; i <= tracer_num; i++) {
		curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
		get_tracer_struct_write(curr_tracer);
		if (curr_tracer) {
			PDEBUG_A("Sync And Freeze: "
			         "Setting Virt time for Tracer %d and its children\n", i);
			set_children_time(curr_tracer, curr_tracer->tracer_task, now, 0);
			curr_tracer->round_start_virt_time = virt_exp_start_time;
			curr_tracer->curr_virtual_time = virt_exp_start_time;
			if (curr_tracer->spinner_task) {
				curr_tracer->spinner_task->virt_start_time = now;
				curr_tracer->spinner_task->curr_virt_time = now;
				curr_tracer->spinner_task->wakeup_time = 0;
                curr_tracer->spinner_task->burst_target = 0;

			}

			if (curr_tracer->proc_to_control_task != NULL) {
				curr_tracer->proc_to_control_task->virt_start_time = now;
				curr_tracer->proc_to_control_task->curr_virt_time = now;
				curr_tracer->proc_to_control_task->wakeup_time = 0;
                curr_tracer->proc_to_control_task->burst_target = 0;
			}

			curr_tracer->tracer_task->virt_start_time = 0;
			curr_tracer->tracer_task->curr_virt_time = now;
			curr_tracer->tracer_task->wakeup_time = 0;
            curr_tracer->tracer_task->burst_target = 0;

            aligned_tracer_clock_array[i-1] = now;
		}
		put_tracer_struct_write(curr_tracer);
	}

	experiment_status = FROZEN;
	PDEBUG_A("Finished Sync and Freeze\n");


	return SUCCESS;

}


/***
The function called by each synchronization thread (CBE specific).
For every process it is in charge of it will see how long it should run,
then start running the process at the head of the chain.
***/
int per_cpu_worker(void *data) {
	int round = 0;
	int cpuID = *((int *)data);
	tracer * curr_tracer;
	llist_elem * head;
	llist * tracer_list;
	s64 now;
	struct timeval ktv;
	ktime_t ktime;
	int run_cpu;

	set_current_state(TASK_INTERRUPTIBLE);


	PDEBUG_I("#### per_cpu_worker: Started per cpu worker thread for "
	         " Tracers alotted to CPU = %d\n", cpuID + 2);
	tracer_list =  &per_cpu_tracer_list[cpuID];


	/* if it is the very first round, don't try to do any work, just rest */
	if (round == 0) {
		PDEBUG_I("#### per_cpu_worker: For Tracers alotted to CPU = %d. "
		         "Waiting to be woken up !\n", cpuID + 2);
		goto startWork;
	}

	while (!kthread_should_stop()) {

		if (experiment_status == STOPPING) {


			atomic_dec(&n_workers_running);
			run_cpu = get_cpu();
			PDEBUG_V("#### per_cpu_worker: Stopping. Sending wake up from "
			         "worker Thread for lxcs on CPU = %d. "
			         "My Run cpu = %d\n", cpuID + 2, run_cpu);
			wake_up_interruptible(&sync_worker_wqueue);
			return 0;
		}

		head = tracer_list->head;


		while (head != NULL) {

			curr_tracer = (tracer *)head->item;
			get_tracer_struct_read(curr_tracer);
			if (schedule_list_size(curr_tracer) > 0) {
				PDEBUG_V("per_cpu_worker: Called "
				         "UnFreeze Proc Recurse on CPU: %d\n", cpuID +  2);
				unfreeze_proc_exp_recurse(curr_tracer);
				PDEBUG_V("per_cpu_worker: "
				         "Finished Unfreeze Proc on CPU: %d\n", cpuID + 2);

			}
			put_tracer_struct_read(curr_tracer);
			head = head->next;
		}

		PDEBUG_V("per_cpu_worker: Thread done with on %d\n", cpuID + 2);
		/* when the first task has started running, signal you are done working,
		and sleep */
		round++;
		set_current_state(TASK_INTERRUPTIBLE);
		atomic_dec(&n_workers_running);
		run_cpu = get_cpu();
		PDEBUG_V("#### per_cpu_worker: Sending wake up from per_cpu_worker "
		         "on behalf of all Tracers on CPU = %d. My Run cpu = %d\n",
		         cpuID + 2, run_cpu);
		wake_up_interruptible(&sync_worker_wqueue);


startWork:
		schedule();
		set_current_state(TASK_RUNNING);
		run_cpu = get_cpu();
		PDEBUG_V("~~~~ per_cpu_worker: Woken up for Tracers on CPU =  %d. "
		         "My Run cpu = %d\n", cpuID + 2, run_cpu);

	}
	return 0;
}

/***
The main synchronization thread (For CBE mode). When all tasks in a round have
completed, this will get woken up, increment the experiment virtual time,
and then wake up every other synchronization thread
to have it do work
***/
int round_sync_task(void *data) {
	int round_count = 0;
	struct timeval ktv;
	int i;

	struct timeval now;
	s64 start_ns;
	tracer * curr_tracer;
	int run_cpu;


	set_current_state(TASK_INTERRUPTIBLE);
	PDEBUG_I("round_sync_task: Started.\n");

	while (!kthread_should_stop()) {

		if (round_count == 0)
			goto end;
		round_count++;
redo:

		if (experiment_status == STOPPING) {

			PDEBUG_I("round_sync_task: Cleaning experiment via catchup task. "
			         "Waiting for all cpu workers to exit...\n");

			atomic_set(&n_workers_running, EXP_CPUS);
			for (i = 0; i < EXP_CPUS; i++) {
				/* chaintask refers to per_cpu_worker */
				if (wake_up_process(chaintask[i]) == 1) {
					PDEBUG_V("round_sync_task: Sync thread %d wake up\n", i);
				} else {
					while (wake_up_process(chaintask[i]) != 1);
					PDEBUG_V("round_sync_task: "
					         "Sync thread %d already running\n", i);
				}
			}
			wait_event_interruptible(sync_worker_wqueue,
			                         atomic_read(&n_workers_running) == 0);
			PDEBUG_I("round_sync_task: "
			         "All cpu workers and all syscalls exited !\n");
			init_task.freeze_time = KTIME_MAX;
			preempt_disable();
			local_irq_disable();
			dilated_hrtimer_run_queues_flush(0);
			local_irq_enable();
			preempt_enable();

			init_task.freeze_time = 0;
			clean_exp();
			round_count = 0;
			continue;
		}



		if (atomic_read(&experiment_stopping) == 1
		        && atomic_read(&n_active_syscalls) == 0) {
			experiment_status = STOPPING;
			continue;
		} else if (atomic_read(&experiment_stopping) == 1) {
			PDEBUG_I("round_sync_task: Stopping. NActive syscalls = %d\n",
			         atomic_read(&n_active_syscalls));
			atomic_set(&progress_n_rounds, 0);
			atomic_set(&progress_n_enabled, 0);

			for (i = 1; i <= tracer_num; i++) {
				curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
				if (curr_tracer) {

					get_tracer_struct_read(curr_tracer);
					resume_all_syscall_blocked_processes(curr_tracer, 0);
					put_tracer_struct_read(curr_tracer);
				}
			}
			PDEBUG_I("round_sync_task: Waiting for syscalls to exit\n");
			
			experiment_status = STOPPING;
			continue;
		}




		run_cpu = get_cpu();
		PDEBUG_V("round_sync_task: Waiting for progress sync proc queue "
		         "to resume. Run_cpu %d. N_waiting tracers = %d\n", run_cpu,
		         atomic_read(&n_waiting_tracers));
		wait_event_interruptible(
		    progress_sync_proc_wqueue,
		    ((atomic_read(&progress_n_enabled) == 1
		      && atomic_read(&progress_n_rounds) > 0)
		     || atomic_read(&progress_n_enabled) == 0)
		    && atomic_read(&n_waiting_tracers) == tracer_num);

		if (atomic_read(&experiment_stopping) == 1) {
			continue;
		}
		do_gettimeofday(&ktv);

		/* wait up each synchronization worker thread,
		then wait til they are all done */
		if (EXP_CPUS > 0 && tracer_num  > 0) {

			PDEBUG_I("$$$$$$$$$$$$$$$$$$$$$$$ round_sync_task: "
			         "Round %d Starting. Waking up worker threads "
			         "$$$$$$$$$$$$$$$$$$$$$$$$$$\n", round_count);
			atomic_set(&n_workers_running, EXP_CPUS);


			for (i = 0; i < EXP_CPUS; i++) {

				/* chaintask refers to per_cpu_worker */
				if (wake_up_process(chaintask[i]) == 1) {
					PDEBUG_V("round_sync_task: Sync thread %d wake up\n", i);
				} else {
					while (wake_up_process(chaintask[i]) != 1) {
						msleep(50);
					}
					PDEBUG_V("round_sync_task: "
					         "Sync thread %d already running\n", i);
				}
			}

			run_cpu = get_cpu();
			PDEBUG_I("round_sync_task: "
			         "Waiting for per_cpu_workers to finish. Run_cpu %d\n",
			         run_cpu);

			do_gettimeofday(&now);
			start_ns = timeval_to_ns(&now);
			wait_event_interruptible(sync_worker_wqueue,
			                         atomic_read(&n_workers_running) == 0);


			PDEBUG_I("round_sync_task: All sync drift thread finished\n");
			for (i = 0 ; i < EXP_CPUS; i++) {
				update_all_tracers_virtual_time(i);
			}

			if (!app_driven_hrtimer_firing) {
				PDEBUG_V("App driven firing not configured: "
				         "Calling dilated hrtimer run queues\n");
				preempt_disable();
				local_irq_disable();
				dilated_hrtimer_run_queues(0);
				local_irq_enable();
				preempt_enable();
				PDEBUG_V("App driven firing not configured: "
				         "Finished dilated hrtimer run queues\n");
			}

		}

		if (atomic_read(&progress_n_enabled) == 1
		        && atomic_read(&progress_n_rounds) > 0) {
			atomic_dec(&progress_n_rounds);
			if (atomic_read(&progress_n_rounds) == 0) {
				PDEBUG_V("Waking up Progress rounds wait Process\n");
				wake_up_interruptible(&progress_call_proc_wqueue);
			}
		}

end:
		set_current_state(TASK_INTERRUPTIBLE);

		if (experiment_status == NOTRUNNING) {
			set_current_state(TASK_INTERRUPTIBLE);
			round_count++;
			PDEBUG_I("round_sync_task: Waiting to be woken up\n");
			schedule();
		}
		set_current_state(TASK_INTERRUPTIBLE);
		PDEBUG_V("round_sync_task: Resumed\n");
	}
	return 0;
}



/***
Searches an tracer for the process with given pid. returns success if found
***/

struct task_struct * search_tracer(struct task_struct * aTask, int pid) {


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
		t =  search_tracer(taskRecurse, pid);
		if (t != NULL)
			return t;
	}

	return NULL;
}

/*
Assumes curr_tracer read lock is acquired prior to function call. Must return
with read lock still acquired.
*/
void clean_up_all_irrelevant_processes(tracer * curr_tracer) {

	struct pid *pid_struct;
	struct task_struct *task;
	llist *schedule_queue ;
	llist_elem *head ;
	lxc_schedule_elem *curr_elem;
	int n_checked_processes = 0;
	int n_scheduled_processes = 0;


	if (!curr_tracer)
		return;

	curr_elem = NULL;

	PDEBUG_V("Clean up irrelevant processes: Entered.\n");
	n_scheduled_processes = schedule_list_size(curr_tracer);
	PDEBUG_V("Clean up irrelevant processes: "
	         "Entered. n_scheduled_processes: %d\n", n_scheduled_processes);

	while (n_checked_processes < n_scheduled_processes) {
		curr_elem = (lxc_schedule_elem *)llist_get(
		                &curr_tracer->schedule_queue, 0);
		PDEBUG_V("Clean up irrelevant processes: Got head.\n");
		if (!curr_elem)
			return;

		if (curr_elem->pid == curr_tracer->proc_to_control_pid) {
			if (find_task_by_pid(curr_elem->pid) != NULL) {
				n_checked_processes ++;
				continue;
			} else {
				PDEBUG_V("Clean up irrelevant processes: "
				         "Curr elem: %d. Task is dead\n", curr_elem->pid);
				put_tracer_struct_read(curr_tracer);
				get_tracer_struct_write(curr_tracer);
				pop_schedule_list(curr_tracer);
				put_tracer_struct_write(curr_tracer);
				get_tracer_struct_read(curr_tracer);
				n_checked_processes ++;
				curr_tracer->proc_to_control_pid = -1;
				curr_tracer->proc_to_control_task = NULL;
				continue;
			}
		}

		PDEBUG_V("Clean up irrelevant processes: "
		         "Curr elem: %d. n_scheduled_processes: %d\n",
		         curr_elem->pid, n_scheduled_processes);
		task = search_tracer(curr_tracer->tracer_task, curr_elem->pid);
		if ( task == NULL || hmap_get_abs(&curr_tracer->ignored_children,
		                                  curr_elem->pid) != NULL) {

			if (task == NULL) { // task is dead
				PDEBUG_I("Clean up irrelevant processes: "
				         "Curr elem: %d. Task is dead\n", curr_elem->pid);
				put_tracer_struct_read(curr_tracer);
				get_tracer_struct_write(curr_tracer);
				pop_schedule_list(curr_tracer);
				put_tracer_struct_write(curr_tracer);
				get_tracer_struct_read(curr_tracer);
			} else { // task is ignored
				PDEBUG_I("Clean up irrelevant processes: "
				         "Curr elem: %d. Task is ignored\n", curr_elem->pid);

				put_tracer_struct_read(curr_tracer);
				get_tracer_struct_write(curr_tracer);
				pop_schedule_list(curr_tracer);
				put_tracer_struct_write(curr_tracer);
				get_tracer_struct_read(curr_tracer);
			}


		} else {
			put_tracer_struct_read(curr_tracer);
			get_tracer_struct_write(curr_tracer);
			requeue_schedule_list(curr_tracer);
			put_tracer_struct_write(curr_tracer);
			get_tracer_struct_read(curr_tracer);
		}
		n_checked_processes ++;
	}
}

/*
Assumes curr_tracer read lock is acquired prior to function call. Must return
with read lock still acquired.
*/
void update_all_runnable_task_timeslices(tracer * curr_tracer) {
	llist* schedule_queue = &curr_tracer->schedule_queue;
	llist_elem* head = schedule_queue->head;
	lxc_schedule_elem* curr_elem;
	lxc_schedule_elem* tmp;
	unsigned long flags;
	s64 total_insns = curr_tracer->quantum_n_insns;
	s64 n_alotted_insns = 0;
	int no_task_runnable = 1;

#ifdef __TK_MULTI_CORE_MODE
	while (head != NULL) {
#else
	while (head != NULL && n_alotted_insns < total_insns) {
#endif
		curr_elem = (lxc_schedule_elem *)head->item;

		if (!curr_elem) {
			PDEBUG_V("Update all runnable task timeslices: "
			         "Curr elem is NULL\n");
			return;
		}

		PDEBUG_V("Update all runnable task timeslices: "
		         "Processing Curr elem Left\n");
		PDEBUG_V("Update all runnable task timeslices: "
		         "Curr elem is %d. Quantum n_insns left: %llu\n",
		         curr_elem->pid, curr_elem->n_insns_left);

#ifdef IGNORE_BLOCKED_PROCESS_SCHED_MODE
		//struct task_struct * tsk = find_task_by_pid(curr_elem->pid);
		curr_elem->blocked = 1;
		if (curr_elem->curr_task && (!test_bit(
		                                 PTRACE_BREAK_WAITPID_FLAG,
		                                 &curr_elem->curr_task->ptrace_mflags)
		                             || curr_elem->curr_task->on_rq == 1)) {
			curr_elem->blocked = 0;
			no_task_runnable = 0;
		}
#else
		no_task_runnable = 0;
#endif
		put_tracer_struct_read(curr_tracer);
		get_tracer_struct_write(curr_tracer);
#ifdef __TK_MULTI_CORE_MODE
		curr_elem->n_insns_curr_round = total_insns;
		//n_alotted_insns = total_insns;
#else
		if (curr_elem->n_insns_left > 0) {
			// there should exist only one element like this

#ifdef IGNORE_BLOCKED_PROCESS_SCHED_MODE
			if (!curr_elem->blocked) {
#endif
				if (n_alotted_insns + curr_elem->n_insns_left > total_insns) {
					curr_elem->n_insns_curr_round =
					    total_insns - n_alotted_insns;
					curr_elem->n_insns_left =
					    curr_elem->n_insns_left - curr_elem->n_insns_curr_round;
					n_alotted_insns = total_insns;
				} else {
					curr_elem->n_insns_curr_round = curr_elem->n_insns_left;
					curr_elem->n_insns_left = 0;
					n_alotted_insns += curr_elem->n_insns_curr_round;
				}
				curr_tracer->last_run = curr_elem;
#ifdef IGNORE_BLOCKED_PROCESS_SCHED_MODE
			} else {
				curr_elem->n_insns_curr_round = 0;
				curr_elem->n_insns_left = 0;
				curr_tracer->last_run = curr_elem;
			}
		} else {
			curr_elem->n_insns_curr_round = 0;
			curr_elem->n_insns_left = 0;
		}
#else
			}
#endif

#endif
		put_tracer_struct_write(curr_tracer);
		get_tracer_struct_read(curr_tracer);
		head = head->next;
	}



#ifndef __TK_MULTI_CORE_MODE
	if (n_alotted_insns < total_insns && no_task_runnable == 0) {
		if (curr_tracer->last_run == NULL)
			head = schedule_queue->head;
		else {
			head = schedule_queue->head;
			while (head != NULL) {
				tmp = (lxc_schedule_elem *)head->item;
				if (tmp == curr_tracer->last_run) {
					head = head->next;
					break;
				}
				head = head->next;
			}

			if (head == NULL) {
				//last run task no longer exists in schedule queue
				//reset to head of schedule queue for now.
				head = schedule_queue->head;
			}
		}

		while (n_alotted_insns < total_insns) {


			curr_elem = (lxc_schedule_elem *)head->item;
			if (!curr_elem)
				return;
#ifdef IGNORE_BLOCKED_PROCESS_SCHED_MODE
			//struct task_struct * tsk = find_task_by_pid(curr_elem->pid);
			if (!curr_elem->blocked) {
#endif
				PDEBUG_V("Update all runnable task timeslices: "
				         "Processing Curr elem Share\n");
				PDEBUG_V("Update all runnable task timeslices: "
				         "Curr elem is %d. Quantum n_insns current round: %llu\n",
				         curr_elem->pid, curr_elem->n_insns_curr_round);
				put_tracer_struct_read(curr_tracer);
				get_tracer_struct_write(curr_tracer);
				if (n_alotted_insns + curr_elem->n_insns_share > total_insns) {
					curr_elem->n_insns_curr_round += total_insns - n_alotted_insns;
					//for next round
					curr_elem->n_insns_left =
					    curr_elem->n_insns_share - (total_insns - n_alotted_insns);
					n_alotted_insns = total_insns;
				} else {
					curr_elem->n_insns_curr_round =
					    curr_elem->n_insns_curr_round + curr_elem->n_insns_share;
					curr_elem->n_insns_left = 0;
					n_alotted_insns += curr_elem->n_insns_share;
				}

				if (n_alotted_insns == total_insns) {
					curr_tracer->last_run = curr_elem;
					put_tracer_struct_write(curr_tracer);
					get_tracer_struct_read(curr_tracer);
					return;
				}
				put_tracer_struct_write(curr_tracer);
				get_tracer_struct_read(curr_tracer);
#ifdef IGNORE_BLOCKED_PROCESS_SCHED_MODE
			}
#endif

			head = head->next;

			if (head == NULL) {
				head = schedule_queue->head;
			}
		}
	}
#endif
}


/*
Assumes tracer read lock is acquired prior to call. Must return with read lock
acquired.
*/
lxc_schedule_elem * get_next_runnable_task(tracer * curr_tracer) {
	lxc_schedule_elem * curr_elem = NULL;
	int n_checked_processes = 0;
	int n_scheduled_processes = 0;

	if (!curr_tracer)
		return NULL;

	llist* schedule_queue = &curr_tracer->schedule_queue;
	llist_elem* head = schedule_queue->head;


	head = schedule_queue->head;
	while (head != NULL) {
		curr_elem = (lxc_schedule_elem *)head->item;
		if (!curr_elem)
			return NULL;

		if (curr_elem->n_insns_curr_round)
			return curr_elem;
		head = head->next;
	}

	return NULL; // all processes are blocked. nothing to run.

}

int unfreeze_proc_exp_single_core_mode(tracer * curr_tracer) {

	struct timeval now;
	s64 now_ns;
	s64 start_ns;
	int i = 0;
	unsigned long flags;
	struct poll_helper_struct * task_poll_helper = NULL;
	struct select_helper_struct * task_select_helper = NULL;
	struct sleep_helper_struct * task_sleep_helper = NULL;
	s64 rem_n_insns = 0;
	s64 total_insns = 0;
	lxc_schedule_elem * curr_elem;

	if (!curr_tracer)
		return FAIL;

	if (curr_tracer->quantum_n_insns == 0)
		return SUCCESS;

	

	/* for adding any new tasks that might have been spawned */
	put_tracer_struct_read(curr_tracer);
	get_tracer_struct_write(curr_tracer);
	refresh_tracer_schedule_queue(curr_tracer);
	put_tracer_struct_write(curr_tracer);
	get_tracer_struct_read(curr_tracer);
	clean_up_all_irrelevant_processes(curr_tracer);
	resume_all_syscall_blocked_processes(curr_tracer, 0);
	update_all_runnable_task_timeslices(curr_tracer);
	flush_buffer(curr_tracer->run_q_buffer, BUF_MAX_SIZE);
	print_schedule_list(curr_tracer);
	curr_tracer->buf_tail_ptr = 0;

	total_insns = curr_tracer->quantum_n_insns;

	while (rem_n_insns < total_insns) {
		curr_elem = get_next_runnable_task(curr_tracer);
		if (!curr_elem)
			break;

		put_tracer_struct_read(curr_tracer);
		get_tracer_struct_write(curr_tracer);
		add_task_to_tracer_run_queue(curr_tracer, curr_elem);
		rem_n_insns += curr_elem->n_insns_curr_round;
		curr_elem->n_insns_curr_round = 0; // reset to zero
		put_tracer_struct_write(curr_tracer);
		get_tracer_struct_read(curr_tracer);


	}

	if (rem_n_insns >= total_insns) {
		put_tracer_struct_read(curr_tracer);
		signal_tracer_resume(curr_tracer);
		wait_for_tracer_completion(curr_tracer);
		get_tracer_struct_read(curr_tracer);
	} 
	return SUCCESS;
}

/*
Assumes that curr_tracer read lock is acquired before entry. Must return with
read lock still acquired.
*/
int unfreeze_proc_exp_recurse(tracer * curr_tracer) {

#ifdef __TK_MULTI_CORE_MODE
	return unfreeze_proc_exp_multi_core_mode(curr_tracer);
#else
	return unfreeze_proc_exp_single_core_mode(curr_tracer);
#endif
}

/*
Assumes no tracer lock is acquired prior to call.
*/
void clean_exp() {

	int i;
	tracer * curr_tracer;


	wait_event_interruptible(progress_sync_proc_wqueue,
	                         atomic_read(&n_waiting_tracers) == tracer_num);

	PDEBUG_I("Clean exp: Cleaning up initiated ...");
	if (sys_call_table) {
		preempt_disable();
		local_irq_disable();
		orig_cr0 = read_cr0();
		write_cr0(orig_cr0 & ~0x00010000);

		sys_call_table[NR_select] = (unsigned long *)ref_sys_select;
		sys_call_table[__NR_poll] = (unsigned long *) ref_sys_poll;
		sys_call_table[__NR_nanosleep] = (unsigned long *)ref_sys_sleep;
		sys_call_table[__NR_clock_gettime] =
		    (unsigned long *) ref_sys_clock_gettime;
		sys_call_table[__NR_clock_nanosleep] =
		    (unsigned long *) ref_sys_clock_nanosleep;
		write_cr0(orig_cr0 | 0x00010000 );
		local_irq_enable();
		preempt_enable();

	}
	PDEBUG_I("Clean exp: Syscall unhooked ...");
	atomic_set(&experiment_stopping, 0);
	experiment_status = NOTRUNNING;
	mutex_lock(&exp_lock);
	for (i = 1; i <= tracer_num; i++) {

		curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
		if (curr_tracer) {
			get_tracer_struct_write(curr_tracer);
			clean_up_schedule_list(curr_tracer);
			flush_buffer(curr_tracer->run_q_buffer, BUF_MAX_SIZE);
			curr_tracer->buf_tail_ptr = 4;
			sprintf(curr_tracer->run_q_buffer, "STOP");
			atomic_set(&curr_tracer->w_queue_control, 0);
			put_tracer_struct_write(curr_tracer);

			wake_up_interruptible(&curr_tracer->w_queue);
		}
	}
	mutex_unlock(&exp_lock);

}

