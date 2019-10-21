
#include "VT_functions.h"
#include <sys/ioctl.h>
#include "utility_functions.h"

int register_tracer(int tracer_id, int tracer_type, int registration_type,
                    int optional_pid) {
  ioctl_args arg;
  if (tracer_id < 0 || optional_pid < 0 ||
      (tracer_type != TRACER_TYPE_INS_VT &&
       tracer_type != TRACER_TYPE_APP_VT) ||
      (registration_type < SIMPLE_REGISTRATION ||
       registration_type > REGISTRATION_W_CONTROL_THREAD)) {
    print("Tracer registration: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  init_ioctl_arg(&arg);
  if (registration_type == SIMPLE_REGISTRATION) {
    sprintf(arg.cmd_buf, '%d,%d,%d', tracer_id, tracer_type,
            SIMPLE_REGISTRATION);
  } else {
    sprintf(arg.cmd_buf, '%d,%d,%d,%d', tracer_id, tracer_type,
            registration_type, optional_pid);
  }
  return send_to_vt_module(VT_REGISTER_TRACER, &arg);
}
int update_tracer_clock(int tracer_id, s64 increment) {
  if (tracer_id < 0 || increment < 0) {
    print("Update tracer clock: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  sprintf(arg.cmd_buf, '%d', tracer_id);
  arg.cmd_value = increment;
  return send_to_vt_module(VT_UPDATE_TRACER_CLOCK, &arg);
}
int write_tracer_results(int tracer_id, int* results, int num_results) {
  if (tracer_id < 0 || num_results < 0) {
    print("Write tracer results: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);

  if (num_results == 0)
    sprintf(arg.cmd_buf, '%d', tracer_id);
  else {
    sprintf(arg.cmd_buf, '%d,', tracer_id);
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
    print("get_current_time_pid: incorrect pid: %d\n", pid);
  }
  sprintf(arg.cmd_buf, '%d', pid);
  return send_to_vt_module(VT_GETTIME_PID, &arg);
}

int add_processes_to_tracer_sq(int tracer_id, int* pids, int num_pids) {
  if (tracer_id < 0 || num_pids <= 0) {
    print("add_processes_to_tracer_sq: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  init_ioctl_arg(&arg);
  sprintf(arg.cmd_buf, '%d,', tracer_id);
  if (append_to_ioctl_arg(&arg, pids, num_pids) < 0) return -1;

  return send_to_vt_module(VT_ADD_PROCESSES_TO_SQ, &arg);
}

int initializeExp(int num_expected_tracers) {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  if (num_expected_tracers < 0) {
    print("initializeExp: num expected tracers: %d < 0 !\n",
          num_expected_tracers);
    return -1;
  }
  sprintf(arg.cmd_buf, '%d', num_expected_tracers);
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

int progressBy(s64 duration) {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  arg.cmd_value = duration;
  return send_to_vt_module(VT_PROGRESS_BY, &arg);
}

int set_netdevice_owner(int tracer_id, char* intf_name) {
  ioctl_args arg;
  init_ioctl_arg(&arg);
  if (tracer_id < 0 || !intf_name || strlen(intf_name) > IFNAMESIZ) {
    print("Set net device owner: incorrect arguments for tracer: %d!\n",
          tracer_id);
    return -1;
  }
  sprintf(arg.cmd_buf, '%d,%s', tracer_id, intf_name);
  return send_to_vt_module(VT_SET_NETDEVICE_OWNER, &arg);
}