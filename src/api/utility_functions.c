#include "utility_functions.h"
#include <fcntl.h>  // for open
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <unistd.h>  // for close

#define NSEC_PER_SEC 1000000000

const char *FILENAME = "/proc/status";
/*
Sends a specific command to the VT Kernel Module.
To communicate with the TLKM, you send messages to the location
specified by FILENAME
*/
s64 send_to_vt_module(unsigned int cmd, ioctl_args *arg) {
  int fp = open(FILENAME, O_RDWR);
  s64 ret = 0;

  if (fp < 0) {
    printf("ERROR communicating with VT module: Could not open proc file\n");
    return -1;
  }

  if (!arg) {
    printf("ERROR communicating with VT module: Missing argument !\n");
    close(fp);
    return -1;
  }
  if (strlen(arg->cmd_buf) > BUF_MAX_SIZE) {
    printf("ERROR communicating with VT module: Too much data to copy !\n");
    close(fp);
    return -1;
  }
  ret = ioctl(fp, cmd, arg);
  if (ret < 0) {
    printf("Error executing cmd: %d\n", cmd);
    close(fp);
    return ret;
  }
  
  close(fp);
  if (cmd == VT_WRITE_RESULTS || cmd == VT_REGISTER_TRACER
    || cmd == VT_SET_RUNNABLE || cmd == VT_GETTIME_MY_PID
    || cmd == VT_GETTIME_PID)
    return arg->cmd_value;
  return ret;
}

void init_ioctl_arg(ioctl_args *arg) {
  if (arg) {
    flush_buffer(arg->cmd_buf, BUF_MAX_SIZE);
    arg->cmd_value = 0;
  }
}

int num_characters(int n) {
  int count = 0;
  if (n < 0) {
    count = 1;
    n = -1 * n;
  }
  while (n != 0) {
    n /= 10;
    ++count;
  }
  return count;
}


int atoi(char *s) {
	int i, n;
	n = 0;
	for (i = 0; * (s + i) >= '0' && *(s + i) <= '9'; i++)
		n = 10 * n + *(s + i) - '0';
	return n;
}


int append_to_ioctl_arg(ioctl_args *arg, int *append_values, int num_values) {
  if (!arg) return -1;

  for (int i = 0; i < num_values; i++) {
    if (strlen(arg->cmd_buf) + num_characters(append_values[i]) + 1 >=
        BUF_MAX_SIZE)
      return -1;

    if (i < num_values - 1)
      sprintf(arg->cmd_buf + strlen(arg->cmd_buf), "%d,", append_values[i]);
    else {
      sprintf(arg->cmd_buf + strlen(arg->cmd_buf), "%d", append_values[i]);
    }
  }
  return 0;
}
/*
Returns the thread id of a process
*/
int gettid() { return syscall(SYS_gettid); }

/*
Checks if it is being ran as root or not.
*/
int is_root() {
  if (geteuid() == 0) return 1;
  return 0;
}

/*
Returns 1 if module is loaded, 0 otherwise
*/
int isModuleLoaded() {
  if (access(FILENAME, F_OK) != -1) {
    return 1;
  } else {
    printf("VT kernel module is not loaded\n");
    return 0;
  }
}

void flush_buffer(char *buf, int size) {
  if (size) memset(buf, 0, size * sizeof(char));
}


void ns_to_timespec(const s64 nsec, struct timespec * ts) {

	int32_t rem;

	if (!nsec)
		return (struct timespec) {0, 0};

	ts->tv_sec = div_s64_rem(nsec, NSEC_PER_SEC, &rem);
	if (unlikely(rem < 0)) {
		ts->tv_sec--;
		rem += NSEC_PER_SEC;
	}
	ts->tv_nsec = rem;
}


void ns_to_timeval(const s64 nsec, struct timeval * tv) {
	struct timespec ts;
	
  ns_to_timespec(nsec, &ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = (suseconds_t) ts.tv_nsec / 1000;

	return;
}