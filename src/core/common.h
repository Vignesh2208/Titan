#ifndef __COMMON_H
#define __COMMON_H

#include "includes.h"
#include "vt_module.h"

struct task_struct* get_task_ns(pid_t pid, struct task_struct * parent);
int pop_schedule_list(tracer * tracer_entry);
void requeue_schedule_list(tracer * tracer_entry);
lxc_schedule_elem * pop_run_queue(tracer * tracer_entry);
void requeue_run_queue(tracer * tracer_entry);
void clean_up_schedule_list(tracer * tracer_entry);
void clean_up_run_queue(tracer * tracer_entry);
int schedule_list_size(tracer * tracer_entry);
int run_queue_size(tracer * tracer_entry);
void add_to_tracer_schedule_queue(tracer * tracer_entry,
                                  int tracee_pid) ;
void remove_from_tracer_schedule_queue(tracer * tracer_entry, int tracee_pid);
int register_tracer_process(char * write_buffer);
void update_all_children_virtual_time(tracer * tracer_entry, s64 time_increment);
void update_init_task_virtual_time(s64 time_to_set);
void update_all_tracers_virtual_time(int cpuID);
int handle_tracer_results(tracer * curr_tracer, int * api_args, int num_args);
int handle_stop_exp_cmd();
s64 get_dilated_time(struct task_struct * task) ;
s64 handle_gettimepid(char * write_buffer);
void wait_for_task_completion(tracer * curr_tracer, struct task_struct * relevant_task);
void signal_cpu_worker_resume(tracer * curr_tracer);


#endif