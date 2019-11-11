#ifndef __VT_FUNCTIONS
#define __VT_FUNCTIONS
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utility_functions.h"

typedef unsigned long u32;

// These functions can only be called from inside a tracer process or its
// children
s64 register_tracer(int tracer_id, int tracer_type, int registration_type,
                    int optional_pid);
int update_tracer_clock(int tracer_id, s64 increment);
s64 write_tracer_results(int tracer_id, int* results, int num_results);
int add_processes_to_tracer_sq(int tracer_id, int* pids, int num_pids);

// These functions can be called by the orchestrater script which may be in c,
// c++ or python
s64 get_current_virtual_time(void);
s64 get_current_time_pid(int pid);
int initializeExp(int num_expected_tracers);
int synchronizeAndFreeze(void);
int stopExp(void);
int progressBy(s64 duration, int num_rounds);
int set_netdevice_owner(int tracer_id, char* intf_name);
int wait_for_exit(int tracer_id);

#endif
