#ifndef __UTILITY_FNS_H
#define __UTILITY_FNS_H

#include <stdint.h>
#include <sys/ioctl.h>

#define EXP_CBE 1
#define EXP_CS 2

#define SUCCESS 1
#define FAILURE 0

#define MAX_API_ARGUMENT_SIZE 100
#define BUF_MAX_SIZE MAX_API_ARGUMENT_SIZE

#define NSEC_PER_SEC 1000000000
#define NSEC_PER_MS 1000000
#define NSEC_PER_US 1000

#define LOOKAHEAD_ANCHOR_NONE 0
#define LOOKAHEAD_ANCHOR_CURR_TIME 1
#define LOOKAHEAD_ANCHOR_EAT 2

typedef unsigned long long u64_t;
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
#define VT_GETTIME_TRACER _IOW(VT_IOC_MAGIC, 8, int)
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
#define VT_GETTIME_PID _IOW(VT_IOC_MAGIC, 19, int)
#define VT_SET_PACKET_SEND_TIME _IOW(VT_IOC_MAGIC, 20, int)
#define VT_GET_PACKET_SEND_TIME _IOW(VT_IOC_MAGIC, 21, int)
#define VT_GET_NUM_ENQUEUED_BYTES _IOW(VT_IOC_MAGIC, 22, int)
#define VT_SET_EAT _IOW(VT_IOC_MAGIC, 23, int)
#define VT_GET_EAT _IOW(VT_IOC_MAGIC, 24, int)
#define VT_SET_PROCESS_LOOKAHEAD _IOW(VT_IOC_MAGIC, 25, int)
#define VT_GET_TRACER_LOOKAHEAD _IOW(VT_IOC_MAGIC, 26, int)
#define VT_GET_TRACER_NEAT_LOOKAHEAD _IOW(VT_IOC_MAGIC, 27, int)
#define VT_MARK_STACK_ACTIVE _IOW(VT_IOC_MAGIC, 28, int)
#define VT_MARK_STACK_INACTIVE _IOW(VT_IOC_MAGIC, 29, int)
#define VT_MARK_STACK_RXLOOP_COMPLETE _IOW(VT_IOC_MAGIC, 30, int)
#define VT_THREAD_STACK_WAIT _IOW(VT_IOC_MAGIC, 31, int)
#define VT_THREAD_STACK_FINISH _IOW(VT_IOC_MAGIC, 32, int)
#define VT_UPDATE_STACK_RTX_SEND_TIME _IOW(VT_IOC_MAGIC, 33, int)

#ifndef DISABLE_LOOKAHEAD
struct lookahead_map {
  long start_offset;
  long finish_offset;
  long number_of_values;
  long * lookahead_values;
};

struct lookahead_info {
    const char * mapped_memblock;
    int mapped_memblock_length;
    struct lookahead_map lmap;
};
#endif

//! Sends the command to the virtual time module as an ioctl call
s64 SendToVtModule(unsigned int cmd, ioctl_args* arg);

//! Returns the threadID of the invoking process
int gettid(void);

//! Returns the next first integer value found in a string
int GetNextValue (char *write_buffer);

//! Returns 1 if run with root permissions
int IsRoot(void);

//! Checks if the virtual time manager module is loaded
int IsModuleLoaded(void);

//! Initializes/zeros out an ioct_args structure 
void InitIoctlArg(ioctl_args* arg);

//! Zeros out a string buffer of the specified size
void FlushBuffer(char* buf, int size);

//! Returns number of digits in the integer b
int NumDigits(int n);

//! Converts an integer list of values into a comma separated string and adds
//  them to an ioctl_args structure
int AppendToIoctlArg(ioctl_args* arg, int* append_values, int num_values);

//! Parses an environment variable which is supposed to be a float and sets
//  the return value appropriately. If there is an error, it returns FAILURE
int GetFloatEnvVariable(char * env_variable_name, float * return_value);

//! Parses an environment variable which is supposed to be a integer and sets
//  the return value appropriately. If there is an error, it returns FAILURE
int GetIntEnvVariable(char * env_variable_name, int * return_value);




#endif