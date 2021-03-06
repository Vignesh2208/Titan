#ifndef __INCLUDES_H
#define __INCLUDES_H


#define _GNU_SOURCE

#include <libsyscall_intercept_hook_point.h>
#include <assert.h>
#include <syscall.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>
#include <sys/select.h>
#include <linux/futex.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <sys/timerfd.h>

#define LOOKAHEAD_ANCHOR_NONE 0
#define LOOKAHEAD_ANCHOR_CURR_TIME 1
#define LOOKAHEAD_ANCHOR_EAT 2

#define FD_TYPE_DEFAULT 0x0
#define FD_TYPE_SOCKET 0x1
#define FD_TYPE_TIMERFD 0x2
#define MAX_ASSIGNABLE_FD 1024

#define FD_PROTO_TYPE_TCP 0x1
#define FD_PROTO_TYPE_OTHER 0x2
#define FD_PROTO_TYPE_NONE 0

typedef long long s64;
typedef unsigned long long u64;

#define MAX_NUM_FDS_PER_PROCESS 1024

#endif
