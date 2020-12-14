#include "VT_functions.h"
#include <sys/ioctl.h>
#include "utility_functions.h"

s64 RegisterTracer(int tracer_id, int experiment_type,
                    int optional_timeline_id) {
  ioctl_args arg;
  if (tracer_id < 0 || optional_timeline_id < 0 || 
      (experiment_type != EXP_CBE && experiment_type != EXP_CS)) {
    printf("Tracer registration: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  InitIoctlArg(&arg);
  if (experiment_type == EXP_CBE) {
    sprintf(arg.cmd_buf, "%d,%d,0", tracer_id, experiment_type);
  } else {
    sprintf(arg.cmd_buf, "%d,%d,%d", tracer_id, experiment_type,
            optional_timeline_id);
  }
  return SendToVtModule(VT_REGISTER_TRACER, &arg);
}
int UpdateTracerClock(int tracer_id, s64 increment) {
  if (tracer_id < 0 || increment < 0) {
    printf("Update tracer clock: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d", tracer_id);
  arg.cmd_value = increment;
  return SendToVtModule(VT_UPDATE_TRACER_CLOCK, &arg);
}

int WaitForExit(int tracer_id) {
  if (tracer_id < 0) {
    printf("Update tracer clock: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d", tracer_id);
  arg.cmd_value = 0;
  return SendToVtModule(VT_WAIT_FOR_EXIT, &arg);
}
s64 WriteTracerResults(int* results, int num_results) {
  if (num_results < 0) {
    printf("Write tracer results: incorrect parameters\n");
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);

  if (num_results > 0) {
    if (AppendToIoctlArg(&arg, results, num_results) < 0) return -1;
  }
  return SendToVtModule(VT_WRITE_RESULTS, &arg);
}


s64 GetCurrentTimePid(int pid) {
  ioctl_args arg;
  InitIoctlArg(&arg);
  if (pid < 0) {
    printf("GetCurrentTimePid: incorrect pid: %d\n", pid);
  }
  sprintf(arg.cmd_buf, "%d,", pid);
  return SendToVtModule(VT_GETTIME_PID, &arg);
}

s64 GetCurrentTimeTracer(int tracer_id) {
  ioctl_args arg;
  InitIoctlArg(&arg);
  if (tracer_id <= 0) {
    printf("GetCurrentTimeTracer: incorrect tracer_id: %d\n", tracer_id);
  }
  sprintf(arg.cmd_buf, "%d,", tracer_id);
  return SendToVtModule(VT_GETTIME_TRACER, &arg);
}


int GetNumEnqueuedBytes(int tracer_id) {
  ioctl_args arg;
  InitIoctlArg(&arg);
  if (tracer_id <= 0) {
    printf("get_num_enqueued_packets: incorrect tracer_id: %d\n", tracer_id);
  }
  sprintf(arg.cmd_buf, "%d,", tracer_id);
  return SendToVtModule(VT_GET_NUM_ENQUEUED_BYTES, &arg);
}


s64 GetCurrentVtTime() {
  ioctl_args arg;
  InitIoctlArg(&arg);
  return SendToVtModule(VT_GETTIME_MY_PID, &arg);
}

int AddProcessesToTracerSchQueue(int tracer_id, int* pids, int num_pids) {
  if (tracer_id < 0 || num_pids <= 0) {
    printf("AddProcessesToTracerSchQueue: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,", tracer_id);
  if (AppendToIoctlArg(&arg, pids, num_pids) < 0) return -1;

  return SendToVtModule(VT_ADD_PROCESSES_TO_SQ, &arg);
}

int AddToTracerSchQueue(int tracer_id) {
  if (tracer_id < 0) {
    printf("AddToTracerSchQueue: incorrect parameters for tracer: %d\n",
          tracer_id);
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,", tracer_id);
  return SendToVtModule(VT_ADD_TO_SQ, &arg);
}

int SetPktSendTimeAPI(int payload_hash, int payload_len, s64 send_tstamp) {
  if (payload_hash < 0 || send_tstamp <= 0) {
  printf("SetPktSendTimeAPI: incorrect parameters\n");
  return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,%d,", payload_hash, payload_len);
  arg.cmd_value = send_tstamp;
  //printf("Invoking VT_SET_PACKET_SEND_TIME for hash: %d, tstamp: %llu\n", payload_hash, send_tstamp); 
  return SendToVtModule(VT_SET_PACKET_SEND_TIME, &arg);
}

s64 GetPktSendTimeAPI(int tracer_id, int pkt_hash) {

  if (tracer_id <= 0 || pkt_hash < 0) {
    printf("GetPktSendTimeAPI: incorrect parameters\n");
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  
  sprintf(arg.cmd_buf, "%d,%d", tracer_id, pkt_hash);
  return SendToVtModule(VT_GET_PACKET_SEND_TIME, &arg);

}

int InitializeExp(int num_expected_tracers) {
  ioctl_args arg;
  InitIoctlArg(&arg);
  if (num_expected_tracers < 0) {
    printf("InitializeExp: num expected tracers: %d < 0 !\n",
          num_expected_tracers);
    return -1;
  }
  sprintf(arg.cmd_buf, "%d,0,%d", EXP_CBE, num_expected_tracers);
  return SendToVtModule(VT_INITIALIZE_EXP, &arg);
}

int InitializeVtExp(int exp_type, int num_timelines,
                    int num_expected_tracers) {
  ioctl_args arg;
  InitIoctlArg(&arg);
  if (num_expected_tracers < 0) {
    printf("InitializeVtExp: num expected tracers: %d < 0 !\n",
          num_expected_tracers);
    return -1;
  }

  if (exp_type != EXP_CS && exp_type != EXP_CBE) {
    printf("InitializeVtExp: Incorrect Experiment type !\n");
    return -1;
  }

  if (num_timelines <= 0) {
    printf("InitializeVtExp: Number of timelines cannot be <= 0\n");
    return -1;
  }

  sprintf(arg.cmd_buf, "%d,%d,%d", exp_type, num_timelines,
                                   num_expected_tracers);

  return SendToVtModule(VT_INITIALIZE_EXP, &arg);
}

int SynchronizeAndFreeze() {
  ioctl_args arg;
  InitIoctlArg(&arg);
  return SendToVtModule(VT_SYNC_AND_FREEZE, &arg);
}

int StopExp() {
  ioctl_args arg;
  InitIoctlArg(&arg);
  return SendToVtModule(VT_STOP_EXP, &arg);
}

int ProgressBy(s64 duration, int num_rounds) {
  if (num_rounds <= 0)
     num_rounds = 1;
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,", num_rounds);
  arg.cmd_value = duration;
  return SendToVtModule(VT_PROGRESS_BY, &arg);
}


int ProgressTimelineBy(int timeline_id, s64 duration) {
  if (timeline_id < 0 || duration <= 0) {
    printf("progress_timeline_By: incorrect arguments !\n");
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,", timeline_id);
  arg.cmd_value = duration;
  return SendToVtModule(VT_PROGRESS_TIMELINE_BY, &arg);

}

int VtSleepFor(s64 duration) {
  if (duration <= 0) {
    printf("VtSleepFor: Duration must be positive !\n");
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  arg.cmd_value = duration;

  return SendToVtModule(VT_SLEEP_FOR, &arg);
}

int ReleaseWorker() {
  ioctl_args arg;
  InitIoctlArg(&arg); 
  return SendToVtModule(VT_RELEASE_WORKER, &arg);
}

s64 FinishBurst() {
  return WriteTracerResults(NULL, 0);
}

s64 MarkBurstComplete(int signal_syscall_finish) {

  // signal = 1 indicates finish of syscall like poll/select/sleep
  ioctl_args arg;
  InitIoctlArg(&arg); 

  if (signal_syscall_finish != 0)
    arg.cmd_value = 1;
  return SendToVtModule(VT_SET_RUNNABLE, &arg);
}

s64 FinishBurstAndDiscard() {
  int my_pid = gettid();
  return WriteTracerResults(&my_pid, 1);
}


s64 TriggerSyscallWaitAPI() {
  ioctl_args arg;
  InitIoctlArg(&arg); 
  return SendToVtModule(VT_SYSCALL_WAIT, &arg);
}

int SetEarliestArrivalTime(int tracer_id, s64 eat_tstamp) {

  if (tracer_id <= 0) {
    printf("SetEarliestArrivalTime: incorrect tracer-id\n");
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  arg.cmd_value = eat_tstamp;
  sprintf(arg.cmd_buf, "%d", tracer_id);

  return SendToVtModule(VT_SET_EAT, &arg);

}
s64 GetEarliestArrivalTime() {
  ioctl_args arg;
  InitIoctlArg(&arg);
  return SendToVtModule(VT_GET_EAT, &arg);
}

int SetProcessLookahead(s64 bulk_lookahead_expiry_time, int lookahead_anchor_type) {
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d", lookahead_anchor_type);
  if (bulk_lookahead_expiry_time < 0)
     bulk_lookahead_expiry_time = 0;
  arg.cmd_value = bulk_lookahead_expiry_time;
  return SendToVtModule(VT_SET_PROCESS_LOOKAHEAD, &arg);
}

s64 GetTracerLookahead(int tracer_id) {

  if (tracer_id <= 0) {
    printf("GetTracerLookahead: incorrect tracer-id\n");
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d", tracer_id);

  return SendToVtModule(VT_GET_TRACER_LOOKAHEAD, &arg);
}


s64 GetTracerNEATLookahead(int tracer_id) {

  if (tracer_id <= 0) {
    printf("GetTracerNEATLookahead: incorrect tracer-id\n");
    return -1;
  }
  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d", tracer_id);
  return SendToVtModule(VT_GET_TRACER_NEAT_LOOKAHEAD, &arg);
}


int MarkStackActive(int tracerID, int stackID) {
  if (tracerID <= 0 || stackID <= 0) {
    printf ("MarkStackActive: invalid arguments !\n");
    return -1;
  }

  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,%d", tracerID, stackID);
  return SendToVtModule(VT_MARK_STACK_ACTIVE, &arg);

}

int MarkStackInActive(int tracerID, int stackID) {
    if (tracerID <= 0 || stackID <= 0) {
    printf ("MarkStackInActive: invalid arguments !\n");
    return -1;
  }

  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,%d", tracerID, stackID);
  return SendToVtModule(VT_MARK_STACK_INACTIVE, &arg);

}

int MarkStackRxLoopComplete(int tracerID, int stackID) {
    if (tracerID <= 0 || stackID <= 0) {
    printf ("MarkStackRxLoopComplete: invalid arguments !\n");
    return -1;
  }

  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,%d", tracerID, stackID);
  return SendToVtModule(VT_MARK_STACK_RXLOOP_COMPLETE, &arg);

}

s64 TriggerStackThreadWait(int tracerID, int stackID) {
  if (tracerID <= 0 || stackID <= 0) {
    printf ("TriggerStackThreadWait: invalid arguments !\n");
    return -1;
  }

  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,%d", tracerID, stackID);
  return SendToVtModule(VT_THREAD_STACK_WAIT, &arg);
}


int TriggerStackThreadFinish(int tracerID, int stackID) {
  if (tracerID <= 0 || stackID <= 0) {
    printf ("TriggerStackThreadWait: invalid arguments !\n");
    return -1;
  }

  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,%d", tracerID, stackID);
  return SendToVtModule(VT_THREAD_STACK_FINISH, &arg);
}


int UpdateStackSendRtxTime(int tracerID, int stackID, s64 stack_send_rtx_time) {
  if (tracerID <= 0 || stackID <= 0 || stack_send_rtx_time < 0) {
    printf ("UpdateStackSendRtxTime: invalid arguments !\n");
    return -1;
  }

  ioctl_args arg;
  InitIoctlArg(&arg);
  sprintf(arg.cmd_buf, "%d,%d", tracerID, stackID);
  arg.cmd_value = stack_send_rtx_time;
  return SendToVtModule(VT_UPDATE_STACK_RTX_SEND_TIME, &arg);
}
