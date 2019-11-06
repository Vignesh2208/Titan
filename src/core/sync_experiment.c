#include "vt_module.h"
#include "utils.h"
#include "sync_experiment.h"

/** EXTERN VARIABLES **/

extern int tracer_num;
extern int EXP_CPUS;
extern int TOTAL_CPUS;
extern int experiment_status;
extern int initialization_status;
extern s64 expected_time;
extern s64 virt_exp_start_time;
extern s64 current_progress_duration;
extern int current_progress_n_rounds;
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
int progress_by(s64 progress_duration, int num_rounds) {

	int progress_rounds = 0;
	int ret = 0;


	if (experiment_status == NOTRUNNING)
		return FAIL;


	current_progress_n_rounds = 1;
	if (num_rounds)
		current_progress_n_rounds = num_rounds;
	current_progress_duration = progress_duration;

	atomic_set(&progress_n_enabled, 1);

	if (experiment_status == FROZEN) {
		experiment_status = RUNNING;
		PDEBUG_A("progress_by: Waking up round_sync_task\n");
		while (wake_up_process(round_task) != 1);
		PDEBUG_A("progress_by: Woke up round_sync_task\n");
	}

	PDEBUG_V("progress_by for fixed duration initiated."
	        " Progress duration = %lu, Num rounds = %d\n", current_progress_duration, current_progress_n_rounds);

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
	current_progress_n_rounds = 0;

	return SUCCESS;
}



void free_all_tracers() {
    int i;

    for (i = 1; i <= tracer_num; i++) {
        tracer * curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
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
	int num_prev_alotted_pages;

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
		if (tracer_clock_array) {
			num_prev_alotted_pages = (tracer_num * sizeof(s64))/PAGE_SIZE;
			num_prev_alotted_pages ++;
			free_mmap_pages(tracer_clock_array, num_prev_alotted_pages);
	    }
    	tracer_clock_array = NULL;
    	aligned_tracer_clock_array = NULL;
        tracer_num = 0;
    }

    WARN_ON(tracer_clock_array != NULL);
    WARN_ON(aligned_tracer_clock_array != NULL);

    num_required_pages = (num_expected_tracers * sizeof(s64))/PAGE_SIZE;
    num_required_pages ++;
    PDEBUG_I("Num required pages = %d\n", num_required_pages); 
    
    
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
        total_expected_tracers = num_expected_tracers;

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

	initialization_status = NOT_INITIALIZED;
	experiment_status = NOTRUNNING;
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

	set_current_state(TASK_INTERRUPTIBLE);

	PDEBUG_I("#### per_cpu_worker: Started per cpu worker thread for "
	         " Tracers alotted to CPU = %d\n", cpuID + 2);
	tracer_list =  &per_cpu_tracer_list[cpuID];

	/* if it is the very first round, don't try to do any work, just rest */
	if (round == 0) {
		PDEBUG_I("#### per_cpu_worker: For Tracers alotted to CPU = %d. "
		         "Waiting to be woken up !\n", cpuID);
		goto startWork;
	}

	while (!kthread_should_stop()) {

		if (experiment_status == STOPPING) {


			atomic_dec(&n_workers_running);
			PDEBUG_I("#### per_cpu_worker: Stopping. Sending wake up from "
			         "worker process for tracers on CPU = %d.\n", cpuID);
			wake_up_interruptible(&sync_worker_wqueue);
			return 0;
		}

		head = tracer_list->head;

		BUG_ON(current_progress_duration <= 0);
		while (head != NULL) {

			curr_tracer = (tracer *)head->item;
			get_tracer_struct_read(curr_tracer);
			if (current_progress_duration + expected_time > curr_tracer->curr_virtual_time) {
				curr_tracer->nxt_round_burst_length = (current_progress_duration + expected_time - curr_tracer->curr_virtual_time);
			} else {
				curr_tracer->nxt_round_burst_length = 0;
			}

			if (curr_tracer->tracer_type == TRACER_TYPE_INS_VT) {
				WARN_ON(curr_tracer->nxt_round_burst_length != current_progress_duration);
			} else {
				WARN_ON(curr_tracer->nxt_round_burst_length > current_progress_duration);
			}
			
			if (schedule_list_size(curr_tracer) > 0 && curr_tracer->nxt_round_burst_length) {
				PDEBUG_V("per_cpu_worker: Called "
				         "UnFreeze Proc Recurse for tracer: %d on CPU: %d\n", curr_tracer->tracer_id, cpuID);
				unfreeze_proc_exp_recurse(curr_tracer);
				PDEBUG_V("per_cpu_worker: "
				         "Finished Unfreeze Proc Recurse for tracer: %d on CPU: %d\n", curr_tracer->tracer_id, cpuID);

			}
			put_tracer_struct_read(curr_tracer);
			head = head->next;
		}

		update_all_tracers_virtual_time(cpuID);
		PDEBUG_V("per_cpu_worker: Finished round on %d\n", cpuID);
		/* when the first task has started running, signal you are done working,
		and sleep */
		round++;
		set_current_state(TASK_INTERRUPTIBLE);
		atomic_dec(&n_workers_running);
		PDEBUG_V("#### per_cpu_worker: Sending wake up from per_cpu_worker "
		         "on behalf of all Tracers on CPU = %d\n",
		         cpuID);
		wake_up_interruptible(&sync_worker_wqueue);


startWork:
		schedule();
		set_current_state(TASK_RUNNING);
		PDEBUG_V("#### per_cpu_worker: Woken up for Tracers on CPU =  %d\n", cpuID);

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
			BUG_ON(atomic_read(&n_workers_running) != 0);
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
			         "All cpu workers exited !\n");
			init_task.curr_virt_time = KTIME_MAX;
			preempt_disable();
			local_irq_disable();
			dilated_hrtimer_run_queues_flush(0);
			local_irq_enable();
			preempt_enable();

			init_task.curr_virt_time = 0;
			clean_exp();
			round_count = 0;
			current_progress_duration = -1;
			PDEBUG_V("Waking up Progress control process to finish EXP STOP !\n");
			wake_up_interruptible(&progress_call_proc_wqueue);
			continue;
		}

		PDEBUG_V("round_sync_task: Waiting for progress sync proc queue to resume. \n");
		BUG_ON(atomic_read(&n_workers_running) != 0);
		wait_event_interruptible(
		    progress_sync_proc_wqueue, (atomic_read(&progress_n_enabled) == 1 && current_progress_duration >= 0));

		
		if (current_progress_n_rounds == 0) {
			atomic_set(&progress_n_enabled, 0);
		}

		if (current_progress_duration == 0) {
			current_progress_n_rounds = 0;
			atomic_set(&progress_n_enabled, 0);
			experiment_status = STOPPING;
			continue;
		}

		/* wait up each synchronization worker thread,
		then wait til they are all done */
		if (EXP_CPUS > 0 && tracer_num  > 0) {

			PDEBUG_I("$$$$$$$$$$$$$$$$$$$$$$$ round_sync_task: "
			         "Round %d Starting. Waking up worker threads "
			         "$$$$$$$$$$$$$$$$$$$$$$$$$$\n", round_count - 1);
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

			
			PDEBUG_V("round_sync_task: Waiting for per_cpu_workers to finish.\n");
			wait_event_interruptible(sync_worker_wqueue,
			                         atomic_read(&n_workers_running) == 0);


			PDEBUG_I("round_sync_task: All per_cpu_workers finished\n");
		}

		expected_time += current_progress_duration;
		update_init_task_virtual_time(expected_time);
		preempt_disable();
		local_irq_disable();
		dilated_hrtimer_run_queues(0);
		local_irq_enable();
		preempt_enable();

		if (current_progress_n_rounds == 0) {
			current_progress_duration = -1;
			PDEBUG_V("Waking up Progress control process\n");
			wake_up_interruptible(&progress_call_proc_wqueue);

		} else {
			current_progress_n_rounds --;
			round_count++;
			continue;
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


void prune_tracer_queue(tracer * curr_tracer, int is_schedule_queue){
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

		if (curr_elem->pid == curr_tracer->proc_to_control_pid) {
			if (find_task_by_pid(curr_elem->pid) != NULL) {
				n_checked_processes ++;
				continue;
			} else {
				PDEBUG_V("Clean up irrelevant processes: "
				         "Curr elem: %d. Task is dead\n", curr_elem->pid);
				put_tracer_struct_read(curr_tracer);
				get_tracer_struct_write(curr_tracer);

				if (!is_schedule_queue)
					pop_run_queue(curr_tracer);
				else
					pop_schedule_list(curr_tracer);
				put_tracer_struct_write(curr_tracer);
				get_tracer_struct_read(curr_tracer);
				n_checked_processes ++;
				curr_tracer->proc_to_control_pid = -1;
				curr_tracer->proc_to_control_task = NULL;
				continue;
			}
		}
		task = search_tracer(curr_tracer->tracer_task, curr_elem->pid);
		if (!task){
			PDEBUG_I("Clean up irrelevant processes: "
				     "Curr elem: %d. Task is dead\n", curr_elem->pid);
			put_tracer_struct_read(curr_tracer);
			get_tracer_struct_write(curr_tracer);
			if (!is_schedule_queue)
				pop_run_queue(curr_tracer);
			else
				pop_schedule_list(curr_tracer);
			put_tracer_struct_write(curr_tracer);
			get_tracer_struct_read(curr_tracer);
		} else {
			put_tracer_struct_read(curr_tracer);
			get_tracer_struct_write(curr_tracer);
			if (!is_schedule_queue) {

				if (curr_tracer->tracer_type == TRACER_TYPE_APP_VT && task->ready == 0) {
					// This task was blocked in previous run. Remove from run queue
					curr_elem->blocked = 1;
					WARN_ON(task->burst_target != 0);
					task->burst_target = 0;
					curr_elem->quanta_curr_round = 0;
					curr_elem->quanta_left_from_prev_round = 0;
					pop_run_queue(curr_tracer);
					
				} else {
					WARN_ON(curr_elem->blocked != 0);
					curr_elem->blocked = 0;
					requeue_run_queue(curr_tracer);
				}
			} else {
				requeue_schedule_list(curr_tracer);
			}
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
void clean_up_all_irrelevant_processes(tracer * curr_tracer) {
	if (curr_tracer) {

		if (curr_tracer->tracer_type == TRACER_TYPE_APP_VT)
			prune_tracer_queue(curr_tracer, 0);

		prune_tracer_queue(curr_tracer, 1);
	}
	
}



/*
Assumes curr_tracer read lock is acquired prior to function call. Must return
with read lock still acquired.
*/
int update_all_runnable_task_timeslices(tracer * curr_tracer) {
	llist* schedule_queue = &curr_tracer->schedule_queue;
	llist * run_queue = &curr_tracer->run_queue;
	llist_elem* head = schedule_queue->head;
	lxc_schedule_elem* curr_elem;
	lxc_schedule_elem* tmp;
	unsigned long flags;
	s64 total_quanta = curr_tracer->nxt_round_burst_length;
	s64 alotted_quanta = 0;
	int alteast_one_task_runnable = 0;

	put_tracer_struct_read(curr_tracer);
	get_tracer_struct_write(curr_tracer);


	while (head != NULL) {
		curr_elem = (lxc_schedule_elem *)head->item;
		if (!curr_elem) {
			PDEBUG_V("Update all runnable task timeslices: "
			         "Curr elem is NULL\n");
			return 0;
		}
		PDEBUG_V("Update all runnable task timeslices: "
		         "Processing Curr elem Left\n");
		PDEBUG_V("Update all runnable task timeslices: "
		         "Curr elem is %d. \n", curr_elem->pid);

		curr_elem->blocked = 1;

		if (curr_tracer->tracer_type == TRACER_TYPE_INS_VT) {
			if (curr_elem->curr_task && (!test_bit(
										PTRACE_BREAK_WAITPID_FLAG,
										&curr_elem->curr_task->ptrace_mflags)
										|| curr_elem->curr_task->on_rq == 1)) {
				curr_elem->blocked = 0;
				alteast_one_task_runnable = 1;
			}
		} else {
			if (curr_elem->curr_task && curr_elem->curr_task->ready == 0) {
				curr_elem->blocked = 1;
			} else if (curr_elem->curr_task && curr_elem->curr_task->ready) {
				if (curr_elem->blocked == 1) { // If it was previously marked as blocked, add it to run_queue
					curr_elem->quanta_left_from_prev_round = 0;
					curr_elem->quanta_curr_round = 0;
					WARN_ON(curr_elem->curr_task->burst_target != 0);
					curr_elem->curr_task->burst_target = 0;
					curr_elem->blocked = 0;
					llist_append(&curr_tracer->run_queue, curr_elem);
				} 
				alteast_one_task_runnable = 1;
			}
		}
		head = head->next;
	}

	if (curr_tracer->tracer_type == TRACER_TYPE_INS_VT) {
		put_tracer_struct_write(curr_tracer);
		get_tracer_struct_read(curr_tracer);
		return alteast_one_task_runnable;
	}


	head = run_queue->head;

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

			if (alotted_quanta + curr_elem->quanta_left_from_prev_round > total_quanta) {
				curr_elem->quanta_curr_round = total_quanta - alotted_quanta;
				curr_elem->quanta_left_from_prev_round =
					curr_elem->quanta_left_from_prev_round - curr_elem->quanta_curr_round;
				alotted_quanta = total_quanta;
			} else {
				curr_elem->quanta_curr_round = curr_elem->quanta_left_from_prev_round;
				curr_elem->quanta_left_from_prev_round = 0;
				alotted_quanta += curr_elem->quanta_curr_round;
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

			if (head == NULL) {
				//last run task no longer exists in schedule queue
				//reset to head of schedule queue for now.
				head = schedule_queue->head;
			}
		}

		while (alotted_quanta < total_quanta) {


			curr_elem = (lxc_schedule_elem *)head->item;
			if (!curr_elem) {
				put_tracer_struct_write(curr_tracer);
				get_tracer_struct_read(curr_tracer);
				return alteast_one_task_runnable;
			}

			WARN_ON(curr_elem->blocked || curr_elem->curr_task->ready == 0);

			if (curr_elem->blocked || curr_elem->curr_task->ready == 0) {
				curr_elem->quanta_curr_round = 0;
				curr_elem->quanta_left_from_prev_round = 0;
				head = head->next;
				continue;
			}

			
			if (alotted_quanta + curr_elem->base_quanta > total_quanta) {
				curr_elem->quanta_curr_round = total_quanta - alotted_quanta;
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
				put_tracer_struct_write(curr_tracer);
				get_tracer_struct_read(curr_tracer);
				return alteast_one_task_runnable;
			}
				
			head = head->next;
			if (head == NULL) {
				head = run_queue->head;
			}
		}
	}

	put_tracer_struct_write(curr_tracer);
	get_tracer_struct_read(curr_tracer);

	return alteast_one_task_runnable;
}


/*
Assumes tracer read lock is acquired prior to call. Must return with read lock
acquired.
*/
lxc_schedule_elem * get_next_runnable_task(tracer * curr_tracer) {
	lxc_schedule_elem * curr_elem = NULL;
	int n_checked_processes = 0;
	int n_scheduled_processes = 0;

	if (!curr_tracer || curr_tracer->tracer_type == TRACER_TYPE_INS_VT)
		return NULL;

	llist* run_queue = &curr_tracer->run_queue;
	llist_elem* head = run_queue->head;


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

int unfreeze_proc_exp_single_core_mode(tracer * curr_tracer) {

	struct timeval now;
	s64 now_ns;
	s64 start_ns;
	int i = 0;
	unsigned long flags;
	s64 used_quanta = 0;
	s64 total_quanta = 0;
	lxc_schedule_elem * curr_elem;
	int atleast_on_task_runnable;

	if (!curr_tracer)
		return FAIL;

	if (curr_tracer->nxt_round_burst_length == 0)
		return SUCCESS;
	
	clean_up_all_irrelevant_processes(curr_tracer);
	atleast_on_task_runnable = update_all_runnable_task_timeslices(curr_tracer);
	print_schedule_list(curr_tracer);
	

	if (curr_tracer->tracer_type == TRACER_TYPE_INS_VT) {
		if (!atleast_on_task_runnable)
			return SUCCESS;

		curr_tracer->tracer_task->burst_target = curr_tracer->nxt_round_burst_length;
		put_tracer_struct_read(curr_tracer);
		wait_for_insvt_tracer_completion(curr_tracer);
		get_tracer_struct_read(curr_tracer);
		return SUCCESS;
	}

	total_quanta = curr_tracer->nxt_round_burst_length;

	while (used_quanta < total_quanta) {
		curr_elem = get_next_runnable_task(curr_tracer);
		if (!curr_elem)
			break;

		put_tracer_struct_read(curr_tracer);
		get_tracer_struct_write(curr_tracer);
		used_quanta += curr_elem->quanta_curr_round;
		curr_elem->curr_task->burst_target = curr_elem->quanta_curr_round;
		curr_elem->quanta_curr_round = 0; // reset to zero
		put_tracer_struct_write(curr_tracer);
		
		wait_for_appvt_tracer_completion(curr_tracer, curr_elem->curr_task);
		get_tracer_struct_read(curr_tracer);
	}
	return SUCCESS;
}

/*
Assumes that curr_tracer read lock is acquired before entry. Must return with
read lock still acquired.
*/
int unfreeze_proc_exp_recurse(tracer * curr_tracer) {
	return unfreeze_proc_exp_single_core_mode(curr_tracer);
}

/*
Assumes no tracer lock is acquired prior to call.
*/
void clean_exp() {

	int i;
	tracer * curr_tracer;

	PDEBUG_I("Clean exp: Cleaning up initiated ...");
	mutex_lock(&exp_lock);
	for (i = 1; i <= tracer_num; i++) {

		curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
		if (curr_tracer) {
			get_tracer_struct_write(curr_tracer);
			clean_up_run_queue(curr_tracer);
			clean_up_schedule_list(curr_tracer);
			put_tracer_struct_write(curr_tracer);
			wake_up_interruptible(curr_tracer->w_queue);
		}
	}
	mutex_unlock(&exp_lock);
	atomic_set(&experiment_stopping, 0);
	experiment_status = NOTRUNNING;
	PDEBUG_I("Clean exp: Cleaning up finished ...");

}

