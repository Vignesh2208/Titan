#include "utils.h"


extern int experiment_status;
extern s64 virt_exp_start_time;
extern int tracer_num;
extern int schedule_list_size(tracer * tracer_entry);
extern hashmap get_tracer_by_id;
extern hashmap get_dilated_task_struct_by_pid;
extern struct mutex exp_lock;


void bind_to_cpu(struct task_struct * task, int target_cpu) {
	/*if (target_cpu != -1)
		set_cpus_allowed_ptr(task, cpumask_of(target_cpu));*/
}


char * alloc_mmap_pages(int npages)
{
    int i;
    char *mem = kmalloc(PAGE_SIZE * npages, GFP_KERNEL);

    if (!mem)
        return mem;

    memset(mem, 0, PAGE_SIZE * npages);
    for(i = 0; i < npages * PAGE_SIZE; i += PAGE_SIZE) {
        SetPageReserved(virt_to_page(((unsigned long)mem) + i));
    }

    return mem;
}

void free_mmap_pages(void *mem, int npages)
{
    int i;
    if (!mem)
        return;

    for(i = 0; i < npages * PAGE_SIZE; i += PAGE_SIZE) {
        ClearPageReserved(virt_to_page(((unsigned long)mem) + i));
    }

    kfree(mem);
}


/***
Used when reading the input from a userland process.
Will basically return the next number in a string
***/
int get_next_value (char *write_buffer) {
	int i;
	for (i = 1; * (write_buffer + i) >= '0'
	        && *(write_buffer + i) <= '9'; i++) {
		continue;
	}
	return (i + 1);
}

/***
 Convert string to integer
***/
int atoi(char *s) {
	int i, n;
	n = 0;
	for (i = 0; * (s + i) >= '0' && *(s + i) <= '9'; i++)
		n = 10 * n + *(s + i) - '0';
	return n;
}


/***
Given a pid, returns a pointer to the associated task_struct
***/
struct task_struct* find_task_by_pid(unsigned int nr) {
	struct task_struct* task;
	rcu_read_lock();
	task = pid_task(find_vpid(nr), PIDTYPE_PID);
	rcu_read_unlock();
	return task;
}

void initialize_tracer_entry(tracer * new_tracer, uint32_t tracer_id) {

	if (!new_tracer)
		return;

	/*struct dilated_task_struct * main_task;

	main_task = (struct dilated_task_struct *)kmalloc(
		sizeof(struct dilated_task_struct), GFP_KERNEL);
	if (!main_task) {
		PDEBUG_E("Failed to Allot Memory For Tracer Task !\n");
		BUG_ON(true);
	}

	memset(main_task, 0, sizeof(struct dilated_task_struct));
	main_task->associated_tracer_id = tracer_id;
	main_task->virt_start_time = START_VIRTUAL_TIME;
	main_task->curr_virt_time = START_VIRTUAL_TIME;
	main_task->pid = current->pid;
	main_task->base_task = current;


	new_tracer->timeline_assignment = 0;
	new_tracer->cpu_assignment = 0;
	new_tracer->tracer_id = tracer_id;
	new_tracer->tracer_pid = 0;
	new_tracer->round_start_virt_time = 0;
	new_tracer->nxt_round_burst_length = 0;
	new_tracer->curr_virtual_time = START_VIRTUAL_TIME;
	new_tracer->round_overshoot = 0;
	new_tracer->w_queue_wakeup_pid = 0;
	new_tracer->running_task_lookahead = 0;
	new_tracer->last_run = NULL;*/
	new_tracer->main_task = NULL;
	//new_tracer->main_task = main_task;

	llist_destroy(&new_tracer->schedule_queue);
	llist_destroy(&new_tracer->run_queue);

	llist_init(&new_tracer->schedule_queue);
	llist_init(&new_tracer->run_queue);	

	rwlock_init(&new_tracer->tracer_lock);
}

tracer * alloc_tracer_entry(uint32_t tracer_id) {

	tracer * new_tracer = NULL;

	new_tracer = (tracer *)kmalloc(sizeof(tracer), GFP_KERNEL);
	if (!new_tracer)
		return NULL;

	memset(new_tracer, 0, sizeof(tracer));

	initialize_tracer_entry(new_tracer, tracer_id);
	return new_tracer;
}

void free_tracer_entry(tracer * tracer_entry) {

	llist_destroy(&tracer_entry->schedule_queue);
    	llist_destroy(&tracer_entry->run_queue);
	kfree(tracer_entry);

}


/***
Set the time dilation variables to be consistent with all children
***/
void set_children_cpu(struct task_struct *aTask, int cpu) {
	struct list_head *list;
	struct task_struct *taskRecurse;
	struct task_struct *me;
	struct task_struct *t;

	if (aTask == NULL) {
		PDEBUG_E("Set Children CPU: Task does not exist\n");
		return;
	}
	if (aTask->pid == 0) {
		return;
	}

	me = aTask;
	t = me;

	/* set policy for all threads as well */
	do {
		if (t->pid != aTask->pid) {
			if (cpu != -1) {
				bind_to_cpu(t, cpu);
			}
		}
	} while_each_thread(me, t);

	list_for_each(list, &aTask->children) {
		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
			return;
		}
		if (cpu == -1) {
			bind_to_cpu(taskRecurse, cpu);
		}
		set_children_cpu(taskRecurse, cpu);
	}
}


/***
Set the time variables to be consistent with all children.
Assumes tracer write lock is acquired prior to call
***/
void set_children_time(tracer * tracer_entry, s64 time, int increment) {


	unsigned long flags;
	lxc_schedule_elem* curr_elem;

	BUG_ON(!tracer_entry || time <= 0);

	llist* schedule_queue = &tracer_entry->schedule_queue;
	llist_elem* head = schedule_queue->head;

	while (head != NULL) {
		curr_elem = (lxc_schedule_elem *)head->item;
		if (!curr_elem || !curr_elem->curr_task) {
			break;
		}

		if (increment) {	
			curr_elem->curr_task->curr_virt_time += time;
		} else {
			curr_elem->curr_task->curr_virt_time = time;
		}

		if (experiment_status != RUNNING)
			curr_elem->curr_task->wakeup_time = 0;
		head = head->next;
	}
}

/*
Assumes tracer read lock is acquired prior to call.
*/
void print_schedule_list(tracer* tracer_entry) {
	int i = 0;
	lxc_schedule_elem * curr;
	if (tracer_entry != NULL) {
		for (i = 0; i < schedule_list_size(tracer_entry); i++) {
			curr = llist_get(&tracer_entry->schedule_queue, i);
			if (curr != NULL) {
				PDEBUG_V("Schedule List Item No: %d, TRACER PID: %d, "
				         "TRACEE PID: %d, N_insns_curr_round: %d, "
				         "Size OF SCHEDULE QUEUE: %d\n", i,
				         tracer_entry->main_task->pid, curr->pid,
				         curr->quanta_curr_round,
				         schedule_list_size(tracer_entry));
			}
		}
	}
}


/***
My implementation of the kill system call. Will send a signal to a container.
Used for freezing/unfreezing containers
***/
int kill_p(struct task_struct *killTask, int sig) {
	struct siginfo info;
	int returnVal;
	info.si_signo = sig;
	info.si_errno = 0;
	info.si_code = SI_USER;
	if (killTask) {
		if ((returnVal = send_sig_info(sig, &info, killTask)) != 0) {
			PDEBUG_I("Kill: Error sending kill msg for pid %d\n",
			         killTask->pid);
		}
	}
	return returnVal;
}


/*tracer * get_tracer_for_task(struct task_struct * aTask) {

	if (!aTask || experiment_status != RUNNING)
		return NULL;
	struct dilated_task_struct * dilated_task 
		= hmap_get_abs(&get_dilated_task_struct_by_pid, aTask->pid);
	
	if (dilated_task)
		return (tracer *)dilated_task->associated_tracer;
	return NULL;
}*/


void get_tracer_struct_read(tracer* tracer_entry) {
	if (tracer_entry) {
		read_lock(&tracer_entry->tracer_lock);
	}
}

void put_tracer_struct_read(tracer* tracer_entry) {
	if (tracer_entry) {
		read_unlock(&tracer_entry->tracer_lock);
	}
}

void get_tracer_struct_write(tracer* tracer_entry) {
	if (tracer_entry) {
		write_lock(&tracer_entry->tracer_lock);
	}
}

void put_tracer_struct_write(tracer* tracer_entry) {
	if (tracer_entry) {
		write_unlock(&tracer_entry->tracer_lock);
	}
}

/*struct dilated_task_struct * get_dilated_task_struct(
	struct task_struct * task) {
	if (!task)
		return NULL;
	return (struct dilated_task_struct * )hmap_get_abs(&get_dilated_task_struct_by_pid, task->pid);
}


struct dilated_task_struct * find_dilated_task_by_pid(int pid) {
	return (struct dilated_task_struct *) hmap_get_abs(&get_dilated_task_struct_by_pid, pid);
}
*/

int convert_string_to_array(char * str, int * arr, int arr_size) 
{ 
    // get length of string str 
    int str_length = strlen(str); 
    int i = 0, j = 0;
    memset(arr, 0, sizeof(int)*arr_size);

  
    // Traverse the string 
    for (i = 0; str[i] != '\0'; i++) { 
  
        // if str[i] is ',' then split 
        if (str[i] == ',') { 
  
            // Increment j to point to next 
            // array location 
            j++; 
        } 
        else if (j < arr_size) { 
  
            // subtract str[i] by 48 to convert it to int 
            // Generate number by multiplying 10 and adding 
            // (int)(str[i]) 
            arr[j] = arr[j] * 10 + (str[i] - 48); 
        } 
    } 
    return j + 1;
} 
