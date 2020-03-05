#ifndef __VT_UTILS_H
#define __VT_UTILS_H

#include <linux/string.h>
#include "vt_module.h"


char * alloc_mmap_pages(int npages);
void free_mmap_pages(void *mem, int npages);
int get_next_value (char *write_buffer);
int atoi(char *s);
struct task_struct* find_task_by_pid(unsigned int nr);
void initialize_tracer_entry(tracer * new_tracer, uint32_t tracer_id);
tracer * alloc_tracer_entry(uint32_t tracer_id);
void free_tracer_entry(tracer * tracer_entry);
void set_children_cpu(struct task_struct *aTask, int cpu);
void set_children_time(tracer * tracer_entry, s64 time, int increment);
void print_schedule_list(tracer* tracer_entry);
int kill_p(struct task_struct *killTask, int sig);
//tracer * get_tracer_for_task(struct task_struct * aTask);
void get_tracer_struct_read(tracer* tracer_entry);
void put_tracer_struct_read(tracer* tracer_entry);
void get_tracer_struct_write(tracer* tracer_entry);
void put_tracer_struct_write(tracer* tracer_entry);

//struct dilated_task_struct * get_dilated_task_struct(
//	struct task_struct * task);

//struct dilated_task_struct * find_dilated_task_by_pid(int pid);

int convert_string_to_array(char * str, int * arr, int arr_size);
void bind_to_cpu(struct task_struct * task, int target_cpu);

#endif
