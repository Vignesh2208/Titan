#ifndef __UTILITY_FNS_H
#define __UTILITY_FNS_H

#include <stdint.h>
#include <sys/ioctl.h>

#define EXP_CBE 1
#define EXP_CS 2

#define MAX_API_ARGUMENT_SIZE 100
#define BUF_MAX_SIZE MAX_API_ARGUMENT_SIZE

#define MAX_PAYLOAD BUF_MAX_SIZE
#define NETLINK_USER 31
#define IFNAMESIZ 16

#define TRACER_TYPE_INS_VT 1
#define TRACER_TYPE_APP_VT 2

#define SIMPLE_REGISTRATION 0
#define REGISTRATION_W_SPINNER 1
#define REGISTRATION_W_CONTROL_THREAD 2

#define NSEC_PER_SEC 1000000000
#define NSEC_PER_MS 1000000
#define NSEC_PER_US 1000

#define SMALLEST_PROCESS_QUANTA_INSNS 100000

typedef long long s64;

typedef struct ioctl_args_struct {
  char cmd_buf[BUF_MAX_SIZE];
  s64 cmd_value;
} ioctl_args;

#define VT_IOC_MAGIC 'k'

#define VT_UPDATE_TRACER_CLOCK _IOW(VT_IOC_MAGIC, 1, int)
#define VT_WRITE_RESULTS _IOW(VT_IOC_MAGIC, 2, int)
#define VT_GET_CURRENT_VIRTUAL_TIME _IOW(VT_IOC_MAGIC, 3, int)
#define VT_REGISTER_TRACER _IOW(VT_IOC_MAGIC, 4, int)
#define VT_ADD_PROCESSES_TO_SQ _IOW(VT_IOC_MAGIC, 5, int)
#define VT_SYNC_AND_FREEZE _IOW(VT_IOC_MAGIC, 6, int)
#define VT_INITIALIZE_EXP _IOW(VT_IOC_MAGIC, 7, int)
#define VT_GETTIME_PID _IOW(VT_IOC_MAGIC, 8, int)
#define VT_STOP_EXP _IOW(VT_IOC_MAGIC, 9, int)
#define VT_PROGRESS_BY _IOW(VT_IOC_MAGIC, 10, int)
#define VT_PROGRESS_TIMELINE_BY _IOW(VT_IOC_MAGIC, 11, int)
#define VT_WAIT_FOR_EXIT _IOW(VT_IOC_MAGIC, 12, int)
#define VT_SLEEP_FOR _IOW(VT_IOC_MAGIC, 13, int)
#define VT_RELEASE_WORKER _IOW(VT_IOC_MAGIC, 14, int)
#define VT_SET_RUNNABLE _IOW(VT_IOC_MAGIC, 15, int)
#define VT_GETTIME_MY_PID _IOW(VT_IOC_MAGIC, 16, int)
#define VT_ADD_TO_SQ _IOW(VT_IOC_MAGIC, 17, int)
#define VT_SYSCALL_WAIT _IOW(VT_IOC_MAGIC, 18, int)
#define VT_GETTIME_TRACER _IOW(VT_IOC_MAGIC, 19, int)
#define VT_SET_PACKET_SEND_TIME _IOW(VT_IOC_MAGIC, 20, int)
#define VT_GET_PACKET_SEND_TIME _IOW(VT_IOC_MAGIC, 21, int)
#define VT_GET_NUM_ENQUEUED_BYTES _IOW(VT_IOC_MAGIC, 22, int)

s64 send_to_vt_module(unsigned int cmd, ioctl_args* arg);
int gettid(void);
int get_next_value (char *write_buffer);
int is_root(void);
int isModuleLoaded(void);
void init_ioctl_arg(ioctl_args* arg);
void flush_buffer(char* buf, int size);
void init_ioctl_arg(ioctl_args* arg);
int num_characters(int n);
int append_to_ioctl_arg(ioctl_args* arg, int* append_values, int num_values);

#endif
