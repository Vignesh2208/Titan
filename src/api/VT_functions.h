#ifndef __VT_FUNCTIONS
#define __VT_FUNCTIONS
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utility_functions.h"



// These functions can only be called from inside a tracer process or its
// children

//! Registers the calling process as a new tracer
/*!
    \param tracer_id ID of the tracer
    \param experiment_type EXP_CS or EXP_CBE
    \param optional_timeline_id If experiment type is EXP_CS, then this value
        refers to the timeline which should be associated with the tracer
*/
s64 RegisterTracer(int tracer_id, int experiment_type,
                   int optional_timeline_id);

//! Increments a tracer's virtual clock
/*!
    \param tracer_id ID of the tracer
    \param increment The clock is incremented by this value
*/
int UpdateTracerClock(int tracer_id, s64 increment);

//! Invoked by a tracee to signal to the virtual time module about processes which
//  should no longer be managed.
/*!
    \param pids Refers to list of process-ids which need to be ignored
    \param num_pids Number of process-ids
*/
s64 WriteTracerResults(int* pids, int num_pids);

//! Adds a list of pids to a tracer's schedule queue
/*!
    \param pids Refers to list of process-ids which need to be added
    \param num_pids Number of process-ids
*/
int AddProcessesToTracerSchQueue(int tracer_id, int* pids, int num_pids);

//! Adds the calling process/thread into a tracer's schedule queue
/*! \param tracer_id ID of the relevant tracer */
int AddToTracerSchQueue(int tracer_id);

// These functions can be called by the orchestrater script which may be in c,
// c++ or python

//! Returns the current virtual time of a process with specified pid
s64 GetCurrentTimePid(int pid);

//! Returns the current virtual time of a tracer with specified id
s64 GetCurrentTimeTracer(int tracer_id);

//! Returns the current virtual time of a tracer with id = 1
s64 GetCurrentVtTime(void);

//! Initializes a virtual time managed experiment
/*!
    \param exp_type EXP_CS or EXP_CBE
    \param num_timelines Number of timelines (in case of EXP_CS). Set to 0 for EXP_CBE 
    \param num_expected_tracers Number of tracers which would be spawned
*/
int InitializeVtExp(int exp_type, int num_timelines,
                    int num_expected_tracers);


//! Initializes a EXP_CBE experiment with specified number of expected tracers
int InitializeExp(int num_expected_tracers);

//! Synchronizes and Freezes all started tracers
int SynchronizeAndFreeze(void);

//! Initiates Stoppage of the experiment
int StopExp(void);

//! Only to be used for EXP_CBE type experiment. Instructs the experiment to
//  be run for the specific number of rounds where each round signals advancement
//  of virtual time by the specified duration
int ProgressBy(s64 duration, int num_rounds);

//! Advance a EXP_CS timeline by the specified duration. May be used by a network
//  simulator like S3FNet using composite synchronization mechanism
int ProgressTimelineBy(int timeline_id, s64 duration);

//! Invoked by the tracer to wait until completion of the virtual time
//  experiment
int WaitForExit(int tracer_id);

//! Makes the calling process sleep for specified duration of virtual time
int VtSleepFor(s64 duration);

//! Invoked by a tracee if it is about to enter a syscall which may yield the cpu
int ReleaseWorker(void);

//! Invoked by a tracee to signal to the virtual time module that it finished its
//  previous execution burst and is now available to be run again
s64 FinishBurst(void);

//! Invoked by a tracee to signal to the virtual time module that it finished its
//  previous execution burst and can be ignored and removed from its tracer
//  schedule queue
s64 FinishBurstAndDiscard(void);

//! Invoked by a tracee to signal to the virtual time module to signal that it
//  is now available to be run again
s64 MarkBurstComplete(int signal_syscall_finish);

//! Invoked by a tracee to signal a virtual time accorded wait for syscalls such
//  as select, poll etc
s64 TriggerSyscallWaitAPI(void);

//! Sets the packet send time before the packet is sent out
int SetPktSendTimeAPI(int payload_hash, int payload_len, s64 send_tstamp);


//! Gets the packet send time given its hash. This API could be used by a network
//  simulator
s64 GetPktSendTimeAPI(int tracer_id, int pkt_hash);

//! Returns the number of bytes sent by a tracer. This API could be used by a
//  network simulator
int GetNumEnqueuedBytes(int tracer_id);

//! Sets the earliest arrival time for a tracer. This API could be used by a
//  network simulator
int SetEarliestArrivalTime(int tracer_id, s64 eat_tstamp);

//! Invoked by a tracee to get the earliest time at which a packet may arrive
//  at this node. This may be used to compute lookahead by the tracee
s64 GetEarliestArrivalTime(void);

//! Invoked by a tracee to set its lookahead. The lookahead of a node is
//  computed as the min of lookaheads of all its tracees
int SetProcessLookahead(s64 bulk_lookahead_expiry_time, int lookahead_anchor_type);

//! Returns the lookahead of a node. This API could be used by a network simulator
//  Returns absolute timestamp.
s64 GetTracerLookahead(int tracer_id);

//! Returns the lookahead of a node while excluding any tasks/threads which are
//  blocked on a packet receive syscall. Returns 0 if all threads are blocked
//  on packet receive.
s64 GetTracerNEATLookahead(int tracer_id);

#endif
