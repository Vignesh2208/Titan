#include "includes.h"
#include "vt_module.h"
#include "sync_experiment.h"
#include "utils.h"

extern int tracer_num;
extern int EXP_CPUS;
extern int TOTAL_CPUS;
extern struct task_struct *round_task;
extern int experiment_status;
extern int initialization_status;
extern s64 virt_exp_start_time;

extern struct mutex exp_lock;
extern int *per_cpu_chain_length;
extern llist * per_cpu_tracer_list;

extern s64 * tracer_clock_array;
extern s64 * aligned_tracer_clock_array;

extern hashmap get_tracer_by_id;
extern hashmap get_tracer_by_pid;


extern atomic_t experiment_stopping;
extern wait_queue_head_t expstop_call_proc_wqueue;

extern atomic_t progress_n_rounds ;
extern atomic_t progress_n_enabled ;
extern atomic_t n_waiting_tracers;
extern wait_queue_head_t progress_sync_proc_wqueue;
extern wait_queue_head_t * tracer_wqueue;


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
		struct task_struct * taskRecurse = list_entry(list, struct task_struct, sibling);
		if (task_pid_nr_ns(taskRecurse, task_active_pid_ns(taskRecurse)) == pid)
			return taskRecurse;
		t =  get_task_ns(pid, taskRecurse);
		if (t != NULL)
			return t;
	}
	return NULL;
}



/***
Remove head of schedule queue and return the task_struct of the head element.
Assumes tracer write lock is acquired prior to call.
***/
int pop_schedule_list(tracer * tracer_entry) {
	if (!tracer_entry)
		return 0;
	lxc_schedule_elem * head;
	head = llist_pop(&tracer_entry->schedule_queue);
	struct task_struct * curr_task;
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

	if (tracer_entry && tracer_entry->tracer_task) {
		tracer_entry->tracer_task->associated_tracer_id = -1;
		tracer_entry->tracer_task->virt_start_time = 0;
		tracer_entry->tracer_task->curr_virt_time = 0;
		tracer_entry->tracer_task->wakeup_time = 0;
		tracer_entry->tracer_task->burst_target = 0;
	}
	while ( pid != 0) {
		pid = pop_schedule_list(tracer_entry);
		if (pid) {
			pid_struct = find_get_pid(pid);
			task = pid_task(pid_struct, PIDTYPE_PID);
			if (task != NULL) {
				task->virt_start_time = 0;
				task->curr_virt_time = 0;
				task->wakeup_time = 0;
				task->burst_target = 0;
				task->associated_tracer_id = -1;
			}
		} else
			break;
	}
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
acquired.
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

	if (!tracee) {
		PDEBUG_E("add_to_tracer_schedule_queue: tracee: %d not found!\n", tracee_pid);
		return;
	}
	
	if (experiment_status == STOPPING) {
		PDEBUG_E("add_to_tracer_schedule_queue: cannot add when experiment is stopping !\n");
		return;
	}

	PDEBUG_I("add_to_tracer_schedule_queue: "
	         "Adding new tracee %d to tracer-id: %d\n",
	         tracee->pid, tracer_entry->tracer_id);

	new_elem = (lxc_schedule_elem *)kmalloc(sizeof(lxc_schedule_elem), GFP_KERNEL);
	memset(new_elem, 0, sizeof(lxc_schedule_elem));
	if (!new_elem) {
		PDEBUG_E("add_to_tracer_schedule_queue: "
		         "Tracer %d, tracee %d. Failed to alot Memory\n",
		         tracer_entry->tracer_id, tracee->pid);
		return;
	}

	new_elem->pid = tracee->pid;
	new_elem->curr_task = tracee;
	tracee->associated_tracer_id = tracer_entry->tracer_id;

	new_elem->base_quanta = PROCESS_MIN_QUANTA_NS;
	new_elem->quanta_left_from_prev_round = 0;
	new_elem->quanta_curr_round = 0;
	new_elem->blocked = 0;

		

	if (tracer_entry->tracer_type == TRACER_TYPE_APP_VT) {
		BUG_ON(!aligned_tracer_clock_array);
		tracee->tracer_clock = (s64 *)&aligned_tracer_clock_array[tracee->associated_tracer_id - 1];
		tracee->vt_exec_task_wqueue = &tracer_wqueue[tracer_entry->cpu_assignment - 2];
		tracee->ready = 0;
	} else {
		tracee->vt_exec_task_wqueue = NULL;
		tracee->ready = 0;
	}

	if (!tracee->virt_start_time) {
		tracee->virt_start_time = virt_exp_start_time;
		tracee->curr_virt_time = tracer_entry->curr_virtual_time;
		tracee->wakeup_time = 0;
		tracee->burst_target = 0;
	}

	llist_append(&tracer_entry->schedule_queue, new_elem);


	bitmap_zero((&tracee->cpus_allowed)->bits, 8);
	cpumask_set_cpu(tracer_entry->cpu_assignment, &tracee->cpus_allowed);
	sp.sched_priority = 99;
	sched_setscheduler(tracee, SCHED_RR, &sp);
	PDEBUG_E("add_to_tracer_schedule_queue:  "
	         "Tracee %d added successfully to tracer-id: %d. schedule list size: %d\n",
	         tracee->pid, tracer_entry->tracer_id, schedule_list_size(tracer_entry));
}



/*
Assumes tracer write lock is acquired prior to call. Must return with lock still
acquired.
*/
void remove_from_tracer_schedule_queue(tracer * tracer_entry, int tracee_pid) 
{

	lxc_schedule_elem * curr_elem;
	lxc_schedule_elem * removed_elem;
	struct task_struct * tracee = find_task_by_pid(tracee_pid);

	llist_elem * head_1;
	llist_elem * head_2;
	int pos = 0;

	

	PDEBUG_I("remove_from_tracer_schedule_queue: "
	         "Removing tracee %d from tracer-id: %d\n",
	         tracee_pid, tracer_entry->tracer_id);

	head_1 = tracer_entry->schedule_queue.head;
	head_2 = tracer_entry->run_queue.head;

	if (tracee) {
		tracee->associated_tracer_id = -1;
		tracee->virt_start_time = 0;
		tracee->curr_virt_time = 0;
		tracee->wakeup_time = 0;
		tracee->burst_target = 0;
	}

	while (head_1 != NULL || head_2 != NULL) {

		if (head_1) {
			curr_elem = (lxc_schedule_elem *)head_1->item;
			if (curr_elem && curr_elem->pid == tracee_pid) {
				removed_elem = llist_remove_at(&tracer_entry->schedule_queue, pos);
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

	if (removed_elem) 
		kfree(removed_elem);
}


/**
* write_buffer: <tracer_id>,<tracer_type>,<meta_task_type>,<spinner_pid> or <proc_to_control_pid>
**/
int register_tracer_process(char * write_buffer) {

	tracer * new_tracer;
	uint32_t tracer_id;
	int i;
	int best_cpu = 0;
	int meta_task_type = 0;
	int tracer_type;
	int spinner_pid = 0, process_to_control_pid = 0;
	struct task_struct * spinner_task = NULL;
	struct task_struct * process_to_control = NULL;
	int api_args[MAX_API_ARGUMENT_SIZE];
	int num_args;


	num_args = convert_string_to_array(write_buffer, api_args, MAX_API_ARGUMENT_SIZE);

	BUG_ON(num_args <= 2);

	tracer_type = api_args[1];
	tracer_id = api_args[0];
	meta_task_type = api_args[2];

	if (tracer_type != TRACER_TYPE_INS_VT && tracer_type != TRACER_TYPE_APP_VT) {
		PDEBUG_E("Unknown Tracer Type !");
		return FAIL;
	}

	if (meta_task_type == 1) {
		BUG_ON(num_args != 4);
		spinner_pid  = api_args[3];
		if (spinner_pid <= 0) {
			PDEBUG_E("Spinner pid must be greater than zero. "
			         "Received value: %d\n", spinner_pid);
			return FAIL;
		}

		spinner_task = get_task_ns(spinner_pid, current);

		if (!spinner_task) {
			PDEBUG_E("Spinner pid task not found. "
			         "Received pid: %d\n", spinner_pid);
			return FAIL;
		}


	} else if (meta_task_type != 0) {
		meta_task_type = 0;
		BUG_ON(num_args != 4);
		process_to_control_pid = api_args[3];
		if (process_to_control_pid <= 0) {
			PDEBUG_E("proccess_to_control_pid must be greater than zero. "
			         "Received value: %d\n", process_to_control_pid);
			return FAIL;
		}

		process_to_control = find_task_by_pid(process_to_control_pid);
		if (!process_to_control) {
			PDEBUG_E("proccess_to_control_pid task not found. "
			         "Received pid: %d\n", process_to_control_pid);
			return FAIL;
		}
	}


	PDEBUG_I("Register Tracer: Starting for tracer: %d\n", tracer_id);

	new_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);

	if (!new_tracer) {
		new_tracer = alloc_tracer_entry(tracer_id, tracer_type);
		hmap_put_abs(&get_tracer_by_id, tracer_id, new_tracer);
		hmap_put_abs(&get_tracer_by_pid, current->pid, new_tracer);
	} else {
		initialize_tracer_entry(new_tracer, tracer_id, tracer_type);
		hmap_put_abs(&get_tracer_by_pid, current->pid, new_tracer);
	}

	if (!new_tracer)
		return FAIL;

	for (i = 0; i < EXP_CPUS; i++) {
		if (per_cpu_chain_length[i] < per_cpu_chain_length[best_cpu])
			best_cpu = i;
	}

	mutex_lock(&exp_lock);
	
	per_cpu_chain_length[best_cpu] ++;
	new_tracer->tracer_id = tracer_id;
	new_tracer->cpu_assignment = best_cpu + 2;
	new_tracer->tracer_task = current;
	new_tracer->create_spinner = meta_task_type;
	new_tracer->proc_to_control_task = NULL;
	new_tracer->spinner_task = NULL;
	new_tracer->w_queue = &tracer_wqueue[new_tracer->cpu_assignment - 2];
	new_tracer->tracer_pid = current->pid;

	current->associated_tracer_id = tracer_id;
	current->ready = 0;

	++tracer_num;
	llist_append(&per_cpu_tracer_list[best_cpu], new_tracer);
	mutex_unlock(&exp_lock);

	if (meta_task_type) {
		PDEBUG_I("Register Tracer: Pid: %d, ID: %d "
		 "assigned cpu: %d, spinner pid = %d\n",
		 current->pid, new_tracer->tracer_id,
		 new_tracer->cpu_assignment, spinner_pid);

	} 

	if (process_to_control != NULL) {
		PDEBUG_I("Register Tracer: Pid: %d, ID: %d "
		"assigned cpu: %d, process_to_control pid = %d\n",
		current->pid, new_tracer->tracer_id,
		new_tracer->cpu_assignment, process_to_control_pid);

	}

	bitmap_zero((&current->cpus_allowed)->bits, 8);

	get_tracer_struct_write(new_tracer);
	cpumask_set_cpu(new_tracer->cpu_assignment, &current->cpus_allowed);


	if (meta_task_type && spinner_task) {
		PDEBUG_I("Set Spinner Task for Tracer: %d, Spinner: %d\n",
		         current->pid, spinner_task->pid);
		new_tracer->spinner_task = spinner_task;
		kill_p(spinner_task, SIGSTOP);
		bitmap_zero((&spinner_task->cpus_allowed)->bits, 8);
		cpumask_set_cpu(1, &spinner_task->cpus_allowed);

	} 
	
	if (!meta_task_type && process_to_control != NULL) {
		add_to_tracer_schedule_queue(new_tracer, process_to_control_pid);
		new_tracer->proc_to_control_pid = process_to_control_pid;
		new_tracer->proc_to_control_task = process_to_control;
	} else {
		new_tracer->proc_to_control_pid = -1;
	}

	put_tracer_struct_write(new_tracer);
	PDEBUG_I("Register Tracer: Finished for tracer: %d\n", tracer_id);
	return new_tracer->cpu_assignment;	//return the allotted cpu back to the tracer.
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
			set_children_time(tracer_entry, tracer_entry->tracer_task,
								tracer_entry->curr_virtual_time, 0);

		if (tracer_entry->spinner_task)
			tracer_entry->spinner_task->curr_virt_time =
			    tracer_entry->curr_virtual_time;

		if (tracer_entry->proc_to_control_task != NULL) {
			if (find_task_by_pid(tracer_entry->proc_to_control_pid) != NULL)
				tracer_entry->proc_to_control_task->curr_virt_time =
				    tracer_entry->curr_virtual_time;
		}

	}

}


void update_init_task_virtual_time(s64 time_to_set) {
	init_task.curr_virt_time = time_to_set;
}

/*
Assumes no tracer lock is acquired prior to call
*/
void update_all_tracers_virtual_time(int cpuID) {
	llist_elem * head;
	llist * tracer_list;
	tracer * curr_tracer;
	s64 overshoot_err;
	s64 target_increment;

	tracer_list =  &per_cpu_tracer_list[cpuID];
	head = tracer_list->head;


	while (head != NULL) {

		curr_tracer = (tracer*)head->item;
		get_tracer_struct_write(curr_tracer);
		target_increment = curr_tracer->nxt_round_burst_length;

		if (target_increment > 0) {
			if (curr_tracer && curr_tracer->tracer_type == TRACER_TYPE_APP_VT) {
				BUG_ON(!aligned_tracer_clock_array);
				//WARN_ON(aligned_tracer_clock_array[curr_tracer->tracer_id - 1] < curr_tracer->round_start_virt_time +  target_increment);
				if (aligned_tracer_clock_array[curr_tracer->tracer_id - 1] < curr_tracer->round_start_virt_time +  target_increment) {
					// this can happen if no process belonging to tracer was runnable in the previous round.
					aligned_tracer_clock_array[curr_tracer->tracer_id - 1] = curr_tracer->round_start_virt_time +  target_increment;
				}
				overshoot_err = aligned_tracer_clock_array[curr_tracer->tracer_id - 1] - (curr_tracer->round_start_virt_time +  target_increment);
				curr_tracer->round_overshoot = overshoot_err;
				target_increment = target_increment + overshoot_err;
				update_all_children_virtual_time(curr_tracer, target_increment);

				BUG_ON(aligned_tracer_clock_array[curr_tracer->tracer_id - 1] != curr_tracer->curr_virtual_time);
			} else {
				update_all_children_virtual_time(curr_tracer, target_increment);
			}
		} else {
			PDEBUG_E("Tracer nxt round burst length is 0\n");
		}
		curr_tracer->nxt_round_burst_length = 0;
		put_tracer_struct_write(curr_tracer);
		head = head->next;
	}
}





/**
* write_buffer: result which indicates overflow number of instructions.
  It specifies the total number of instructions by which the tracer overshot
  in the current round. The overshoot is ignored if experiment type is CS.
  Assumes no tracer lock is acquired prior to call.
**/

int handle_tracer_results(tracer * curr_tracer, int * api_args, int num_args) {

	struct pid *pid_struct;
	struct task_struct * task;
	int i, pid_to_remove;
	int wakeup = 0;

	if (!curr_tracer)
		return FAIL;

	get_tracer_struct_write(curr_tracer);
	for (i = 0; i < num_args; i++) {
		pid_to_remove = api_args[i];
		if (pid_to_remove <= 0)
			break;

		PDEBUG_I("Handle tracer results: Pid: %d, Tracer ID: %d, "
			     "Ignoring Process: %d\n", curr_tracer->tracer_task->pid,
			     curr_tracer->tracer_id, pid_to_remove);
		struct task_struct * mappedTask = get_task_ns(pid_to_remove, curr_tracer->tracer_task);
		if (curr_tracer->tracer_type == TRACER_TYPE_APP_VT) {
			WARN_ON(mappedTask && mappedTask->pid != current->pid);
			if (mappedTask && mappedTask->pid == current->pid && current->burst_target > 0) {
				wakeup = 1;
			}
		}
		if (mappedTask != NULL) {
			remove_from_tracer_schedule_queue(curr_tracer, mappedTask->pid);
		}

		
	}

	put_tracer_struct_write(curr_tracer);

	if (wakeup) {
		PDEBUG_V("APPVT Tracer signalling resume. Tracer ID: %d\n",
	            curr_tracer->tracer_id);
		curr_tracer->w_queue_wakeup_pid = 1;
		wake_up_interruptible(curr_tracer->w_queue);

	} else {
		signal_cpu_worker_resume(curr_tracer);
	}
	return SUCCESS;
}

void wait_for_insvt_tracer_completion(tracer * curr_tracer)
{

	if (!curr_tracer)
		return;
	BUG_ON(curr_tracer->tracer_type != TRACER_TYPE_INS_VT);
	curr_tracer->w_queue_wakeup_pid = curr_tracer->tracer_pid;
	wake_up_interruptible(curr_tracer->w_queue);
	wait_event_interruptible(*curr_tracer->w_queue, curr_tracer->w_queue_wakeup_pid == 1);
	PDEBUG_V("Resuming from Tracer completion for Tracer ID: %d\n", curr_tracer->tracer_id);

}

void wait_for_appvt_tracer_completion(tracer * curr_tracer, struct task_struct * relevant_task)
{
	if (!curr_tracer || !relevant_task) {
		return;
	}

	BUG_ON(curr_tracer->tracer_type != TRACER_TYPE_APP_VT);
	curr_tracer->w_queue_wakeup_pid = relevant_task->pid;
	wake_up_interruptible(curr_tracer->w_queue);
	wait_event_interruptible(*curr_tracer->w_queue, relevant_task->burst_target == 0 || curr_tracer->w_queue_wakeup_pid == 1);
	PDEBUG_V("Resuming from Tracer completion for Tracer ID: %d\n", curr_tracer->tracer_id);

}

void signal_cpu_worker_resume(tracer * curr_tracer) {

	if (!curr_tracer)
		return;

	if (curr_tracer->tracer_type == TRACER_TYPE_INS_VT) {
		PDEBUG_V("INSVT Tracer signalling resume. Tracer ID: %d\n",
	             curr_tracer->tracer_id);

		curr_tracer->w_queue_wakeup_pid = 1;
		wake_up_interruptible(curr_tracer->w_queue);
	} else {
		if (current->burst_target > 0) {
			PDEBUG_V("APPVT Tracer signalling resume. Tracer ID: %d\n",
	            curr_tracer->tracer_id);
			curr_tracer->w_queue_wakeup_pid = 1;
			wake_up_interruptible(curr_tracer->w_queue);
		}
	}

}

int handle_stop_exp_cmd() {
	if (progress_by(0, 1) == SUCCESS)
		return cleanup_experiment_components();
	return FAIL;
}

int handle_initialize_exp_cmd(char * buffer) {
	int num_expected_tracers;

	num_expected_tracers = atoi(buffer);
	if (num_expected_tracers) {
		return initialize_experiment_components(num_expected_tracers);
	}

	return FAIL;
}


/**
* write_buffer: <tracer_id>,<network device name>
* Can be called after successfull synchronize and freeze command
**/
int handle_set_netdevice_owner_cmd(char * write_buffer) {


	char dev_name[IFNAMSIZ];
	int tracer_id;
	struct pid * pid_struct = NULL;
	int i = 0;
	struct net * net;
	struct task_struct * task;
	tracer * curr_tracer;
	int found = 0;

	for (i = 0; i < IFNAMSIZ; i++)
		dev_name[i] = '\0';

	tracer_id = atoi(write_buffer);
	int next_idx = get_next_value(write_buffer);


	for (i = 0; * (write_buffer + next_idx + i) != '\0'
	        && *(write_buffer + next_idx + i) != ','  && i < IFNAMSIZ ; i++)
		dev_name[i] = *(write_buffer + next_idx + i);

	PDEBUG_A("Set Net Device Owner: Received tracer id: %d, Dev Name: %s\n",
	         tracer_id, dev_name);

	struct net_device * dev;
	curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);

	if (!curr_tracer)
		return FAIL;

	task = curr_tracer->spinner_task;
	if (!task) {
		PDEBUG_E("Must have spinner task to "
				 "be able to set net device owner\n");
		return FAIL;
	}

	pid_struct = get_task_pid(task, PIDTYPE_PID);
	if (!pid_struct) {
		PDEBUG_E("Pid Struct of spinner task not found for tracer: %d\n",
					curr_tracer->tracer_task->pid);
		return FAIL;
	}

	write_lock_bh(&dev_base_lock);
	for_each_net(net) {
		for_each_netdev(net, dev) {
			if (dev != NULL) {
				if (strcmp(dev->name, dev_name) == 0 && !found) {
					PDEBUG_A("Set Net Device Owner: "
							 "Found Specified Net Device: %s\n", dev_name);
					dev->owner_pid = pid_struct;
					found = 1;
				} else if (found) {
					break;
				}
			}
		}
	}
	write_unlock_bh(&dev_base_lock);
	return SUCCESS;
}


s64 get_dilated_time(struct task_struct * task) {
	struct timeval tv;
	do_gettimeofday(&tv);
	s64 now = timeval_to_ns(&tv);

	if (task->virt_start_time != 0) {

		if (task->tracer_clock != NULL) {
			return *(task->tracer_clock);
		}
		return task->curr_virt_time;
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

	PDEBUG_V("Handle gettimepid: Received Pid = %d\n", pid);

	task = find_task_by_pid(pid);
	if (!task)
		return 0;
	return get_dilated_time(task);
}


