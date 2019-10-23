#ifndef __UTILS_H
#define __UTILS_H

#include "includes.h"

int run_command(char * full_command_str, pid_t * child_pid);
void print_tracee_list(llist * tracee_list);
int create_spinner_task(pid_t * child_pid);

#endif
