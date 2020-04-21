#ifndef __GENERAL_COMMANDS_H

#define __GENERAL_COMMANDS_H

#include "vt_module.h"

int handle_vt_update_tracer_clock(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_write_results(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_register_tracer(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_add_processes_to_sq(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_sync_freeze();
int handle_vt_initialize_exp(unsigned long arg);
int handle_vt_gettimepid(unsigned long arg);
int handle_vt_gettime_tracer(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_stop_exp();
int handle_vt_progress_by(unsigned long arg);
int handle_vt_progress_timeline_by(unsigned long arg);
int handle_vt_wait_for_exit(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_sleep_for(unsigned long arg, struct dilated_task_struct * dilated_task); 
int handle_vt_release_worker(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_set_runnable(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_gettime_my_pid(unsigned long arg);
int handle_vt_add_to_sq(unsigned long arg);
int handle_vt_syscall_wait(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_set_packet_send_time(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_get_packet_send_time(unsigned long arg);
int handle_vt_get_num_queued_bytes(unsigned long arg);
int handle_vt_set_eat(unsigned long arg);
int handle_vt_get_eat(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_set_process_lookahead(unsigned long arg, struct dilated_task_struct * dilated_task);
int handle_vt_get_tracer_lookahead(unsigned long arg);


#endif

