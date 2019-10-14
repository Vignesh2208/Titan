#include "vt_module.h"


#include "module.h"
#include "common.h"
#include "sync_experiment.h"

/*
Has basic functionality for the Kernel Module itself. It defines how the
userland process communicates with the kernel module,
as well as what should happen when the kernel module is initialized and removed.
*/


// number of TRACERS in the experiment
int tracer_num = 0;
// number of tracers for which a spinner has already been spawned
int n_processed_tracers = 0;
int EXP_CPUS = 0;
int TOTAL_CPUS = 0;
// flag to determine state of the experiment
int experiment_stopped;
// INTIALIZED/NOT INITIALIZED
int experiment_status;
// CS or CBE
int experiment_type;

struct mutex exp_lock;
struct mutex file_lock;
int *per_cpu_chain_length;
llist * per_cpu_tracer_list;

s64 * tracer_clock_array;
s64 * aligned_tracer_clock_array;

//hashmap of <TRACER_NUMBER, TRACER_STRUCT>
hashmap get_tracer_by_id;
//hashmap of <PID, TRACER_STRUCT>
hashmap get_tracer_by_pid;
atomic_t n_waiting_tracers = ATOMIC_INIT(0);


// Proc file declarations
static struct proc_dir_entry *dilation_dir = NULL;
static struct proc_dir_entry *dilation_file = NULL;

//number of CPUs in the system
int TOTAL_CPUS;


//task that loops endlessly (64-bit)
struct task_struct *loop_task;
struct task_struct * round_task = NULL;
extern wait_queue_head_t progress_sync_proc_wqueue;
extern wait_queue_head_t expstop_call_proc_wqueue;
extern int initialize_experiment_components(char * write_buffer);
extern int round_sync_task(void *data);
extern void run_dilated_hrtimers();
extern s64 expected_time;
extern s64 virt_exp_start_time;



static const struct file_operations proc_file_fops = {
	.unlocked_ioctl = vt_ioctl,
	.llseek = vt_llseek,
	.release = vt_release,
    .mmap = vt_mmap,
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
Gets the PID of our tracer spin task (only in 64 bit)
***/
int get_tracer_spinner_pid(struct subprocess_info *info, struct cred *new) {

	int curr_tracer_no;
	tracer * curr_tracer;
	int i = 0;
	mutex_lock(&exp_lock);
	for (i = 1 ; i <= tracer_num; i++) {
		curr_tracer = hmap_get_abs(&get_tracer_by_id, i);

		if (curr_tracer && curr_tracer->create_spinner) {
			curr_tracer->spinner_task = current;
			curr_tracer->create_spinner = 0;
			curr_tracer_no = i;
			PDEBUG_A("Tracer Spinner Started for Tracer no: %d, "
			         "Spinned Pid = %d\n", curr_tracer_no, current->pid);
		}
	}
	mutex_unlock(&exp_lock);

	bitmap_zero((&current->cpus_allowed)->bits, 8);
	cpumask_set_cpu(1, &current->cpus_allowed);



	return 0;
}

/***
Hack to get 64 bit running correctly. Starts a process that will just loop
while the experiment is going on. Starts an executable specified in the path
in user space from kernel space.
***/
int run_usermode_synchronizer_process(char *path, char **argv,
                                      char **envp, int wait) {
	struct subprocess_info *info;
	gfp_t gfp_mask = (wait == UMH_NO_WAIT) ? GFP_ATOMIC : GFP_KERNEL;

	info = call_usermodehelper_setup(path, argv, envp, gfp_mask, getSpinnerPid,
	                                 NULL, NULL);
	if (info == NULL)
		return -ENOMEM;

	return call_usermodehelper_exec(info, wait);
}


int run_usermode_tracer_spin_process(char *path, char **argv,
                                     char **envp, int wait) {
	struct subprocess_info *info;
	gfp_t gfp_mask = (wait == UMH_NO_WAIT) ? GFP_ATOMIC : GFP_KERNEL;

	info = call_usermodehelper_setup(path, argv, envp, gfp_mask,
	                                 get_tracer_spinner_pid, NULL, NULL);
	if (info == NULL)
		return -ENOMEM;

	return call_usermodehelper_exec(info, wait);
}


loff_t vt_llseek(struct file * filp, loff_t pos, int whence) 
{
	return 0;
}

int vt_release(struct inode *inode, struct file *filp) 
{
	return 0;
}


static int vt_mmap(struct file *filp, struct vm_area_struct *vma)
{
    
    unsigned long pfn = virt_to_phys((void *)aligned_tracer_clock_array)>>PAGE_SHIFT;
    unsigned long len = vma->vm_end - vma->vm_start;
    int ret ;

    ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);
    if (ret < 0) {
        pr_err("could not map the address area\n");
        return -EIO;
    }

    return 0;
}

long vt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

	int err = 0;
	int retval = 0;
	int i = 0;
	uint8_t mask = 0;
	unsigned long flags;
	overshoot_info * args;
    invoked_api * api_info;
	struct timeval * tv;
	overshoot_info tmp;
    invoked_api api_info_tmp;
	char * ptr;
	int ret, num_integer_args;
    int api_integer_args[MAX_API_ARGUMENT_SIZE];
	tracer * curr_tracer;

	memset(api_info_tmp.api_argument, 0, sizeof(char)*MAX_API_ARGUMENT_SIZE);
    memset(api_integer_args, 0, sizeof(int)*MAX_API_ARGUMENT_SIZE);

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
		err =  !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) return -EFAULT;

	switch (cmd) {


    case VT_UPDATE_TRACER_CLOCK :
        return 0;

    case VT_WRITE_RESULTS :
        api_info = (invoked_api *) arg;
        if (!api_info)
            return -EFAULT;
        if (copy_from_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
			return -EFAULT;
		}
        
        return 0;

    case VT_GET_CURRENT_VIRTUAL_TIME :
        tv = (struct timeval *)arg;
		if (!tv)
			return -EFAULT;

		struct timeval ktv;
		if (expected_time == 0)
			do_gettimeofday(&ktv);
		else {
			ktv = ns_to_timeval(expected_time);
		}

		if (copy_to_user(tv, &ktv, sizeof(ktv)))
			return -EFAULT;

        return 0;

    case VT_REGISTER_TRACER :
        api_info = (invoked_api *) arg;
        if (!api_info)
            return -EFAULT;
        if (copy_from_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
			return -EFAULT;
		}

        return register_tracer_process(api_info_tmp.api_argument);

    case VT_ADD_PROCESSES_TO_SQ :
        api_info = (invoked_api *) arg;
        if (!api_info)
            return -EFAULT;
        if (copy_from_user(api_info, &api_info_tmp,  sizeof(invoked_api))) {
			return -EFAULT;
		}
        num_integer_args = convert_string_to_array(
            api_info_tmp.api_agrument, api_integer_args, MAX_API_ARGUMENT_SIZE);

        curr_tracer = hmap_get_abs(&get_tracer_by_pid, current->pid);
		if (!curr_tracer) {
			PDEBUG_I("VT-IO: Tracer : %d, not registered\n", current->pid);
			return -EFAULT;
		}
        for (i = 0; i < num_integer_args; i++) {
            add_to_tracer_schedule_queue(curr_tracer, api_integer_args[i]);
        }
        
        return 0;

    case VT_RM_PROCESSES_FROM_SQ :
        api_info = (invoked_api *) arg;
        if (!api_info)
            return -EFAULT;
        if (copy_from_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
			return -EFAULT;
		}
        num_integer_args = convert_string_to_array(
            api_info_tmp.api_agrument, api_integer_args, MAX_API_ARGUMENT_SIZE);

        curr_tracer = hmap_get_abs(&get_tracer_by_pid, current->pid);
		if (!curr_tracer) {
			PDEBUG_I("VT-IO: Tracer : %d, not registered\n", current->pid);
			return -EFAULT;
		}
        for (i = 0; i < num_integer_args; i++) {
            remove_from_tracer_schedule_queue(curr_tracer, api_integer_args[i]);
        }
        
        return 0;

    case VT_SYNC_AND_FREEZE :
        return handle_sync_and_freeze_cmd();

    case VT_INITIALIZE_EXP :
        api_info = (invoked_api *) arg;
        if (!api_info)
            return -EFAULT;
        if (copy_from_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
			return -EFAULT;
		}
        return handle_initialize_exp_cmd(api_info_tmp.api_argument);

    case VT_GETTIME_PID :
        api_info = (invoked_api *) arg;
        if (!api_info)
            return -EFAULT;
        if (copy_from_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
			return -EFAULT;
		}
        pi_info_tmp.return_value = handle_gettimepid(api_info_tmp.api_argument);
        return 0;


    case VT_START_EXP :
        return handle_start_exp_cmd();

    case VT_STOP_EXP :
        return handle_stop_exp_cmd();

    case VT_PROGRESS_BY :
        api_info = (invoked_api *) arg;
        if (!api_info)
            return -EFAULT;
        if (copy_from_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
			return -EFAULT;
		}
        return 0;

    case VT_SET_NETDEVICE_OWNER :
        api_info = (invoked_api *) arg;
        if (!api_info)
            return -EFAULT;
        if (copy_from_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
			return -EFAULT;
		}
        return handle_set_netdevice_owner_cmd(api_info_tmp.api_argument);
	
	case TK_IO_WRITE_RESULTS	:	ptr = (char *) arg;
		if (copy_from_user(write_buffer, ptr, STATUS_MAXSIZE)) {
			return -EFAULT;
		}

		current->virt_start_time = 0;
		mutex_lock(&file_lock);
		ret = handle_tracer_results(write_buffer + 2);
		mutex_unlock(&file_lock);

		curr_tracer = hmap_get_abs(&get_tracer_by_pid, current->pid);
		if (!curr_tracer) {
			PDEBUG_I("TK-IO: Tracer : %d, not registered\n", current->pid);
			return -1;
		}

		PDEBUG_V("TK-IO: Tracer : %d, Waiting for next command.\n",
		         current->pid);

		atomic_inc(&n_waiting_tracers);
		wake_up_interruptible(&progress_sync_proc_wqueue);
		wait_event_interruptible(
		    curr_tracer->w_queue,
		    atomic_read(&curr_tracer->w_queue_control) == 0);
		PDEBUG_V("TK-IO: Tracer : %d, Resuming from wait\n", current->pid);


		get_tracer_struct_read(curr_tracer);
		if (copy_to_user(ptr, curr_tracer->run_q_buffer,
		                 curr_tracer->buf_tail_ptr + 1 )) {
			put_tracer_struct_read(curr_tracer);
			atomic_dec(&n_waiting_tracers);
			wake_up_interruptible(&expstop_call_proc_wqueue);
			PDEBUG_I("Status Read: Tracer : %d, Resuming from wait. "
			         "Error copying to user buf\n", current->pid);
			return -EFAULT;
		}

		if (strcmp(curr_tracer->run_q_buffer, "STOP") == 0) {
			ret = curr_tracer->buf_tail_ptr;
			put_tracer_struct_read(curr_tracer);
			// free up memory
			PDEBUG_I("TK-IO: Tracer: %d, STOPPING\n", current->pid);
			get_tracer_struct_write(curr_tracer);
			if (curr_tracer->spinner_task != NULL) {
				curr_tracer->spinner_task = NULL;
			}
			put_tracer_struct_write(curr_tracer);
			hmap_remove_abs(&get_tracer_by_id, curr_tracer->tracer_id);
			hmap_remove_abs(&get_tracer_by_pid, current->pid);

			mutex_lock(&exp_lock);
			kfree(curr_tracer);
			mutex_unlock(&exp_lock);

			atomic_dec(&n_waiting_tracers);
			wake_up_interruptible(&expstop_call_proc_wqueue);
			return 0;

		}
		atomic_dec(&n_waiting_tracers);
		ret = curr_tracer->buf_tail_ptr;
		put_tracer_struct_read(curr_tracer);
		PDEBUG_V("TK-IO: Tracer: %d, Returning value: %d\n", current->pid, ret);
		return 0;

	default: return -ENOTTY;
	}

	return retval;

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

    tracer_clock_array = NULL;
    aligned_tracer_clock_array = NULL;

	mutex_init(&file_lock);

	if (dilation_file == NULL) {
		remove_proc_entry(DILATION_FILE, dilation_dir);
		PDEBUG_E("Error: Could not initialize /proc/%s/%s\n",
		         DILATION_DIR, DILATION_FILE);
		return -ENOMEM;
	}
	PDEBUG_A(" /proc/%s/%s created\n", DILATION_DIR, DILATION_FILE);

	/* If it is 64-bit, initialize the looping script */
#ifdef __x86_64
	char *argv[] = { "/bin/x64_synchronizer", NULL };
	static char *envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL
	};
	run_usermode_synchronizer_process( argv[0], argv, envp, UMH_NO_WAIT );
#endif


	experiment_status = NOT_INITIALIZED;
	experiment_stopped = NOTRUNNING;


	round_task = kthread_create(&round_sync_task, NULL, "round_sync_task");
	if (!IS_ERR(round_task)) {
		kthread_bind(round_task, 0);
		wake_up_process(round_task);
	}

	/* Acquire number of CPUs on system */
	TOTAL_CPUS = num_online_cpus();
	PDEBUG_A("Total Number of CPUS: %d\n", num_online_cpus());

	if (TOTAL_CPUS > 2 )
		EXP_CPUS = TOTAL_CPUS - 2;
	else
		EXP_CPUS = 1;

	expected_time = 0;

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

	//remove_proc_entry(DILATION_FILE, dilation_dir);
	remove_proc_entry(DILATION_FILE, NULL);
	PDEBUG_A(" /proc/%s/%s deleted\n", DILATION_DIR, DILATION_FILE);
	remove_proc_entry(DILATION_DIR, NULL);
	PDEBUG_A(" /proc/%s deleted\n", DILATION_DIR);

	/* Busy wait briefly for tasks to finish -Not the best approach */
	for (i = 0; i < 1000000000; i++) {}

	if ( kthread_stop(round_task) ) {
		PDEBUG_E("Stopping round_task error\n");
	}


	for (i = 0; i < 1000000000; i++) {}



	/* Kill the looping task */
#ifdef __x86_64
	if (loop_task != NULL)
		kill_p(loop_task, SIGKILL);
#endif
	PDEBUG_A("MODULE UNLOADED\n");
}



/* Register the init and exit functions here so insmod can run them */
module_init(my_module_init);
module_exit(my_module_exit);

/* Required by kernel */
MODULE_LICENSE("GPL");
