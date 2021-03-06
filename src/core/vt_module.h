#ifndef __VT_MODULE_H
#define __VT_MODULE_H

#include "includes.h"


#define NOTRUNNING -1
#define RUNNING 0
#define FROZEN 1
#define STOPPING 2
#define NOT_INITIALIZED 0
#define INITIALIZED 1
#define DILATION_FILE "titan"

#define MAX_API_ARGUMENT_SIZE 100
#define BUF_MAX_SIZE MAX_API_ARGUMENT_SIZE

struct tracer_struct;

struct dilated_task_struct {
    s64 curr_virt_time;
    s64 wakeup_time;
    s64 virt_start_time;
    s64 burst_target;
    s64 lookahead;
    s64 trigger_time;

    s64 bulk_lookahead_expiry_time;
    long lookahead_anchor_type;

    int associated_tracer_id;
    int ready;
    int buffer_window_len;
    int pid;
    int syscall_waiting;
    int resumed_by_dilated_timer;

    s64 syscall_wait_return;
    wait_queue_head_t d_task_wqueue;
    struct task_struct * vt_exec_task;
    struct task_struct * base_task;
    struct tracer_struct * associated_tracer;
};


typedef struct pkt_info_struct {
    int payload_hash;
    int payload_len;
    s64 pkt_send_tstamp;
} pkt_info;

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
    s64 total_run_quanta;
    int pid;
    int blocked;
    struct dilated_task_struct * curr_task;
} lxc_schedule_elem;

typedef struct timeline_struct {
    int timeline_id;
    s64 nxt_round_burst_length;
    int status;
    int stopping;
    wait_queue_head_t * w_queue;
} timeline;

typedef struct process_tcp_stack_struct {
    int id;
    int num_sockets;
    int active;
    int rx_loop_complete;
    int stack_thread_waiting;
    int exit_status;
    s64 stack_return_tslice_quanta;
    s64 stack_rtx_send_time;
    struct tracer_struct * associated_tracer;
} process_tcp_stack;


typedef struct tracer_struct {
    struct dilated_task_struct * main_task;
    struct task_struct * vt_exec_task;
    int tracer_id;
    int tracer_pid;
    u32 timeline_assignment;
    u32 cpu_assignment;

    s64 curr_virtual_time;
    s64 round_overshoot;
    s64 round_start_virt_time;
    s64 nxt_round_burst_length;
    s64 running_task_lookahead;
    s64 earliest_arrival_time;

    rwlock_t tracer_lock;
    llist schedule_queue;
    llist run_queue;
    llist pkt_info_queue;
    llist process_tcp_stacks;
    wait_queue_head_t * w_queue;
    wait_queue_head_t stack_w_queue;
    int w_queue_wakeup_pid;
    lxc_schedule_elem * last_run;
    
} tracer;

#endif
