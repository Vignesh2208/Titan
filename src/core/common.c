#include "includes.h"
#include "vt_module.h"
#include "sync_experiment.h"
#include "utils.h"

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


extern int run_usermode_tracer_spin_process(char *path, char **argv,
        char **envp, int wait);
extern void signal_cpu_worker_resume(tracer * curr_tracer);
extern s64 get_dilated_time(struct task_struct * task);
extern int cleanup_experiment_components(void);





struct task_struct* get_task_ns(pid_t pid, struct task_struct * parent) {
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
		struct task_struct * taskRecurse = list_entry(list
			 struct task_struct, sibling);
		if (task_pid_nr_ns(taskRecurse, task_active_pid_ns(taskRecurse)) == pid)
			return taskRecurse;
		t =  get_task_ns(pid, taskRecurse);
		if (t != NULL)
			return t;
	}
	return NULL;
}



/***
Remove head of schedule queue and return the pid of the head element.
Assumes tracer write lock is acquired prior to call.
***/
int pop_schedule_list(tracer * tracer_entry) {
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
void requeue_schedule_list(tracer * tracer_entry) {
	if (tracer_entry == NULL)
		return;
	llist_requeue(&tracer_entry->schedule_queue);
}


/***
Remove head of schedule queue and return the task_struct of the head element.
Assumes tracer write lock is acquired prior to call.
***/
lxc_schedule_elem * pop_run_queue(tracer * tracer_entry) {
	if (!tracer_entry)
		return 0;
	return llist_pop(&tracer_entry->run_queue);
}

/***
Requeue schedule queue, i.e pop from head and add to tail. Assumes tracer
write lock is acquired prior to call
***/
void requeue_run_queue(tracer * tracer_entry) {
	if (!tracer_entry)
		return;
	llist_requeue(&tracer_entry->run_queue);
}

/*
Assumes tracer write lock is acquired prior to call. Must return with lock
still held
*/
void clean_up_schedule_list(tracer * tracer_entry) {

	int pid = 1;
	struct pid *pid_struct;
	struct task_struct * task;

	BUG_ON(!tracer_entry || !tracer_entry->main_task);
	
	lxc_schedule_elem* curr_elem;
	llist* schedule_queue = &tracer_entry->schedule_queue;
	llist_elem* head = schedule_queue->head;

	while ( pid != 0) {
		pid = pop_schedule_list(tracer_entry);
	}
	tracer_entry->main_task = NULL;
}

void clean_up_run_queue(tracer * tracer_entry) {

	if (!tracer_entry)
		return;

	while (llist_size(&tracer_entry->run_queue)) {
		pop_run_queue(tracer_entry);
	}
}

/*
Assumes tracer read lock is acquired before function call
*/
int schedule_list_size(tracer * tracer_entry) {
	if (!tracer_entry)
		return 0;
	return llist_size(&tracer_entry->schedule_queue);
}


/*
Assumes tracer read lock is acquired before function call
*/
int run_queue_size(tracer * tracer_entry) {
	if (!tracer_entry)
		return 0;
	return llist_size(&tracer_entry->run_queue);
}



/*
Assumes tracer write lock is acquired prior to call. Must return with lock still
acquired. It is assumed that tracee_pid is according to the outermost namespace
*/
void add_to_tracer_schedule_queue(tracer * tracer_entry,
                                  int tracee_pid) {

	lxc_schedule_elem * new_elem;
	//represents base time allotted to each process by the Kronos scheduler
	//Only useful in single core mode.
	s64 base_time_quanta;
	s64 base_quanta_n_insns = 0;
	s32 rem = 0;
	struct sched_param sp;
	struct task_struct * tracee = find_task_by_pid(tracee_pid);
	struct dilated_task_struct * tracee_dilated_task_struct;

	if (!tracee) {
		PDEBUG_E("add_to_tracer_schedule_queue: tracee: %d not found!\n",
				 tracee_pid);
		return;
	}

	if (find_dilated_task_by_pid(tracee_pid)) {
		PDEBUG_I("Tracee : %d already exists !\n", tracee_pid);
		return;
	}
	
	if (experiment_status == STOPPING) {
		PDEBUG_E("add_to_tracer_schedule_queue: cannot add when experiment is "
				 "stopping !\n");
		return;
	}

	PDEBUG_I("add_to_tracer_schedule_queue: "
	         "Adding new tracee %d to tracer-id: %d\n",
	         tracee->pid, tracer_entry->tracer_id);

	new_elem = (lxc_schedule_elem *)kmalloc(sizeof(lxc_schedule_elem),
				GFP_KERNEL);
	
	if (!new_elem) {
		PDEBUG_E("Tracer %d, tracee: %d. Failed to alot Memory to add tracee\n",
		         tracer_entry->tracer_id, tracee->pid);
	}
	memset(new_elem, 0, sizeof(lxc_schedule_elem));

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

	tracee_dilated_task_struct->associated_tracer_id = tracer_entry->tracer_id;
	tracee_dilated_task_struct->base_task = tracee;
	tracee_dilated_task_struct->pid = tracee->pid;

	if (!tracee_dilated_task_struct->virt_start_time) {
		tracee_dilated_task_struct->virt_start_time = virt_exp_start_time;
		tracee_dilated_task_struct->curr_virt_time 
			= tracer_entry->curr_virtual_time;
		tracee_dilated_task_struct->wakeup_time = 0;
		tracee_dilated_task_struct->burst_target = 0;
		tracee_dilated_task_struct->buffer_window_len = 0;
		tracee_dilated_task_struct->lookahead = 0;
	}


	
	BUG_ON(!chaintask[tracer_entry->timeline_assignment]);
	BUG_ON(tracer_entry->vt_exec_task 
		!= chaintask[tracer_entry->timeline_assignment]);
	tracee_dilated_task_struct->vt_exec_task = tracer_entry->vt_exec_task;
	tracee_dilated_task_struct->ready = 0;
	tracee_dilated_task_struct->associated_tracer = tracer_entry;
	
	llist_append(&tracer_entry->schedule_queue, new_elem);
	bitmap_zero((&tracee->cpus_allowed)->bits, 8);
	cpumask_set_cpu(tracer_entry->cpu_assignment, &tracee->cpus_allowed);
	hmap_put_abs(&get_dilated_task_struct_by_pid, current->pid,
				tracee_dilated_task_struct);
	sp.sched_priority = 99;

	sched_setscheduler(tracee, SCHED_RR, &sp);
	PDEBUG_I("add_to_tracer_schedule_queue:  Tracee %d added successfully to "
			"tracer-id: %d. schedule list size: %d\n", tracee->pid,
			tracer_entry->tracer_id, schedule_list_size(tracer_entry));
}



/*
Assumes tracer write lock is acquired prior to call. Must return with lock still
acquired. It is assumed that tracee_pid is according to the outermost namespace
*/
void remove_from_tracer_schedule_queue(tracer * tracer_entry, int tracee_pid) {

	lxc_schedule_elem * curr_elem;
	lxc_schedule_elem * removed_elem;
	struct task_struct * tracee = find_task_by_pid(tracee_pid);

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
		hmap_remove_abs(&get_dilated_task_struct_by_pid, removed_elem->pid);
		kfree(removed_elem->curr_task);
		kfree(removed_elem);
	}
}


/**
* write_buffer: <tracer_id>,<registration_type>,<optional_timeline_id>
**/
int register_tracer_process(char * write_buffer) {

	tracer * new_tracer;
	uint32_t tracer_id;
	int i;
	int best_cpu = 0;
	int registration_type = 0;
	int assigned_timeline_id = 0;
	int assigned_cpu = 0;
	int api_args[MAX_API_ARGUMENT_SIZE];
	int num_args;


	num_args = convert_string_to_array(write_buffer, api_args,
									   MAX_API_ARGUMENT_SIZE);

	BUG_ON(num_args < 2);

	tracer_id = api_args[0];
	registration_type = api_args[1];


	if (registration_type == 1) {
		BUG_ON(num_args != 3);
		assigned_timeline_id  = api_args[3];
		if (assigned_timeline_id < 0) {
			PDEBUG_E("assigned_timeline_id pid must be >=0 zero. "
			         "Received value: %d\n", assigned_timeline_id);
			return FAIL;
		}
		assigned_cpu = (assigned_timeline_id % EXP_CPUS);

	} else {
		registration_type = 0;
		for (i = 0; i < EXP_CPUS; i++) {
			if (per_timeline_chain_length[i] < per_timeline_chain_length[best_cpu])
				best_cpu = i;
		}
		BUG_ON(num_args != 2);
		assigned_timeline_id = best_cpu;
		assigned_cpu = best_cpu;
	}


	PDEBUG_I("Register Tracer: Starting for tracer: %d\n", tracer_id);

	new_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);

	if (!new_tracer) {
		new_tracer = alloc_tracer_entry(tracer_id);
		hmap_put_abs(&get_tracer_by_id, tracer_id, new_tracer);
		hmap_put_abs(&get_tracer_by_pid, current->pid, new_tracer);
	} else {
		initialize_tracer_entry(new_tracer, tracer_id);
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

	
	PDEBUG_I("Register Tracer: ID: %d assigned timeline: %d\n",
			 new_tracer->tracer_id, new_tracer->timeline_assignment);

	bitmap_zero((&current->cpus_allowed)->bits, 8);
	get_tracer_struct_write(new_tracer);
	cpumask_set_cpu(new_tracer->timeline_assignment, &current->cpus_allowed);


	add_to_tracer_schedule_queue(new_tracer, current->pid);


	put_tracer_struct_write(new_tracer);
	PDEBUG_I("Register Tracer: Finished for tracer: %d\n", tracer_id);

	//return the allotted timeline_id back to the tracer.
	return new_tracer->timeline_assignment;	
}



/*
Assumes tracer write lock is acquired prior to call. Must return with write lock
still acquired.
*/
void update_all_children_virtual_time(tracer * tracer_entry, s64 time_increment) {
	if (tracer_entry && tracer_entry->tracer_task) {

		tracer_entry->round_start_virt_time += time_increment;
		tracer_entry->curr_virtual_time = tracer_entry->round_start_virt_time;

		if (schedule_list_size(tracer_entry) > 0)
			set_children_time(tracer_entry, tracer_entry->curr_virtual_time, 0);
	}
}



/*
Assumes no tracer lock is acquired prior to call
*/
void update_all_tracers_virtual_time(int timelineID) {
	llist_elem * head;
	llist * tracer_list;
	tracer * curr_tracer;
	s64 target_increment;

	tracer_list =  &per_timeline_tracer_list[timelineID];
	head = tracer_list->head;


	while (head != NULL) {

		curr_tracer = (tracer*)head->item;
		get_tracer_struct_write(curr_tracer);
		target_increment = curr_tracer->nxt_round_burst_length;

		if (target_increment > 0) {
			update_all_children_virtual_time(curr_tracer, target_increment);
		} else {
			PDEBUG_E("Tracer nxt round burst length is 0\n");
		}
		curr_tracer->nxt_round_burst_length = 0;
		put_tracer_struct_write(curr_tracer);
		head = head->next;
	}
}






/**
* write_buffer: result specifies process-IDs to remove from tracer schedule
  and run queues. These process-IDs are no longer a part of the VT experiment.
**/

int handle_tracer_results(tracer * curr_tracer, int * api_args, int num_args) {

	struct pid *pid_struct;
	struct task_struct * task;
	struct dilated_task_struct * dilated_task;
	int i, pid_to_remove;

	if (!curr_tracer)
		return FAIL;

	get_tracer_struct_write(curr_tracer);
	for (i = 0; i < num_args; i++) {
		pid_to_remove = api_args[i];
		if (pid_to_remove <= 0)
			break;

		PDEBUG_I("Handle tracer results: Tracer ID: %d, "
			     "Ignoring Process: %d\n", curr_tracer->tracer_id,
				 pid_to_remove);
		struct task_struct * mappedTask = get_task_ns(pid_to_remove,
			curr_tracer->main_task->base_task);

		WARN_ON(mappedTask && mappedTask->pid != current->pid);
		
		if (mappedTask != NULL) {
			remove_from_tracer_schedule_queue(curr_tracer, mappedTask->pid);
		}
	}

	put_tracer_struct_write(curr_tracer);
	signal_cpu_worker_resume(curr_tracer);
	return SUCCESS;
}

void wait_for_task_completion(tracer * curr_tracer,
							  struct task_struct * relevant_task) {
	if (!curr_tracer || !relevant_task) {
		return;
	}
	int ret;
	PDEBUG_V("Waiting for Tracer completion for Tracer ID: %d\n",
			 curr_tracer->tracer_id);
	curr_tracer->w_queue_wakeup_pid = relevant_task->pid;
	wake_up_interruptible(curr_tracer->w_queue);
	wait_event_interruptible(*curr_tracer->w_queue,
							 curr_tracer->w_queue_wakeup_pid == 1);
	PDEBUG_V("Resuming from Tracer completion for Tracer ID: %d\n",
			 curr_tracer->tracer_id);

}

void signal_cpu_worker_resume(tracer * curr_tracer) {

	if (!curr_tracer)
		return;

	
	PDEBUG_V("Tracer signalling resume. Tracer ID: %d\n",
			curr_tracer->tracer_id);
	curr_tracer->w_queue_wakeup_pid = 1;
	wake_up_interruptible(curr_tracer->w_queue);
}

int handle_stop_exp_cmd() {
	experiment_status = STOPPING;
	initiate_experiment_stop_operation();
	return cleanup_experiment_components();
}

int handle_initialize_exp_cmd(int exp_type, int num_timelines,
							  int num_expected_tracers) {

	if (num_expected_tracers) {
		return initialize_experiment_components(exp_type, num_timelines,
												num_expected_tracers);
	}
	return FAIL;
}



s64 get_dilated_time(struct task_struct * task) {
	struct timeval tv;
	tracer * associated_tracer;
	lxc_schedule_elem * curr_elem;
	llist_elem * head;
	struct dilated_task_struct * dilated_task 
		= hmap_get_abs(&get_dilated_task_struct_by_pid, task->pid);

	do_gettimeofday(&tv);
	s64 now = timeval_to_ns(&tv);

	if (!dilated_task)
		return now;

	associated_tracer = dilated_task->associated_tracer;

	if (associated_tracer && associated_tracer->curr_virt_time) {
		return associated_tracer->curr_virt_time;
	}
	return now;

}

/**
* write_buffer: <pid> of process
**/
s64 handle_gettimepid(char * write_buffer) {

	struct pid *pid_struct;
	struct task_struct * task;
	int pid;

	pid = atoi(write_buffer);

	PDEBUG_V("Handling gettimepid: Received Pid = %d\n", pid);

	task = find_task_by_pid(pid);
	if (!task)
		return 0;
	return get_dilated_time(task);
}


