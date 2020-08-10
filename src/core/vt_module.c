#include "vt_module.h"
#include "dilated_timer.h"
#include "common.h"
#include "sync_experiment.h"
#include "general_commands.h"

/**
Has basic functionality for the Kernel Module itself. It defines how the
userland process communicates with the kernel module,
as well as what should happen when the kernel module is initialized and removed.
**/

/** LOCAL DECLARATIONS **/
int tracer_num;
int total_num_timelines;
int EXP_CPUS;
int TOTAL_CPUS;
int experiment_status;
int experiment_type;
int initialization_status;
s64 expected_time;
s64 virt_exp_start_time;
s64 current_progress_duration;
int current_progress_n_rounds;
int total_expected_tracers;
int total_completed_rounds;

// locks
struct mutex exp_lock;
struct mutex file_lock;

// pointers
int *per_timeline_chain_length;
llist *per_timeline_tracer_list;
wait_queue_head_t * timeline_wqueue;
wait_queue_head_t * timeline_worker_wqueue;
wait_queue_head_t * syscall_wait_wqueue;
static struct proc_dir_entry *dilation_dir;
static struct proc_dir_entry *dilation_file;
timeline * timeline_info;
struct task_struct *loop_task;
struct task_struct ** chaintask;
struct dilated_timer_timeline_base ** global_dilated_timer_timeline_bases;

// hashmaps
hashmap get_tracer_by_id;
hashmap get_tracer_by_pid;
hashmap get_dilated_task_struct_by_pid;

// atomic variables
atomic_t n_workers_running = ATOMIC_INIT(0);
atomic_t progress_n_enabled = ATOMIC_INIT(0);
atomic_t experiment_stopping = ATOMIC_INIT(0);
atomic_t n_waiting_tracers = ATOMIC_INIT(0);




loff_t vt_llseek(struct file *filp, loff_t pos, int whence);
int vt_release(struct inode *inode, struct file *filp);
int vt_mmap(struct file *filp, struct vm_area_struct *vma);
long vt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
ssize_t vt_write(struct file *file, const char __user *buffer, size_t count, loff_t *data);


static const struct file_operations proc_file_fops = {
    .unlocked_ioctl = vt_ioctl,
    .llseek = vt_llseek,
    .release = vt_release,
    .write = vt_write,
    .owner = THIS_MODULE,
};

/***
Gets the PID of our synchronizer spinner task (only in 64 bit)
***/
int GetSpinnerPid(struct subprocess_info *info, struct cred *new) {
  loop_task = current;
  printk(KERN_INFO "Titan: Loop Task Started. Pid: %d\n", current->pid);
  return loop_task->pid;
}


/***
Hack to get 64 bit running correctly. Starts a process that will just loop
while the experiment is going on. Starts an executable specified in the path
in user space from kernel space.
***/
int RunUserModeSynchronizerProcess(char *path, char **argv, char **envp,
                                   int wait) {
  struct subprocess_info *info;
  gfp_t gfp_mask = (wait == UMH_NO_WAIT) ? GFP_ATOMIC : GFP_KERNEL;

  info = call_usermodehelper_setup(path, argv, envp, gfp_mask, GetSpinnerPid,
                                   NULL, NULL);
  if (info == NULL) return -ENOMEM;
  return call_usermodehelper_exec(info, wait);
}


loff_t vt_llseek(struct file *filp, loff_t pos, int whence) { return 0; }

int vt_release(struct inode *inode, struct file *filp) { return 0; }


ssize_t vt_write(struct file *file, const char __user *buffer, size_t count,
                 loff_t *data) {
	return count;
}

long vt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

  int err;
  struct dilated_task_struct * dilated_task = NULL;
  
  PDEBUG_V("VT-IO: Got ioctl from : %d, Cmd = %u\n",
          current->pid, cmd);

  /*
   * extract the type and number bitfields, and don't decode
   * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
   */
  if (_IOC_TYPE(cmd) != VT_IOC_MAGIC) {
    PDEBUG_V("VT-IO: Error ENOTTY\n");
    return -ENOTTY;
  }

  /*
   * the direction is a bitmask, and VERIFY_WRITE catches R/W
   * transfers. `Type' is user-oriented, while
   * access_ok is kernel-oriented, so the concept of "read" and
   * "write" is reversed
   */
  if (_IOC_DIR(cmd) & _IOC_READ)
    err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
  else if (_IOC_DIR(cmd) & _IOC_WRITE)
    err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));

  if (err) {
    PDEBUG_V("VT-IO: Error Access\n");
    return -EFAULT;
  }

  if (cmd != VT_INITIALIZE_EXP) {
    dilated_task = hmap_get_abs(&get_dilated_task_struct_by_pid,
                                (int)current->pid);
  } else {
    dilated_task = NULL;
  }

  switch (cmd) {
	
    case VT_UPDATE_TRACER_CLOCK:
      return HandleVtUpdateTracerClock(arg, dilated_task);
	
    case VT_WRITE_RESULTS:
      return HandleVtWriteResults(arg, dilated_task);

    case VT_REGISTER_TRACER:
      return HandleVtRegisterTracer(arg, dilated_task);

    case VT_ADD_PROCESSES_TO_SQ:
      return HandleVtAddProcessesToSchQueue(arg, dilated_task);

    case VT_SYNC_AND_FREEZE:
      return HandleVtSyncFreeze();
      
    case VT_INITIALIZE_EXP:
      return HandleVtInitializeExp(arg);

    case VT_GETTIME_PID:
      return HandleVtGettimePid(arg);

    case VT_GETTIME_TRACER:
      return HandleVtGettimeTracer(arg, dilated_task);
      
    case VT_STOP_EXP:
      return HandleVtStopExp();
      
    case VT_PROGRESS_BY:
      return HandleVtProgressBy(arg);
      
    case VT_PROGRESS_TIMELINE_BY:
      return HandleVtProgressTimelineBy(arg);
      
    case VT_WAIT_FOR_EXIT:
      return HandleVtWaitForExit(arg, dilated_task);

    case VT_SLEEP_FOR:
      return HandleVtSleepFor(arg, dilated_task);

    case VT_RELEASE_WORKER:
      return HandleVtReleaseWorker(arg, dilated_task);

    case VT_SET_RUNNABLE:
      return HandleVtSetRunnable(arg, dilated_task);

    case VT_GETTIME_MY_PID:
      return HandleVtGettimeMyPid(arg);

    case VT_ADD_TO_SQ:
      return HandleVtAddToSchQueue(arg);

    case VT_SYSCALL_WAIT:
      return HandleVtSyscallWait(arg, dilated_task);

    case VT_SET_PACKET_SEND_TIME:
      return HandleVtSetPacketSendTime(arg, dilated_task);
	
    case VT_GET_PACKET_SEND_TIME:
      return HandleVtGetPacketSendTime(arg);

    case VT_GET_NUM_ENQUEUED_BYTES:
      return HandleVtGetNumQueuedBytes(arg);
	
    case VT_SET_EAT:
      return HandleVtSetEat(arg);

    case VT_GET_EAT:
      return HandleVtGetEat(arg, dilated_task);

    case VT_SET_PROCESS_LOOKAHEAD:
      return HandleVtSetProcessLookahead(arg, dilated_task);

    case VT_GET_TRACER_LOOKAHEAD:
      return HandleVtGetTracerLookahead(arg);

	
    default:
      PDEBUG_V("Unkown Command !\n");
      return -ENOTTY;
  }

  return 0;
}

/***
This function gets executed when the kernel module is loaded.
It creates the file for process -> kernel module communication,
sets up mutexes, timers, and hooks the system call table.
***/
int __init my_module_init(void) {
  int i;

  PDEBUG_A("Loading MODULE\n");

  experiment_type = NOT_SET;
  initialization_status = NOT_INITIALIZED;
  experiment_status = NOTRUNNING;
  tracer_num = 0;

  /* Set up Kronos status file in /proc */
  dilation_dir = proc_mkdir_mode(DILATION_DIR, 0555, NULL);
  if (dilation_dir == NULL) {
    remove_proc_entry(DILATION_DIR, NULL);
    PDEBUG_E(" Error: Could not initialize /proc/%s\n", DILATION_DIR);
    return -ENOMEM;
  }

  PDEBUG_A(" /proc/%s created\n", DILATION_DIR);
  dilation_file = proc_create(DILATION_FILE, 0666, NULL, &proc_file_fops);

  mutex_init(&file_lock);

  if (dilation_file == NULL) {
    remove_proc_entry(DILATION_FILE, dilation_dir);
    PDEBUG_E("Error: Could not initialize /proc/%s/%s\n", DILATION_DIR,
             DILATION_FILE);
    return -ENOMEM;
  }
  PDEBUG_A(" /proc/%s/%s created\n", DILATION_DIR, DILATION_FILE);

  /* If it is 64-bit, initialize the looping script */
#ifdef __x86_64
  char *argv[] = {"/bin/x64_synchronizer", NULL};
  static char *envp[] = {"HOME=/", "TERM=linux",
                         "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};
  RunUserModeSynchronizerProcess(argv[0], argv, envp, UMH_NO_WAIT);
#endif

  initialization_status = NOT_INITIALIZED;
  experiment_status = NOTRUNNING;
  experiment_type = NOT_SET;

  /* Acquire number of CPUs on system */
  TOTAL_CPUS = num_online_cpus();
  PDEBUG_A("Total Number of CPUS: %d\n", num_online_cpus());

  if (TOTAL_CPUS > 2)
    EXP_CPUS = TOTAL_CPUS - 2;
  else
    EXP_CPUS = 1;


  PDEBUG_A("Number of EXP_CPUS: %d\n", EXP_CPUS);
  PDEBUG_A("MODULE LOADED SUCCESSFULLY \n");
  return 0;
}

/***
This function gets called when the kernel module is unloaded.
It frees up all memory, deletes timers, and fixes
the system call table.
***/
void __exit my_module_exit(void) {
  s64 i;
  int num_prev_alotted_pages;

  remove_proc_entry(DILATION_FILE, NULL);
  PDEBUG_A(" /proc/%s deleted\n", DILATION_FILE);
  remove_proc_entry(DILATION_DIR, NULL);
  PDEBUG_A(" /proc/%s deleted\n", DILATION_DIR);

  if (tracer_num > 0) {
    // Free all tracer structs from previous experiment if any
    FreeAllTracers();
    tracer_num = 0;
  }

  /* Kill the looping task */
#ifdef __x86_64
  if (loop_task != NULL) SendSignalToProcess(loop_task, SIGKILL);
#endif
  PDEBUG_A("MODULE UNLOADED\n");
}

/* Register the init and exit functions here so insmod can run them */
module_init(my_module_init);
module_exit(my_module_exit);

/* Required by kernel */
MODULE_LICENSE("GPL");
