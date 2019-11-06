#ifndef __SYNC_EXPERIMENT_H
#define __SYNC_EXPERIMENT_H

#include "includes.h"
#include "vt_module.h"

void clean_exp();
int unfreeze_proc_exp_recurse(tracer * curr_tracer);
lxc_schedule_elem * get_next_runnable_task(tracer * curr_tracer);
int update_all_runnable_task_timeslices(tracer * curr_tracer);
void clean_up_all_irrelevant_processes(tracer * curr_tracer) ;
struct task_struct * search_tracer(struct task_struct * aTask, int pid);
int sync_and_freeze();
int cleanup_experiment_components();
int initialize_experiment_components(int num_expected_tracers);
void free_all_tracers();
int progress_by(s64 progress_duration, int num_rounds);

#endif
