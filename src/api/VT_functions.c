
#include "VT_functions.h"
#include <sys/ioctl.h>
#include "utility_functions.h"

s64 register_tracer(int tracer_id, int experiment_type,
                    int optional_timeline_id) {
  ioctl_args arg;
  if (tracer_id < 0 || optional_timeline_id < 0 || 
      (experiment_type != EXP_CBE && experiment_type != EXP_CS)) {
    printf("Tracer registration: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  init_ioctl_arg(&arg);
  if (experiment_type == EXP_CBE) {
    sprintf(arg.cmd_buf, "%d,%d,0", tracer_id, experiment_type);
  } else {
    sprintf(arg.cmd_buf, "%d,%d,%d", tracer_id, experiment_type,
            optional_timeline_id);
  }
  return send_to_vt_module(VT_REGISTER_TRACER, &arg);
}
int update_tracer_clock(int tracer_id, s64 increment) {
  if (tracer_id < 0 || increment < 0) {
    printf("Update tracer clock: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  sprintf(arg.cmd_buf, "%d", tracer_id);
  arg.cmd_value = increment;
  return send_to_vt_module(VT_UPDATE_TRACER_CLOCK, &arg);
}

int wait_for_exit(int tracer_id) {
  if (tracer_id < 0) {
    printf("Update tracer clock: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  sprintf(arg.cmd_buf, "%d", tracer_id);
  arg.cmd_value = 0;
  return send_to_vt_module(VT_WAIT_FOR_EXIT, &arg);
}
s64 write_tracer_results(int* results, int num_results) {
  if (num_results < 0) {
    printf("Write tracer results: incorrect parameters\n");
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);

  if (num_results > 0) {
    if (append_to_ioctl_arg(&arg, results, num_results) < 0) return -1;
  }
  return send_to_vt_module(VT_WRITE_RESULTS, &arg);
}

s64 get_current_virtual_time() {
  return send_to_vt_module(VT_GET_CURRENT_VIRTUAL_TIME, NULL);
}

s64 get_current_time_pid(int pid) {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  if (pid < 0) {
    printf("get_current_time_pid: incorrect pid: %d\n", pid);
  }
  sprintf(arg.cmd_buf, "%d", pid);
  return send_to_vt_module(VT_GETTIME_PID, &arg);
}

s64 get_current_time_tracer(int tracer_id) {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  if (tracer_id <= 0) {
    printf("get_current_time_tracer: incorrect tracer_id: %d\n", tracer_id);
  }
  sprintf(arg.cmd_buf, "%d", pid);
  return send_to_vt_module(VT_GETTIME_TRACER, &arg);
}


s64 get_current_vt_time() {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  return send_to_vt_module(VT_GETTIME_MY_PID, &arg);
}

int add_processes_to_tracer_sq(int tracer_id, int* pids, int num_pids) {
  if (tracer_id < 0 || num_pids <= 0) {
    printf("add_processes_to_tracer_sq: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  sprintf(arg.cmd_buf, "%d,", tracer_id);
  if (append_to_ioctl_arg(&arg, pids, num_pids) < 0) return -1;

  return send_to_vt_module(VT_ADD_PROCESSES_TO_SQ, &arg);
}

int add_to_tracer_sq(int tracer_id) {
  if (tracer_id < 0) {
    printf("add_to_tracer_sq: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  sprintf(arg.cmd_buf, "%d,", tracer_id);
  return send_to_vt_module(VT_ADD_TO_SQ, &arg);
}

int set_pkt_send_time(int pkt_hash, s64 send_tstamp) {
  if (pkt_hash < 0 || send_tstamp <= 0) {
	printf("set_pkt_send_time: incorrect parameters\n");
	return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  sprintf(arg.cmd_buf, "%d,", pkt_hash);
  arg.cmd_value = send_tstamp;
  return send_to_vt_module(VT_SET_PACKET_SEND_TIME, &arg);
}

s64 get_pkt_send_time(int tracer_id, int pkt_hash) {

  if (tracer_id <= 0 || pkt_hash < 0) {
    printf("get_pkt_send_time: incorrect parameters\n");
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  
  sprintf(arg.cmd_buf, "%d,%d", tracer_id, pkt_hash);
  return send_to_vt_module(VT_GET_PACKET_SEND_TIME, &arg);

}

int initializeExp(int num_expected_tracers) {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  if (num_expected_tracers < 0) {
    printf("initializeExp: num expected tracers: %d < 0 !\n",
          num_expected_tracers);
    return -1;
  }
  sprintf(arg.cmd_buf, "%d,0,%d", EXP_CBE, num_expected_tracers);
  return send_to_vt_module(VT_INITIALIZE_EXP, &arg);
}

int initialize_VT_Exp(int exp_type, int num_timelines,
                      int num_expected_tracers) {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  if (num_expected_tracers < 0) {
    printf("initialize_VT_Exp: num expected tracers: %d < 0 !\n",
          num_expected_tracers);
    return -1;
  }

  if (exp_type != EXP_CS && exp_type != EXP_CBE) {
    printf("initialize_VT_Exp: Incorrect Experiment type !\n");
    return -1;
  }

  if (num_timelines <= 0) {
    printf("initialize_VT_Exp: Number of timelines cannot be <= 0\n");
    return -1;
  }

  sprintf(arg.cmd_buf, "%d,%d,%d", exp_type, num_timelines,
                                   num_expected_tracers);

  return send_to_vt_module(VT_INITIALIZE_EXP, &arg);
}

int synchronizeAndFreeze() {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  return send_to_vt_module(VT_SYNC_AND_FREEZE, &arg);
}

int stopExp() {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  return send_to_vt_module(VT_STOP_EXP, &arg);
}

int progressBy(s64 duration, int num_rounds) {
  if (num_rounds <= 0)
     num_rounds = 1;
  ioctl_args arg;
  init_ioctl_arg(&arg);
  sprintf(arg.cmd_buf, "%d,", num_rounds);
  arg.cmd_value = duration;
  return send_to_vt_module(VT_PROGRESS_BY, &arg);
}


int progress_timeline_by(int timeline_id, s64 duration) {
  if (timeline_id < 0 || duration <= 0) {
    printf("progress_timeline_By: incorrect arguments !\n");
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  sprintf(arg.cmd_buf, "%d,", timeline_id);
  arg.cmd_value = duration;
  return send_to_vt_module(VT_PROGRESS_TIMELINE_BY, &arg);

}

int vt_sleep_for(s64 duration) {
  if (duration <= 0) {
    printf("vt_sleep_for: Duration must be positive !\n");
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  arg.cmd_value = duration;

  return send_to_vt_module(VT_SLEEP_FOR, &arg);
}

int release_worker() {
  ioctl_args arg;
  init_ioctl_arg(&arg); 
  return send_to_vt_module(VT_RELEASE_WORKER, &arg);
}

s64 finish_burst() {
  return write_tracer_results(NULL, 0);
}

s64 mark_burst_complete(int signal_syscall_finish) {

  ioctl_args arg;
  init_ioctl_arg(&arg); 

  if (signal_syscall_finish != 0)
    arg.cmd_value = 1;
  return send_to_vt_module(VT_SET_RUNNABLE, &arg);
}

s64 finish_burst_and_discard() {
  int my_pid = gettid();
  return write_tracer_results(&my_pid, 1);
}


s64 trigger_syscall_wait() {
  ioctl_args arg;
  init_ioctl_arg(&arg); 
  return send_to_vt_module(VT_SYSCALL_WAIT, &arg);
}
