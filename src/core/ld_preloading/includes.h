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


typedef long long s64;

#define MAX_NUM_FDS_PER_PROCESS 1024

#endif
