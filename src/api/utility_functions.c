#include "utility_functions.h"
#include <fcntl.h>  // for open
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#include <unistd.h>  // for close

const char *FILENAME = "/proc/status";
/*
Sends a specific command to the VT Kernel Module.
To communicate with the TLKM, you send messages to the location
specified by FILENAME
*/
s64 send_to_vt_module(unsigned int cmd, ioctl_args *arg) {
  int fp = open(FILENAME, O_RDWR);
  int ret = 0;
  struct timeval vt;
  if (fp < 0) {
    printf("ERROR communicating with VT module: Could not open proc file\n");
    return -1;
  }

  if (cmd != VT_GET_CURRENT_VIRTUAL_TIME) {
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
  } else {
    ret = ioctl(fp, VT_GET_CURRENT_VIRTUAL_TIME, (struct timeval *)&vt);
    if (ret < 0) {
      printf("Error executing VT_GET_CURRENT_VIRTUAL_TIME cmd\n");
      close(fp);
      return ret;
    }
    close(fp);
    return (vt.tv_sec * 1000000 + vt.tv_usec) * 1000;
  }
  close(fp);
  if (cmd == VT_WRITE_RESULTS || cmd == VT_REGISTER_TRACER)
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
