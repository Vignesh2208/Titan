#include "vt_module.h"
#include "dilated_timer.h"
#include "common.h"
#include "sync_experiment.h"

/** EXTERN DECLARATIONS **/

extern int experiment_status;
extern int experiment_type;
extern int initialization_status;

extern hashmap get_tracer_by_id;
extern hashmap get_tracer_by_pid;
extern hashmap get_dilated_task_struct_by_pid;

extern atomic_t n_waiting_tracers;
extern wait_queue_head_t progress_sync_proc_wqueue;
extern wait_queue_head_t * syscall_wait_wqueue;
extern timeline * timeline_info;

ssize_t handle_write_results_cmd(struct dilated_task_struct * dilation_task,
                                 int *api_args, int num_args) {

	int tracer_id;
	tracer *curr_tracer;


	tracer_id = dilation_task->associated_tracer_id;
	curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
	if (!curr_tracer) {
		PDEBUG_I("VT_WRITE_RES: Tracer : %d, not registered\n", tracer_id);
		return 0;
	} 

	dilation_task->ready = 1; 
	dilation_task->syscall_waiting = 0;
	handle_tracer_results(curr_tracer, &api_args[0], num_args);

	wait_event_interruptible(
		dilation_task->d_task_wqueue,
		dilation_task->associated_tracer_id <= 0 ||
		(curr_tracer->w_queue_wakeup_pid == current->pid &&
		 dilation_task->burst_target > 0));
	PDEBUG_V(
		"VT_WRITE_RES: Associated Tracer : %d, Process: %d, resuming from wait\n",
		dilation_task->associated_tracer_id, current->pid);

	//PDEBUG_I("VT_WRITE_RES: Trigger Delay: %lld\n", ktime_get_real() - dilation_task->trigger_time);
	dilation_task->ready = 0;


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

int handle_vt_update_tracer_clock(unsigned long arg, struct dilated_task_struct * dilated_task)  {

	
	invoked_api *api_info;
	invoked_api api_info_tmp;
	int num_integer_args;
	int api_integer_args[MAX_API_ARGUMENT_SIZE];
	tracer *curr_tracer;
	int tracer_id;

	memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
  	memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);


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

}


int handle_vt_write_results(unsigned long arg, struct dilated_task_struct * dilated_task)  {

	
	invoked_api *api_info;
	invoked_api api_info_tmp;
	int num_integer_args;
	int api_integer_args[MAX_API_ARGUMENT_SIZE];
	tracer *curr_tracer;
	int tracer_id;

	memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
  	memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);

      	// A registered tracer process or one of the processes in a tracer's
      	// schedule queue can invoke this ioctl For INSVT tracer type, only the
      	// tracer process can invoke this ioctl

      

      	if (initialization_status != INITIALIZED) {
        	PDEBUG_I("VT_WRITE_RESULTS: Operation cannot be performed when "
        	         "experiment is not initialized !\n");
        	return -EFAULT;
      	}



      	if (!dilated_task || dilated_task->associated_tracer_id <= 0) {
        	PDEBUG_I("VT_WRITE_RESULTS: Process is not associated with "
                 	"any tracer !\n");
        	return -EFAULT;
      	}

      	tracer_id = dilated_task->associated_tracer_id;
      	PDEBUG_V("VT_WRITE_RESULTS: Tracer: %d, Process: %d Entering !\n",
          	tracer_id, current->pid);

      	api_info = (invoked_api *)arg;
      	if (!api_info) return -EFAULT;
      	if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
		return -EFAULT;
      	}


	num_integer_args = convert_string_to_array(
	  api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

	tracer_id = dilated_task->associated_tracer_id;
	curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
	if (!curr_tracer) {
		PDEBUG_I("VT_WRITE_RESULTS: Tracer : %d, not registered\n", current->pid);
		return -EFAULT;
	}

	PDEBUG_V("VT_WRITE_RESULTS: Tracer: %d, Process: %d Handling results. Num args: %d!\n",
		tracer_id, current->pid, num_integer_args);
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

		hmap_remove_abs(&get_dilated_task_struct_by_pid, current->pid);
		if (curr_tracer->main_task == dilated_task)
			curr_tracer->main_task = NULL;
		kfree(dilated_task);
		return 0;
	}

	api_info_tmp.return_value = dilated_task->burst_target;
	if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
		PDEBUG_I("VT_WRITE_RESULTS: Tracer : %d, Process: %d "
			 "Resuming from wait. Error copying to user buf\n",
			 tracer_id, current->pid);
		return -EFAULT;
	}

	return 0;

}

int handle_vt_register_tracer(unsigned long arg, struct dilated_task_struct * dilated_task)  {

	
	invoked_api *api_info;
	invoked_api api_info_tmp;
	int num_integer_args;
	int api_integer_args[MAX_API_ARGUMENT_SIZE];
	tracer *curr_tracer;
	int tracer_id;
	int retval;

	memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
  	memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);

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
}


int handle_vt_add_processes_to_sq(unsigned long arg, struct dilated_task_struct * dilated_task)  {

	
	invoked_api *api_info;
	invoked_api api_info_tmp;
	int num_integer_args;
	int api_integer_args[MAX_API_ARGUMENT_SIZE];
	tracer *curr_tracer;
	int tracer_id, i;

	memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
  	memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);

        // Any process can invoke this call.

	PDEBUG_V("VT_ADD_PROCESSES_TO_SQ: Entering !\n");

	if (initialization_status != INITIALIZED ||
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
		//add_to_tracer_schedule_queue(curr_tracer, get_task_ns(api_integer_args[i], curr_tracer));
	}
	put_tracer_struct_write(curr_tracer);

	PDEBUG_V("VT_ADD_PROCESSES_TO_SQ: Finished Successfully !\n");
	return 0;
}

int handle_vt_initialize_exp(unsigned long arg)  {

	
	invoked_api *api_info;
	invoked_api api_info_tmp;
	int num_integer_args;
	int api_integer_args[MAX_API_ARGUMENT_SIZE];


	memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
  	memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);

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
	num_integer_args = convert_string_to_array(
	  api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

	if (num_integer_args != 3) {
		PDEBUG_I("VT_INITIALIZE_EXP: Not enough arguments !");
		return -EFAULT;
	}

	return handle_initialize_exp_cmd(api_integer_args[0],
					api_integer_args[1], api_integer_args[2]);
}

int handle_vt_sync_freeze() {
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
}

int handle_vt_gettimepid(unsigned long arg) {

	
      invoked_api *api_info;
      invoked_api api_info_tmp;
      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);


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
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
		return -EFAULT;
      }
      api_info_tmp.return_value = handle_gettimepid(api_info_tmp.api_argument);
      if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
        PDEBUG_I("VT_GETTIME_PID: Error copying to user buf\n");
        return -EFAULT;
      }
      return 0;
}

int handle_vt_gettime_tracer(unsigned long arg, struct dilated_task_struct * dilated_task) {


      invoked_api *api_info;
      invoked_api api_info_tmp;
      int num_integer_args;
      int api_integer_args[MAX_API_ARGUMENT_SIZE];
      tracer *curr_tracer;
      int tracer_id;

      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
      memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);
      
      // Any process can invoke this call.

      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_GETTIME_TRACER: Operation cannot be performed when experiment is "
            "not initialized !");
        return -EFAULT;
      }

      if (experiment_status == NOTRUNNING) {
        PDEBUG_I(
            "VT_GETTIME_TRACER: Operation cannot be performed when experiment is "
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
        PDEBUG_I("VT_GETTIME_TRACER: Not enough arguments !");
        return -EFAULT;
      }

      tracer_id = api_integer_args[0];
      curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
      if (!curr_tracer) {
        PDEBUG_I("VT_GETTIME_TRACER: Tracer : %d, not registered\n", tracer_id);
        return -EFAULT;
      }

      api_info_tmp.return_value = curr_tracer->curr_virtual_time;
      if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
        PDEBUG_I("VT_GETTIME_TRACER: Error copying to user buf\n");
        return -EFAULT;
      }
      return 0;
}

int handle_vt_stop_exp() {

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
}


int handle_vt_progress_by(unsigned long arg) {


      invoked_api *api_info;
      invoked_api api_info_tmp;
      int num_integer_args;
      int api_integer_args[MAX_API_ARGUMENT_SIZE];
      
      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
      memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);

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

      if (experiment_type != EXP_CBE) {
        PDEBUG_A("Progress cannot be called for EXP_CS !\n");
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

      return progress_by(api_info_tmp.return_value, api_integer_args[0]);
}

int handle_vt_progress_timeline_by(unsigned long arg) {


      invoked_api *api_info;
      invoked_api api_info_tmp;
      int num_integer_args;
      int api_integer_args[MAX_API_ARGUMENT_SIZE];

      int timeline_id;
      
      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
      memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);


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


      if (experiment_type != EXP_CS) {
        PDEBUG_A("Progress timeline cannot be called for EXP_CBE !\n");
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
        PDEBUG_I("VT_PROGRESS_TIMELINE_BY: Not enough arguments !");
        return -EFAULT;
      }

      timeline_id = api_integer_args[0];
      return progress_timeline_by(timeline_id, api_info_tmp.return_value);
}

int handle_vt_wait_for_exit(unsigned long arg, struct dilated_task_struct * dilated_task) {

      invoked_api *api_info;
      invoked_api api_info_tmp;
      int num_integer_args;
      int api_integer_args[MAX_API_ARGUMENT_SIZE];
      int tracer_id;

      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
      memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);


      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_WAIT_FOR_EXIT: Operation cannot be performed when experiment "
            "is not initialized !\n");
        return -EFAULT;
      }

      if (dilated_task && dilated_task->associated_tracer_id > 0) {
        PDEBUG_I("VT_WAIT_FOR_EXIT: Process is already associated with "
                 "a tracer. It cannot invoke this command !\n");
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
      PDEBUG_I("VT_WAIT_FOR_EXIT: Tracer : %d "
               "starting wait for exit\n", tracer_id);

      wait_event_interruptible(*timeline_info[0].w_queue,
                               timeline_info[0].stopping == 1);
      PDEBUG_I("VT_WAIT_FOR_EXIT: Tracer : %d "
               "resuming from wait for exit\n", tracer_id);
      
      return 0;

}

int handle_vt_sleep_for(unsigned long arg, struct dilated_task_struct * dilated_task) {

      invoked_api *api_info;
      invoked_api api_info_tmp;
      ktime_t duration;
      
      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);


      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_SLEEP_FOR: Operation cannot be performed when experiment "
            "is not initialized !\n");
        return -EFAULT;
      }

      if (experiment_status != RUNNING) {
        PDEBUG_I(
            "VT_SLEEP_FOR: Operation cannot be performed when experiment "
            "is not running !\n");
        return -EFAULT;
      }

      if (!dilated_task || dilated_task->associated_tracer_id <= 0) {
        PDEBUG_I("VT_SLEEP_FOR: Process is not associated with "
                 "any tracer !\n");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }
      duration = api_info_tmp.return_value; 

      PDEBUG_V(
        "VT_SLEEP_FOR: Process: %d, duration: %llu\n", current->pid, duration);
             
      return dilated_hrtimer_sleep(duration, dilated_task);

}

int handle_vt_release_worker(unsigned long arg, struct dilated_task_struct * dilated_task) {
      
      tracer * curr_tracer;

      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_RELEASE_WORKER: Operation cannot be performed when experiment "
            "is not initialized !\n");
        return -EFAULT;
      }

      if (!dilated_task || dilated_task->associated_tracer_id <= 0) {
        PDEBUG_I(
          "VT_RELEASE_WORKER: Process is not associated with any tracer !\n");
        return -EFAULT;
      }
      PDEBUG_V(
        "VT_RELEASE_WORKER: Associated Tracer : %d, Process: %d, "
        "entering wait\n", dilated_task->associated_tracer_id, current->pid);
      dilated_task->burst_target = 0;
      dilated_task->ready = 0;
      dilated_task->syscall_waiting = 0;
      curr_tracer = dilated_task->associated_tracer;
      curr_tracer->w_queue_wakeup_pid = 1;

      if (curr_tracer->last_run != NULL 
	  && dilated_task->pid == curr_tracer->last_run->pid) {
      		curr_tracer->last_run->quanta_left_from_prev_round = 0;
		curr_tracer->last_run = NULL;
      }
      PDEBUG_V("VT_RELEASE_WORKER: signalling worker resume. Tracer ID: %d\n",
			curr_tracer->tracer_id);
      wake_up_interruptible(curr_tracer->w_queue);
      return 0;

}

int handle_vt_set_runnable(unsigned long arg, struct dilated_task_struct * dilated_task) {

      invoked_api *api_info;
      invoked_api api_info_tmp;
      int num_integer_args;
      int api_integer_args[MAX_API_ARGUMENT_SIZE];
      tracer * curr_tracer;
      int tracer_id;

      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
      memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);

      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_SET_RUNNABLE: Operation cannot be performed when experiment "
            "is not initialized !\n");
        return -EFAULT;
      }

      if (!dilated_task || dilated_task->associated_tracer_id <= 0) {
        PDEBUG_I("VT_SET_RUNNABLE: Process is not associated with "
                 "any tracer !\n");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }


      dilated_task->burst_target = 0;
      dilated_task->syscall_waiting = 0;
      dilated_task->ready = 1;
      curr_tracer = dilated_task->associated_tracer;
      tracer_id = curr_tracer->tracer_id;

      if (api_info_tmp.return_value == SIGNAL_SYSCALL_FINISH) {
        PDEBUG_V(
        "VT_SET_RUNNABLE: Associated Tracer : %d, Process: %d, "
        "signalling syscall finish\n", tracer_id, current->pid);
	if (!dilated_task->resumed_by_dilated_timer)
        	wake_up_interruptible(
         	 &syscall_wait_wqueue[curr_tracer->timeline_assignment]);
	else {
		wake_up_interruptible(&dilated_task->d_task_wqueue);
	}
      }
      PDEBUG_V(
        "VT_SET_RUNNABLE: Associated Tracer : %d, Process: %d, "
        "entering wait\n", tracer_id, current->pid);
     
      wait_event_interruptible(
        dilated_task->d_task_wqueue, dilated_task->associated_tracer_id <= 0 ||
            (curr_tracer->w_queue_wakeup_pid == current->pid &&
            dilated_task->burst_target > 0));
      
      dilated_task->ready = 0;

      if (dilated_task->associated_tracer_id <= 0) {
        dilated_task->burst_target = 0;
        dilated_task->virt_start_time = 0;
        dilated_task->curr_virt_time = 0;
        dilated_task->associated_tracer_id = 0;
        dilated_task->wakeup_time = 0;
        dilated_task->vt_exec_task = NULL;
        dilated_task->base_task = NULL;
        PDEBUG_I("VT_SET_RUNNABLE: Tracer: %d, Process: %d STOPPING\n",
                 tracer_id, current->pid);
        hmap_remove_abs(&get_dilated_task_struct_by_pid, current->pid);
	      if (curr_tracer->main_task == dilated_task)
		      curr_tracer->main_task = NULL;

        kfree(dilated_task);

        api_info_tmp.return_value = 0;
        if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
          PDEBUG_I("VT_SET_RUNNABLE: Tracer : %d, Process: %d "
                  "Resuming from wait. Error copying to user buf\n",
                  tracer_id, current->pid);
          return -EFAULT;
        }

        return 0;
      }

      api_info_tmp.return_value = dilated_task->burst_target;
      if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
        PDEBUG_I("VT_SET_RUNNABLE: Tracer : %d, Process: %d "
                 "Resuming from wait. Error copying to user buf\n",
                 tracer_id, current->pid);
        return -EFAULT;
      }

      PDEBUG_V(
        "VT_SET_RUNNABLE: Associated Tracer : %d, Process: %d, "
        "resuming from wait\n", tracer_id, current->pid);

      return 0;

}

int handle_vt_gettime_my_pid(unsigned long arg) {

      invoked_api *api_info;
      invoked_api api_info_tmp;
      tracer * curr_tracer;
      int tracer_id;

      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);


      // Any process can invoke this call.

      if (initialization_status != INITIALIZED) {
        PDEBUG_I(
            "VT_GETTIME_MY_PID: Operation cannot be performed when experiment"
            "is not initialized !\n");
        return -EFAULT;
      }

      if (experiment_status == NOTRUNNING) {
        PDEBUG_I(
            "VT_GETTIME_MY_PID: Operation cannot be performed when experiment"
            "is not running !\n");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }

      PDEBUG_V(
        "VT_GETTIME_MY_PID: Process: %d\n", current->pid);
      api_info_tmp.return_value = get_dilated_time(current);
      if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
        PDEBUG_I("VT_GETTIME_MY_PID: Error copying to user buf \n");
        return -EFAULT;
      }
      return 0;

}

int handle_vt_add_to_sq(unsigned long arg) {

      invoked_api *api_info;
      invoked_api api_info_tmp;
      int num_integer_args;
      int api_integer_args[MAX_API_ARGUMENT_SIZE];
      tracer * curr_tracer;
      int tracer_id;

      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
      memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);


      // Any process can invoke this call.

     // PDEBUG_V("VT_ADD_TO_SQ: Entering !\n");

      if (initialization_status != INITIALIZED
        || experiment_status == STOPPING) {
        PDEBUG_I(
            "VT_ADD_TO_SQ: Operation cannot be performed when experiment is "
            "not initialized !\n");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }
      num_integer_args = convert_string_to_array(
          api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

      if (num_integer_args <= 0) {
        PDEBUG_I("VT_ADD_TO_SQ: Not enough arguments !");
        return -EFAULT;
      }

      tracer_id = api_integer_args[0];
      curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
      if (!curr_tracer) {
        PDEBUG_I("VT_ADD_TO_SQ: Tracer : %d, not registered\n", tracer_id);
        return -EFAULT;
      }
      get_tracer_struct_write(curr_tracer);
      add_to_tracer_schedule_queue(curr_tracer, current);
      put_tracer_struct_write(curr_tracer);

      PDEBUG_V("VT_ADD_TO_SQ: Finished Successfully !\n");
      return 0;
}


int handle_vt_syscall_wait(unsigned long arg, struct dilated_task_struct * dilated_task) {

      invoked_api *api_info;
      invoked_api api_info_tmp;
      int num_integer_args;
      int api_integer_args[MAX_API_ARGUMENT_SIZE];
      tracer * curr_tracer;
      int tracer_id;

      memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
      memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);


      if (initialization_status != INITIALIZED
          || experiment_status != RUNNING) {
        PDEBUG_I(
            "VT_SYSCALL_WAIT: Operation cannot be performed when experiment "
            "is not initialized and running !\n");
        return -EFAULT;
      }

      if (!dilated_task || dilated_task->associated_tracer_id <= 0) {
        PDEBUG_I("VT_SYSCALL_WAIT: Process is not associated with "
                 "any tracer !\n");
        return -EFAULT;
      }

      api_info = (invoked_api *)arg;
      if (!api_info) return -EFAULT;
      if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
        return -EFAULT;
      }

      dilated_task->syscall_waiting = 1;
      dilated_task->ready = 0;
      curr_tracer = dilated_task->associated_tracer;
      tracer_id = curr_tracer->tracer_id;
  
      if (dilated_task->burst_target == 0) {
        wake_up_interruptible(
          &syscall_wait_wqueue[curr_tracer->timeline_assignment]);
        PDEBUG_V("VT_SYSCALL_WAIT: Associated Tracer : %d, Process: %d, "
          "signalling syscall wait\n", tracer_id, current->pid);
      } else {
        dilated_task->burst_target = 0;
        curr_tracer->w_queue_wakeup_pid = 1;
        PDEBUG_V("VT_SYSCALL_WAIT: signalling worker resume. Tracer ID: %d\n",
          curr_tracer->tracer_id);
        wake_up_interruptible(curr_tracer->w_queue);
      }

      
      PDEBUG_V(
        "VT_SYSCALL_WAIT: Associated Tracer : %d, Process: %d, "
        "entering syscall wait\n", tracer_id, current->pid);
     
      wait_event_interruptible(
        syscall_wait_wqueue[curr_tracer->timeline_assignment],
        dilated_task->syscall_waiting == 0);
      
      dilated_task->ready = 0;

      if (dilated_task->associated_tracer_id <= 0) {
        dilated_task->burst_target = 0;
        dilated_task->virt_start_time = 0;
        dilated_task->curr_virt_time = 0;
        dilated_task->associated_tracer_id = 0;
        dilated_task->wakeup_time = 0;
        dilated_task->vt_exec_task = NULL;
        dilated_task->base_task = NULL;
        dilated_task->syscall_waiting = 0;
        PDEBUG_I("VT_SYSCALL_WAIT: Tracer: %d, Process: %d STOPPING\n",
                 tracer_id, current->pid);
        hmap_remove_abs(&get_dilated_task_struct_by_pid, current->pid);
	if (curr_tracer->main_task == dilated_task)
		curr_tracer->main_task = NULL;
        kfree(dilated_task);

        api_info_tmp.return_value = 0;
        if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
          PDEBUG_I("VT_SYSCALL_WAIT: Tracer : %d, Process: %d "
                  "Resuming from wait. Error copying to user buf\n",
                  tracer_id, current->pid);
          return -EFAULT;
        }

        return 0;
      }

      api_info_tmp.return_value = 1;
      if (copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api))) {
        PDEBUG_I("VT_SYSCALL_WAIT: Tracer : %d, Process: %d "
                 "Resuming from wait. Error copying to user buf\n",
                 tracer_id, current->pid);
        return -EFAULT;
      }

      PDEBUG_V(
        "VT_SYSCALL_WAIT: Associated Tracer : %d, Process: %d, "
        "resuming from wait\n", tracer_id, current->pid);
      return 1;
}


int handle_vt_set_packet_send_time(unsigned long arg, struct dilated_task_struct * dilated_task) {

        invoked_api *api_info;
        invoked_api api_info_tmp;
        int num_integer_args;
        int api_integer_args[MAX_API_ARGUMENT_SIZE];
	int payload_hash, payload_len;

        memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
        memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);


	if (initialization_status != INITIALIZED
	    || experiment_status == STOPPING) {
		PDEBUG_I(
		    "VT_SET_PACKET_SEND_TIME: Operation cannot be performed when experiment is "
		    "not initialized !\n");
		return -EFAULT;
      	}

	if (experiment_type != EXP_CS) {
		PDEBUG_I("VT_SET_PACKET_SEND_TIME: Operation cannot be performed for EXP_CBE!\n");
		return 0;
	}

	if (!dilated_task) {
		PDEBUG_I("VT_SET_PACKET_SEND_TIME: No associated dilated task !\n");
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
		PDEBUG_I("VT_SET_PACKET_SEND_TIME: Not enough arguments !");
		return -EFAULT;
	}
	
	
	payload_hash = api_integer_args[0];
	payload_len = api_integer_args[1];
	PDEBUG_I("VT_SET_PACKET_SEND_TIME: Entered for payload_hash: %d, payload_len = %d, at time: %llu\n",
		payload_hash, payload_len, dilated_task->associated_tracer->curr_virtual_time);
	add_to_pkt_info_queue(dilated_task->associated_tracer, payload_hash, payload_len, api_info_tmp.return_value);
	
	return 0;

}


int handle_vt_get_packet_send_time(unsigned long arg) {

        invoked_api *api_info;
        invoked_api api_info_tmp;
        int num_integer_args;
        int api_integer_args[MAX_API_ARGUMENT_SIZE];
	int payload_hash;
	int tracer_id;
	tracer * curr_tracer;

        memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
        memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);


	if (initialization_status != INITIALIZED
	    || experiment_status == STOPPING) {
		PDEBUG_I(
		    "VT_GET_PACKET_SEND_TIME: Operation cannot be performed when experiment is "
		    "not initialized !\n");
		return -EFAULT;
      	}

	if (experiment_type != EXP_CS) {
		PDEBUG_I("VT_GET_PACKET_SEND_TIME: Operation cannot be performed for EXP_CBE!\n");
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
		PDEBUG_I("VT_GET_PACKET_SEND_TIME: Not enough arguments !");
		return -EFAULT;
	}
	
	tracer_id = api_integer_args[0];
	payload_hash = api_integer_args[1];

	curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
        if (!curr_tracer) {
        	PDEBUG_I("VT_GET_PACKET_SEND_TIME: Tracer : %d, not registered\n", tracer_id);
        	return -EFAULT;
      	}
	PDEBUG_I("VT_GET_PACKET_SEND_TIME: Attempting for packets sent by Tracer : %d, "
		"payload_hash: %d, at time: %llu\n", tracer_id, payload_hash, curr_tracer->curr_virtual_time);
	api_info_tmp.return_value = get_pkt_send_tstamp(curr_tracer, payload_hash);
	
	BUG_ON(copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api)));
	return 0;

}

int handle_vt_get_num_queued_bytes(unsigned long arg) {

        invoked_api *api_info;
        invoked_api api_info_tmp;
        int num_integer_args;
        int api_integer_args[MAX_API_ARGUMENT_SIZE];
	int payload_hash;
	int tracer_id;
	tracer * curr_tracer;

        memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
        memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);

	if (initialization_status != INITIALIZED
	    || experiment_status == STOPPING) {
		PDEBUG_I(
		    "VT_GET_NUM_ENQUEUED_BYTES: Operation cannot be performed when experiment is "
		    "not initialized !\n");
		return -EFAULT;
      	}

	if (experiment_type != EXP_CS) {
		PDEBUG_I("VT_GET_NUM_ENQUEUED_BYTES: Operation cannot be performed for EXP_CBE!\n");
		return -EFAULT;
	}

	api_info = (invoked_api *)arg;
	if (!api_info) return -EFAULT;
	if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
		return -EFAULT;
	}
	num_integer_args = convert_string_to_array(
	  api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

	if (num_integer_args <= 0) {
		PDEBUG_I("VT_GET_NUM_ENQUEUED_BYTES: Not enough arguments !");
		return -EFAULT;
	}
	
	tracer_id = api_integer_args[0];
	curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
        if (!curr_tracer) {
        	PDEBUG_I("VT_GET_NUM_ENQUEUED_BYTES: Tracer : %d, not registered\n", tracer_id);
        	return -EFAULT;
      	}

	api_info_tmp.return_value = get_num_enqueued_bytes(curr_tracer);
	if (api_info_tmp.return_value > 0) {
		PDEBUG_I("VT_GET_NUM_ENQUEUED_BYTES: %llu, for Tracer: %d\n",
			api_info_tmp.return_value, tracer_id);
	}
	BUG_ON(copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api)));
	return 0;
}


int handle_vt_set_eat(unsigned long arg) {

        invoked_api *api_info;
        invoked_api api_info_tmp;
        int num_integer_args;
        int api_integer_args[MAX_API_ARGUMENT_SIZE];
	int tracer_id;
	tracer * curr_tracer;

        memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
        memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);


	if (initialization_status != INITIALIZED
	    || experiment_status == STOPPING) {
		PDEBUG_I(
		    "VT_SET_EAT: Operation cannot be performed when experiment is "
		    "not initialized !\n");
		return -EFAULT;
      	}

	if (experiment_type != EXP_CS) {
		PDEBUG_I("VT_SET_EAT: Operation cannot be performed for EXP_CBE!\n");
		return -EFAULT;
	}

	api_info = (invoked_api *)arg;
	if (!api_info) return -EFAULT;
	if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
		return -EFAULT;
	}
	num_integer_args = convert_string_to_array(
	  api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

	if (num_integer_args <= 0) {
		PDEBUG_I("VT_SET_EAT: Not enough arguments !");
		return -EFAULT;
	}
	
	tracer_id = api_integer_args[0];
	curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
        if (!curr_tracer) {
        	PDEBUG_I("VT_SET_EAT: Tracer : %d, not registered\n", tracer_id);
        	return -EFAULT;
      	}

	curr_tracer->earliest_arrival_time = api_info_tmp.return_value;
	return 0;

}


int handle_vt_get_eat(unsigned long arg, struct dilated_task_struct * dilated_task) {

	invoked_api *api_info;
        invoked_api api_info_tmp;
	

        memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
        
	if (initialization_status != INITIALIZED
	    || experiment_status == STOPPING) {
		PDEBUG_I(
		    "VT_GET_EAT: Operation cannot be performed when experiment is "
		    "not initialized !\n");
		return -EFAULT;
      	}

	if (!dilated_task) {
		PDEBUG_I("VT_GET_EAT: Only a dilated process can request EAT!\n");
		return -EFAULT;
	}

	api_info = (invoked_api *)arg;
	if (!api_info) return -EFAULT;

	BUG_ON(!dilated_task->associated_tracer);

	if (experiment_type != EXP_CS) {
		api_info_tmp.return_value = dilated_task->associated_tracer->curr_virtual_time;
		BUG_ON(copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api)));
		return 0;
	}        

	api_info_tmp.return_value = dilated_task->associated_tracer->earliest_arrival_time;
	BUG_ON(copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api)));
	return 0;

}


s64 get_process_curr_lookahead(struct dilated_task_struct * dilated_task) {
	BUG_ON(!dilated_task);
	s64 curr_process_lookahead;
	if (dilated_task->bulk_lookahead_expiry_time 
		< dilated_task->associated_tracer->curr_virtual_time) {
		curr_process_lookahead 
			= dilated_task->associated_tracer->curr_virtual_time + dilated_task->sp_lookahead_duration;
	} else {
		curr_process_lookahead 
			= dilated_task->bulk_lookahead_expiry_time + dilated_task->sp_lookahead_duration;
	}

	return curr_process_lookahead;
}

int handle_vt_set_process_lookahead(unsigned long arg, struct dilated_task_struct * dilated_task) {

        invoked_api *api_info;
        invoked_api api_info_tmp;
	long sp_lookahead_duration;
	
	s64 curr_process_lookahead, new_process_lookahead, bulk_lookahead_expiry_time; 
	char * ptr;


        memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);

	if (initialization_status != INITIALIZED
	    || experiment_status == STOPPING) {
		PDEBUG_I(
		    "VT_SET_PROCESS_LOOKAHEAD: Operation cannot be performed when experiment is "
		    "not initialized !\n");
		return -EFAULT;
      	}

	if (experiment_type != EXP_CS) {
		PDEBUG_I("VT_SET_PROCESS_LOOKAHEAD: Operation cannot be performed for EXP_CBE!\n");
		return -EFAULT;
	}

	if (!dilated_task) {
		PDEBUG_I("VT_SET_PROCESS_LOOKAHEAD: Only a dilated process can SET_PROCESS_LOOKAHEAD!\n");
		return -EFAULT;
	}

	api_info = (invoked_api *)arg;
	if (!api_info) return -EFAULT;
	
	if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
		return -EFAULT;
	}

	kstrtol(api_info_tmp.api_argument, 10, &sp_lookahead_duration);
	bulk_lookahead_expiry_time = api_info_tmp.return_value;

	curr_process_lookahead = get_process_curr_lookahead(dilated_task);

	if (bulk_lookahead_expiry_time 
		< dilated_task->associated_tracer->curr_virtual_time) {
		new_process_lookahead 
			= dilated_task->associated_tracer->curr_virtual_time + sp_lookahead_duration;
	} else {
		new_process_lookahead 
			= bulk_lookahead_expiry_time + sp_lookahead_duration;
	}

	if (new_process_lookahead > curr_process_lookahead) {
		dilated_task->bulk_lookahead_expiry_time = bulk_lookahead_expiry_time;
		dilated_task->sp_lookahead_duration = sp_lookahead_duration;
	}

	return 0;	

}





int handle_vt_get_tracer_lookahead(unsigned long arg) {

        invoked_api *api_info;
        invoked_api api_info_tmp;
        int num_integer_args;
        int api_integer_args[MAX_API_ARGUMENT_SIZE];
	int tracer_id;
	tracer * curr_tracer;
	s64 min_tracer_lookahead = 0;
	s64 curr_elem_lookahead;
	lxc_schedule_elem* curr_elem;
	llist* schedule_queue;
	llist_elem* head;

        memset(api_info_tmp.api_argument, 0, sizeof(char) * MAX_API_ARGUMENT_SIZE);
        memset(api_integer_args, 0, sizeof(int) * MAX_API_ARGUMENT_SIZE);


	if (initialization_status != INITIALIZED
	    || experiment_status == STOPPING) {
		PDEBUG_I(
		    "VT_GET_TRACER_LOOKAHEAD: Operation cannot be performed when experiment is "
		    "not initialized !\n");
		return -EFAULT;
      	}



	api_info = (invoked_api *)arg;
	if (!api_info) return -EFAULT;
	if (copy_from_user(&api_info_tmp, api_info, sizeof(invoked_api))) {
		return -EFAULT;
	}
	num_integer_args = convert_string_to_array(
	  api_info_tmp.api_argument, api_integer_args, MAX_API_ARGUMENT_SIZE);

	if (num_integer_args <= 0) {
		PDEBUG_I("VT_GET_TRACER_LOOKAHEAD: Not enough arguments !");
		return -EFAULT;
	}
	
	tracer_id = api_integer_args[0];
	curr_tracer = hmap_get_abs(&get_tracer_by_id, tracer_id);
        if (!curr_tracer) {
        	PDEBUG_I("VT_GET_TRACER_LOOKAHEAD: Tracer : %d, not registered\n", tracer_id);
        	return -EFAULT;
      	}

	if (experiment_type != EXP_CS) {
		api_info_tmp.return_value = curr_tracer->curr_virtual_time;
		BUG_ON(copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api)));
		return 0;
	}

	
	schedule_queue = &curr_tracer->schedule_queue;
	head = schedule_queue->head;

	while (head != NULL) {

		if (head) {
			curr_elem = (lxc_schedule_elem *)head->item;
			BUG_ON(!curr_elem);
			curr_elem_lookahead = get_process_curr_lookahead(curr_elem->curr_task); 
			if (min_tracer_lookahead == 0) {
				min_tracer_lookahead = curr_elem_lookahead;
			} else if (curr_elem_lookahead < min_tracer_lookahead) {
				min_tracer_lookahead = curr_elem_lookahead;
			}
		
		}
		head = head->next;
	}

	api_info_tmp.return_value = min_tracer_lookahead;
	BUG_ON(copy_to_user(api_info, &api_info_tmp, sizeof(invoked_api)));
	return 0;

}
