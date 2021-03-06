#include "utility_functions.h"
#include <fcntl.h>  // for open
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>


const char *FILENAME = "/proc/titan";
/*
Sends a specific command to the VT Kernel Module.
To communicate with the TLKM, you send messages to the location
specified by FILENAME
*/
s64 SendToVtModule(unsigned int cmd, ioctl_args *arg) {
  int fp = open(FILENAME, O_RDWR);
  s64 ret = 10000;
 
  if (fp < 0) {
    //printf("ERROR communicating with VT module: Could not open proc file: %s\n", FILENAME);
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
    //printf("Error executing cmd: %d\n", cmd);
    close(fp);
    return ret;
  }
  
  close(fp);
  if (cmd == VT_WRITE_RESULTS || cmd == VT_REGISTER_TRACER
    || cmd == VT_SET_RUNNABLE || cmd == VT_GETTIME_MY_PID
    || cmd == VT_GETTIME_PID || cmd == VT_GETTIME_TRACER 
    || cmd == VT_GET_PACKET_SEND_TIME || cmd == VT_SYSCALL_WAIT
    || cmd == VT_GET_NUM_ENQUEUED_BYTES || cmd == VT_GET_EAT
    || cmd == VT_GET_TRACER_LOOKAHEAD
    || cmd == VT_GET_TRACER_NEAT_LOOKAHEAD)
    return arg->cmd_value;

  if (cmd == VT_THREAD_STACK_WAIT) {
    if (ret < 0)
      return ret;
    return arg->cmd_value;
  }
  return ret;
}

void InitIoctlArg(ioctl_args *arg) {
  if (arg) {
    FlushBuffer(arg->cmd_buf, BUF_MAX_SIZE);
    arg->cmd_value = 0;
  }
}

int NumDigits(int n) {
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


int AppendToIoctlArg(ioctl_args *arg, int *append_values, int num_values) {
  if (!arg) return -1;

  for (int i = 0; i < num_values; i++) {
    if (strlen(arg->cmd_buf) + NumDigits(append_values[i]) + 1 >=
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
int IsRoot() {
  if (geteuid() == 0) return 1;
  return 0;
}

/*
Returns 1 if module is loaded, 0 otherwise
*/
int IsModuleLoaded() {
  if (access(FILENAME, F_OK) != -1) {
    return 1;
  } else {
    printf("VT kernel module is not loaded\n");
    return 0;
  }
}

//! Parses an environment variable which is supposed to be a float and sets
//  the return value appropriately. If there is an error, it returns FAILURE
int GetFloatEnvVariable(char * env_variable_name, float * return_value) {
  char * value_string = getenv(env_variable_name);
  if (!value_string)
    return FAILURE;
  
  if (!return_value)
    return FAILURE;

  *return_value = strtof(value_string, NULL);
  return SUCCESS; 

}

//! Parses an environment variable which is supposed to be a integer and sets
//  the return value appropriately. If there is an error, it returns FAILURE
int GetIntEnvVariable(char * env_variable_name, int * return_value) {
  char * value_string = getenv(env_variable_name);
  if (!value_string)
    return FAILURE;
  
  if (!return_value)
    return FAILURE;

  *return_value = atoi(value_string);
  return SUCCESS; 
}

void FlushBuffer(char *buf, int size) {
  if (size) memset(buf, 0, size * sizeof(char));
}


void ns_2_timespec(s64 nsec, struct timespec * ts) {

  int32_t rem;

  if (!nsec) {
    ts->tv_sec = 0, ts->tv_nsec = 0;
    return;
  }

  ts->tv_sec = nsec / NSEC_PER_SEC;
  rem = nsec - ts->tv_sec * NSEC_PER_SEC;
  ts->tv_nsec = rem;
}


void ns_2_timeval(s64 nsec, struct timeval * tv) {
  struct timespec ts;
  
  ns_2_timespec(nsec, &ts);
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;

  return;
}
