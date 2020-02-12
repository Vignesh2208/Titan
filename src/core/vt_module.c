#include "vt_module.h"
#include "common.h"
#include "sync_experiment.h"

/*
Has basic functionality for the Kernel Module itself. It defines how the
userland process communicates with the kernel module,
as well as what should happen when the kernel module is initialized and removed.
*/

/** LOCAL DECLARATIONS **/
int tracer_num = 0;
int EXP_CPUS = 0;
int TOTAL_CPUS = 0;
int experiment_status;
int initialization_status;
s64 expected_time = 0;
s64 virt_exp_start_time = 0;
s64 current_progress_duration = 0;
int current_progress_n_rounds = 0;
int total_expected_tracers;
// locks
struct mutex exp_lock;
struct mutex file_lock;

// pointers
int *per_cpu_chain_length;
llist *per_cpu_tracer_list;
wait_queue_head_t *tracer_wqueue;
s64 *tracer_clock_array = NULL;
s64 *aligned_tracer_clock_array = NULL;
static struct proc_dir_entry *dilation_dir = NULL;
static struct proc_dir_entry *dilation_file = NULL;
struct task_struct *loop_task;
struct task_struct *round_task = NULL;
struct task_struct ** chaintask;

// hashmaps
hashmap get_tracer_by_id;
hashmap get_tracer_by_pid;

// atomic variables
atomic_t n_workers_running = ATOMIC_INIT(0);
atomic_t progress_n_enabled = ATOMIC_INIT(0);
atomic_t experiment_stopping = ATOMIC_INIT(0);
atomic_t n_waiting_tracers = ATOMIC_INIT(0);

/** EXTERN DECLARATIONS **/

extern wait_queue_head_t progress_sync_proc_wqueue;
extern int round_sync_task(void *data);

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
int getSpinnerPid(struct subprocess_info *info, struct cred *new) {
  loop_task = current;
  printk(KERN_INFO "Titan: Loop Task Started. Pid: %d\n", current->pid);
  return 0;
}


/***
Hack to get 64 bit running correctly. Starts a process that will just loop
while the experiment is going on. Starts an executable specified in the path
in user space from kernel space.
***/
int run_usermode_synchronizer_process(char *path, char **argv, char **envp,
                                      int wait) {
  struct subprocess_info *info;
  gfp_t gfp_mask = (wait == UMH_NO_WAIT) ? GFP_ATOMIC : GFP_KERNEL;

  info = call_usermodehelper_setup(path, argv, envp, gfp_mask, getSpinnerPid,
                                   NULL, NULL);
  if (info == NULL) return -ENOMEM;
  return call_usermodehelper_exec(info, wait);
}


loff_t vt_llseek(struct file *filp, loff_t pos, int whence) { return 0; }

int vt_release(struct inode *inode, struct file *filp) { return 0; }


void handle_add_processes_to_sq(int * api_args, int num_args) {
  int tracer_id;
	tracer *curr_tracer;

  tracer_id = api_args[0];
  curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
  if (!curr_tracer) {
    PDEBUG_I("VT_ADD_PROCESS_TO_SQ: Tracer : %d, not registered\n", tracer_id);
    return;
  }

  get_tracer_struct_write(curr_tracer);
  for (i = 1; i < num_args; i++) {
    add_to_tracer_schedule_queue(curr_tracer, api_args[i]);
    PDEBUG_I("VT_ADD_PROCESS_TO_SQ: Tracer : %d, process: %d\n",
             tracer_id, api_args[i]);
  }
  put_tracer_struct_write(curr_tracer);
}

ssize_t handle_write_results_cmd(struct dilation_task_struct * dilation_task,
                                 int *api_args, int num_args) {

  int tracer_id;
	tracer *curr_tracer;

  tracer_id = api_args[0];
  curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
  if (!curr_tracer) {
    PDEBUG_I("VT_WRITE_RES: Tracer : %d, not registered\n", tracer_id);
    return 0;
  } 

  dilation_task->ready = 1; 
  handle_tracer_results(curr_tracer, &api_args[1],
                        num_args - 1);

  wait_event_interruptible(
    *curr_tracer->w_queue,
    dilation_task->associated_tracer_id <= 0 ||
        (curr_tracer->w_queue_wakeup_pid == current->pid &&
         dilation_task->burst_target > 0));
  PDEBUG_V(
    "VT_WRITE_RES: Associated Tracer : %d, Process: %d, resuming from wait\n",
    tracer_id, current->pid);

  dilation_task->ready = 0;

  // Ensure that for INS_VT tracer, only the tracer process can invoke this
  // call
  if (dilation_task->associated_tracer_id)
    BUG_ON(curr_tracer->main_task->pid != current->pid);

  if (dilation_task->associated_tracer_id <= 0) {
    dilation_task->burst_target = 0;
    dilation_task->virt_start_time = 0;
    dilation_task->curr_virt_time = 0;
    dilation_task->associated_tracer_id = 0;
    dilation_task->wakeup_time = 0;
    dilation_task->vt_exec_task = NULL;
    dilation_task->base_task = NULL;
    PDEBUG_I("VT_WRITE_RESULTS: Tracer: %d, Process: %d STOPPING\n", tracer_id,
        current->pid);
    return 0;
  }

  PDEBUG_V("VT_WRITE_RESULTS: Tracer: %d, Process: %d Returning !\n", tracer_id,
          current->pid);
  return (ssize_t) dilation_task->burst_target;
}

ssize_t vt_write(struct file *file, const char __user *buffer, size_t count,
                 loff_t *data) {
	char write_buffer[MAX_API_ARGUMENT_SIZE];
	unsigned long buffer_size;
	int i = 0;
	int ret = 0;
	int num_integer_args;
  int api_integer_args[MAX_API_ARGUMENT_SIZE];
  struct dilated_task_struct * dilated_task = get_dilated_task_struct(current);
	

 	if(count > MAX_API_ARGUMENT_SIZE) {
    buffer_size = MAX_API_ARGUMENT_SIZE;
  } else {
		buffer_size = count;
	}

	for(i = 0; i < MAX_API_ARGUMENT_SIZE; i++)
		write_buffer[i] = '\0';

  if(copy_from_user(write_buffer, buffer, buffer_size)){
	    return -EFAULT;
	}

	/* Use +2 to skip over the first two characters (i.e. the switch and the ,) */
	if (write_buffer[0] == VT_ADD_TO_SQ) {
      if (initialization_status != INITIALIZED && 
          experiment_status == STOPPING) {
			PDEBUG_I("VT_ADD_PROCESSES_TO_SQ: Operation cannot be performed when "
			    "experiment is not initialized !");
			return -EFAULT;
		}

		num_integer_args = convert_string_to_array(
		  write_buffer + 2, api_integer_args, MAX_API_ARGUMENT_SIZE);

		if (num_integer_args <= 1) {
			PDEBUG_I("VT_ADD_PROCESS_TO_SQ: Not enough arguments !");
			return -EFAULT;
		}

    handle_add_processes_to_sq(api_integer_args, num_integer_args);
		return 0;

	} else if (write_buffer[0] == VT_WRITE_RES) {

		if (initialization_status != INITIALIZED) {
			PDEBUG_I("VT_WRITE_RESULTS: Operation cannot be performed when experiment "
			    "is not initialized !");
			return 0;
		}

		if (!dilated_task || dilated_task->associated_tracer_id <= 0) {
			PDEBUG_I("VT_WRITE_RESULTS: Process: %d is not associated with any "
               "tracer !\n", current->pid);
			return 0;
		}

		num_integer_args = convert_string_to_array(
		  write_buffer + 2, api_integer_args, MAX_API_ARGUMENT_SIZE);

		if (num_integer_args < 1) {
			PDEBUG_I("VT_WRITE_RESULTS: Not enough arguments !");
			return 0;
		}

    return handle_write_results_cmd(dilated_task, api_integer_args,
                                    num_integer_args);

	}
	else {
		PDEBUG_I("VT Module Write: Invalid Write Command: %s\n", write_buffer);
		return -EFAULT;
	}
	return 0;
}

long vt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
  int err = 0;
  int retval = 0;
  int i = 0, cpu_assignment;
  uint8_t mask = 0;
  int num_rounds = 0, timeline_id;
  unsigned long flags;
  overshoot_info *args;
  invoked_api *api_info;
  struct timeval *tv;
  overshoot_info tmp;
  invoked_api api_info_tmp;
  char *ptr;
  int ret, num_integer_args;
  int api_integer_args[MAX_API_ARGUMENT_SIZE];
  tracer *curr_tracer;
  int tracer_id;
  struct dilated_task_struct * dilated_task = get_dilated_task_struct(current);
	
  memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
  memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);

  PDEBUG_V("VT-IO: Got ioctl from : %d\n", current->pid);

  /*
   * extract the type and number bitfields, and don't decode
   * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
   */
  if (_IOC_TYPE(cmd) != VT_IOC_MAGIC) return -ENOTTY;

  /*
   * the direction is a bitmask, and VERIFY_WRITE catches R/W
   * transfers. `Type' is user-oriented, while
   * access_ok is kernel-oriented, so the concept of "read" and
   * "write" is reversed
   */
  if (_IOC_DIR(cmd) & _IOC_READ)
    err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
  else if (_IOC_DIR(cmd) & _IOC_WRITE)
    err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));

  if (err) return -EFAULT;

  switch (cmd) {
    case VT_UPDATE_TRACER_CLOCK:
      // Only registered tracer process can invoke this ioctl

      if (initialization_status != INITIALIZED ||
          experiment_status == NOTRUNNING) {
        PDEBUG_I(
            "VT_UPDATE_TRACER_CLOCK: Operation cannot be performed when "
            "experiment is not running !\n");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }

      num_integer_args = convert_string_to_array(
          api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

      if (num_integer_args != 1) {
        PDEBUG_I("VT_UPDATE_TRACER_CLOCK: Not enough arguments. "
                 "Missing tracer_id !\n");
        return -EFAULT;
      }

      if (!dilated_task || dilated_task->associated_tracer_id <= 0)
        return 0;

      tracer_id = api_integer_args[0];
      curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
      if (!curr_tracer) {
        PDEBUG_I("VT-IO: Tracer : %d, not registered\n", tracer_id);
        return -EFAULT;
      }

      curr_tracer->curr_virtual_time += api_info_tmp.return_value;
      set_children_time(curr_tracer, curr_tracer->curr_virtual_time, 0);
      return 0;

    case VT_WRITE_RESULTS:
      // A registered tracer process or one of the processes in a tracer's
      // schedule queue can invoke this ioctl For INSVT tracer type, only the
      // tracer process can invoke this ioctl

      if (initialization_status != INITIALIZED) {
        PDEBUG_I("VT_WRITE_RESULTS: Operation cannot be performed when "
                 "experiment is not initialized !\n");
        return -EFAULT;
      }

      if (!dilated_task || dilated_task->associated_tracer_id <= 0) {
        PDEBUG_E("VT_WRITE_RESULTS: Process is not associated with "
                 "any tracer !\n");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }

      num_integer_args = convert_string_to_array(
          api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

      if (num_integer_args < 1) {
        PDEBUG_I("VT_WRITE_RESULTS: Not enough arguments !");
        return -EFAULT;
      }

      tracer_id = api_integer_args[0];
      curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
      if (!curr_tracer) {
        PDEBUG_I("VT_WRITE_RESULTS: Tracer : %d, not registered\n", current->pid);
        return -EFAULT;
      }

      handle_write_results_cmd(dilated_task, api_integer_args,
                               num_integer_args);

      if (dilated_task->associated_tracer_id <= 0) {
        api_info_tmp.return_value = 0;
        if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
          PDEBUG_I("VT_WRITE_RESULTS: Tracer : %d, Process: %d "
                   "Resuming from wait. Error copying to user buf\n",
                   tracer_id, current->pid);
          return -EFAULT;
        }
        PDEBUG_I("VT_WRITE_RESULTS: Tracer: %d, Process: %d STOPPING\n",
                 tracer_id, current->pid);
        return 0;
      }

      api_info_tmp.return_value = dilated_task->burst_target;
      if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
        PDEBUG_I("VT_WRITE_RESULTS: Tracer : %d, Process: %d "
                 "Resuming from wait. Error copying to user buf\n",
                 tracer_id, current->pid);
        return -EFAULT;
      }

      PDEBUG_V("VT_WRITE_RESULTS: Tracer: %d, Process: %d Returning !\n",
               tracer_id, current->pid);
      return 0;

    case VT_REGISTER_TRACER:
      // Any process can invoke this call if it is not already associated with a
      // tracer

      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_REGISTER_TRACER: Operation cannot be performed when experiment "
            "is not initialized !");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }

      if (dilated_task != NULL && dilated_task->associated_tracer_id) {
        PDEBUG_I("VT_REGISTER_TRACER: Process: %d, already associated with "
                 "another tracer. It cannot be registered as a new tracer !",
                 current->pid);
        return -EFAULT;
      }

      retval = register_tracer_process(api_info_tmp.api_argument);
      if (retval == FAIL) {
        PDEBUG_I("VT_REGISTER_TRACER: Tracer Registration failed for \n");
        return -EFAULT;
      }
      api_info_tmp.return_value = retval;

      if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
        PDEBUG_I("VT_REGISTER_TRACER: Tracer : %d, Error copying to user buf\n",
                 current->pid);
        return -EFAULT;
      }
      atomic_inc(&n_waiting_tracers);
      wake_up_interruptible(&progress_sync_proc_wqueue);
      return 0;

    case VT_ADD_PROCESSES_TO_SQ:
      // Any process can invoke this call.

      if (initialization_status != INITIALIZED &&
          experiment_status == STOPPING) {
        PDEBUG_I(
            "VT_ADD_PROCESSES_TO_SQ: Operation cannot be performed when "
            "experiment is not initialized !");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }
      num_integer_args = convert_string_to_array(
          api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

      if (num_integer_args <= 1) {
        PDEBUG_I("VT_ADD_PROCESS_TO_SQ: Not enough arguments !");
        return -EFAULT;
      }

      tracer_id = api_integer_args[0];
      curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
      if (!curr_tracer) {
        PDEBUG_I("VT_ADD_PROCESS_TO_SQ: Tracer : %d, not registered\n",
                 tracer_id);
        return -EFAULT;
      }

      get_tracer_struct_write(curr_tracer);
      for (i = 1; i < num_integer_args; i++) {
        add_to_tracer_schedule_queue(curr_tracer, api_integer_args[i]);
      }
      put_tracer_struct_write(curr_tracer);
      return 0;

    case VT_SYNC_AND_FREEZE:
      // Any process can invoke this call.
      if (initialization_status != INITIALIZED) {
        PDEBUG_E(
            "VT_SYNC_AND_FREEZE: Operation cannot be performed when experiment "
            "is not initialized !");
        return -EFAULT;
      }

      if (experiment_status != NOTRUNNING) {
        PDEBUG_E(
            "VT_SYNC_AND_FREEZE: Operation cannot be performed when experiment "
            "is already running!");
        return -EFAULT;
      }

      return sync_and_freeze();

    case VT_INITIALIZE_EXP:
      // Any process can invoke this call.

      if (initialization_status == INITIALIZED) {
        PDEBUG_E(
            "VT_INITIALIZE_EXP: Operation cannot be performed when experiment "
            "is already initialized !");
        return -EFAULT;
      }

      if (experiment_status != NOTRUNNING) {
        PDEBUG_E(
            "VT_INITIALIZE_EXP: Operation cannot be performed when experiment "
            "is already running!");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }
      return handle_initialize_exp_cmd(api_info_tmp.api_argument);

    case VT_GETTIME_PID:
      // Any process can invoke this call.

      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_GETTIME_PID: Operation cannot be performed when experiment is "
            "not initialized !");
        return -EFAULT;
      }

      if (experiment_status == NOTRUNNING) {
        PDEBUG_I(
            "VT_GETTIME_PID: Operation cannot be performed when experiment is "
            "not running!");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
        return -EFAULT;
      }
      api_info_tmp.return_value = handle_gettimepid(api_info_tmp.api_argument);
      if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
        PDEBUG_I("VT_GETTIME_PID: Error copying to user buf\n");
        return -EFAULT;
      }
      return 0;

    case VT_STOP_EXP:
      // Any process can invoke this call.
      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_STOP_EXP: Operation cannot be performed when experiment is not "
            "initialized !");
        return -EFAULT;
      }

      if (experiment_status == NOTRUNNING) {
        PDEBUG_I(
            "VT_STOP_EXP: Operation cannot be performed when experiment is not "
            "running!");
        return -EFAULT;
      }

      return handle_stop_exp_cmd();

    case VT_PROGRESS_BY:
      // Any process can invoke this call.

      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_PROGRESS_BY: Operation cannot be performed when experiment is "
            "not initialized !");
        return -EFAULT;
      }

      if (experiment_status == NOTRUNNING) {
        PDEBUG_I(
            "VT_PROGRESS_BY: Operation cannot be performed when experiment is "
            "not running!");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }

      num_integer_args = convert_string_to_array(
          api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

      if (num_integer_args < 1) {
        PDEBUG_I("VT_PROGRESS_BY: Not enough arguments !");
        return -EFAULT;
      }

      num_rounds = api_integer_args[0];
      return progress_by(api_info_tmp.return_value, num_rounds);

    case VT_PROGRESS_TIMELINE_BY:
      // Any process can invoke this call.

      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_PROGRESS_TIMELINE_BY: Operation cannot be performed when "
            "experiment is not initialized !");
        return -EFAULT;
      }

      if (experiment_status == NOTRUNNING) {
        PDEBUG_I(
            "VT_PROGRESS_TIMELINE_BY: Operation cannot be performed when "
            "experiment is not running!");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }

      num_integer_args = convert_string_to_array(
          api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

      if (num_integer_args < 2) {
        PDEBUG_I("VT_PROGRESS_TIMELINE_BY: Not enough arguments !");
        return -EFAULT;
      }

      timeline_id = api_integer_args[0];
      num_rounds = api_integer_args[1];
      return progress_timeline_by(timeline_id, api_info_tmp.return_value,
                                  num_rounds);

    case VT_WAIT_FOR_EXIT:

      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_WAIT_FOR_EXIT: Operation cannot be performed when experiment "
            "is not initialized !\n");
        return -EFAULT;
      }

      if (!dilated_task || dilated_task->associated_tracer_id <= 0) {
        PDEBUG_I("VT_WAIT_FOR_EXIT: Process is not associated with "
                 "any tracer !\n");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }

      num_integer_args = convert_string_to_array(
          api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

      if (num_integer_args < 1) {
        PDEBUG_I("VT_WAIT_FOR_EXIT: Not enough arguments !");
        return -EFAULT;
      }

      tracer_id = api_integer_args[0];
      curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
      if (!curr_tracer) {
        PDEBUG_I("VT_WAIT_FOR_EXIT: Tracer : %d, not registered\n", tracer_id);
        return -EFAULT;
      }

      wait_event_interruptible(*curr_tracer->w_queue,
                               dilated_task->associated_tracer_id <= 0);
      PDEBUG_V("VT_WAIT_FOR_EXIT: Tracer : %d, Process: %d, "
               "resuming from wait for exit\n", tracer_id, current->pid);
      return 0;

    default:
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
  run_usermode_synchronizer_process(argv[0], argv, envp, UMH_NO_WAIT);
#endif

  initialization_status = NOT_INITIALIZED;
  experiment_status = NOTRUNNING;

  round_task = kthread_create(&round_sync_task, NULL, "round_sync_task");
  if (!IS_ERR(round_task)) {
    kthread_bind(round_task, 0);
    wake_up_process(round_task);
  }

  /* Acquire number of CPUs on system */
  TOTAL_CPUS = num_online_cpus();
  PDEBUG_A("Total Number of CPUS: %d\n", num_online_cpus());

  if (TOTAL_CPUS > 2)
    EXP_CPUS = TOTAL_CPUS - 2;
  else
    EXP_CPUS = 1;

  tracer_wqueue = kmalloc(EXP_CPUS * sizeof(wait_queue_head_t), GFP_KERNEL);

  if (!tracer_wqueue) {
    PDEBUG_I("Error: Could not allot memory for tracer wait queue\n");
    return -ENOMEM;
  }

  for (i = 0; i < EXP_CPUS; i++) {
    init_waitqueue_head(&tracer_wqueue[i]);
  }

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

  /* Busy wait briefly for tasks to finish -Not the best approach */
  for (i = 0; i < 1000000000; i++) {}

  if (kthread_stop(round_task)) {
    PDEBUG_E("Stopping round_task error\n");
  }

  for (i = 0; i < 1000000000; i++) {}

  if (tracer_num > 0) {
    // Free all tracer structs from previous experiment if any
    free_all_tracers();
    tracer_num = 0;
  }

  /* Kill the looping task */
#ifdef __x86_64
  if (loop_task != NULL) kill_p(loop_task, SIGKILL);
#endif
  PDEBUG_A("MODULE UNLOADED\n");
}

/* Register the init and exit functions here so insmod can run them */
module_init(my_module_init);
module_exit(my_module_exit);

/* Required by kernel */
MODULE_LICENSE("GPL");
