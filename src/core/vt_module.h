#ifndef __VT_MODULE_H
#define __VT_MODULE_H

#include "includes.h"

#define MAX_API_ARGUMENT_SIZE 100
#define BUF_MAX_SIZE MAX_API_ARGUMENT_SIZE


typedef struct overshoot_info_struct {
	s64 round_error;
	s64 n_rounds;
	s64 round_error_sq;
} overshoot_info;


typedef struct invoked_api_struct {
    char api_argument[MAX_API_ARGUMENT_SIZE];
    s64 return_value;
} invoked_api;

typedef struct sched_queue_element {
	s64 base_quanta;
	s64 quanta_left_from_prev_round;
	s64 quanta_curr_round;
	int pid;
	int blocked;
	struct task_struct * curr_task;
} lxc_schedule_elem;


typedef struct tracer_struct {
	struct task_struct * tracer_task;
	struct task_struct * spinner_task;
	struct task_struct * proc_to_control_task;
    struct task_struct * vt_exec_manager_task;

	int proc_to_control_pid;
	int create_spinner;
	int tracer_id;
    int buf_tail_ptr;
    int tracer_type;
	u32 cpu_assignment;
	s64 curr_virtual_time;
	s64 round_overshoot;
	s64 round_start_virt_time;
    s64 nxt_round_burst_length;

	rwlock_t tracer_lock;
	
    char run_q_buffer[BUF_MAX_SIZE];

	llist schedule_queue;
    llist run_queue;
	wait_queue_head_t w_queue;
	atomic_t w_queue_control;
} tracer;




#endif