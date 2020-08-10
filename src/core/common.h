#ifndef __COMMON_H
#define __COMMON_H

#include "includes.h"
#include "vt_module.h"

//! This file defines some commonly used functions for virtual time management

// ! Returns task with pid in the namespace corresponding to tsk
/*!
    \param pid pid of to search for
    \param tsk Search in the namespace of this task
*/
struct task_struct* GetTaskNs(pid_t pid, struct task_struct * tsk);

//! Removes the first entry in a tracer's schedule queue
/*! \param tracer_entry Represents the relevant tracer */
int PopScheduleList(tracer * tracer_entry);

//!  Removes the first entry from a tracer's schedule queue and appends it to the end
/*! \param tracer_entry Represents the relevant tracer */
void RequeueScheduleList(tracer * tracer_entry);

//! Removes the first entry in a tracer's run queue and returns it
/*! \param tracer_entry Represents the relevant tracer */
lxc_schedule_elem * PopRunQueue(tracer * tracer_entry);

//! Removes the first entry from a tracer's run queue and appends it to the end
/*! \param tracer_entry Represents the relevant tracer */
void RequeueRunQueue(tracer * tracer_entry);

//! Removes all entries in a tracer's schedule list
/*! \param tracer_entry Represents the relevant tracer */
void CleanUpScheduleList(tracer * tracer_entry);

//! Removes all entries in a tracer's run queue
/*! \param tracer_entry Represents the relevant tracer */
void CleanUpRunQueue(tracer * tracer_entry);

//! Returns the size of a tracer's schedule queue
/*! \param tracer_entry Represents the relevant tracer */
int ScheduleListSize(tracer * tracer_entry);

//! Returns the size of a tracer's run queue
/*! \param tracer_entry Represents the relevant tracer */
int RunQueueSize(tracer * tracer_entry);

//! Adds a task to a tracer's schedule queue to make it available for scheduling
/*!
    \param tracer_entry Represents the relevant tracer
    \param tracee Represens a new task to be added to a tracer's schedule queue
*/
void AddToTracerScheduleQueue(tracer * tracer_entry,
    struct task_struct * tracee);


//! Adds information about a packet sent from a tracer
/*!
    \param tracer_entry Represents the relevant tracer
    \param payload_hash A hash of the payload of the packet
    \param payload_len Length of the packet's payload
    \param pkt_send_tstamp Virtual time at which packet was sent by the application
*/
void AddToPktInfoQueue(tracer * tracer_entry, int payload_hash,
                           int payload_len, s64 pkt_send_tstamp);

//! Cleans up information about all packet sends initiated in the previous round
/*! \param tracer_entry Represents the relevant tracer */
void CleanUpPktInfoQueue(tracer * tracer_entry);

//! Returns the sum of payload sizes of all packet sends initiated at this tracer
/*! \param tracer_entry Represents the relevant tracer */
int GetNumEnqueuedBytes(tracer * tracer_entry);

//! Returns the packet send timestamp corresponding to a payload hash
/*!
    \param tracer_entry Represents the relevant tracer to look into
    \param payload_hash Represents the hash of the payload of the interested packet
*/
s64 GetPktSendTstamp(tracer * tracer_entry, int payload_hash);

//! Removes a  specific tracee from a tracer's schedule queue
/*!
    \param tracer_entry Represents the relevant tracer
    \param tracee_pid Represents the pid of the tracee to remove
*/
void RemoveFromTracerScheduleQueue(tracer * tracer_entry, int tracee_pid);

//! Registers the process which makes this call as a new tracer
/*!
    \param write_buffer The process specifies some parameters in a string buffer
*/
int RegisterTracerProcess(char * write_buffer);

//! Increments the current virtual time of all tracees added to a tracer's schedule queue
/*!
    \param tracer_entry Represents the relevant tracer
    \param time_increment Represents amount of increment in virtual time
*/
void UpdateAllChildrenVirtualTime(tracer * tracer_entry, s64 time_increment);


//! Updates the virtual time of all tracers aligned on a given cpu
/*! \param cpuID The ID of the relvant cpu to look into */
void UpdateAllTracersVirtualTime(int cpuID);

//! Invoked by a tracee to signal completion of its run
// The tracee may signal whether it wishes to be ignored/removed from a tracer's
// schedule queue as well
/*!
    \param curr_tracer Points to the tracer which is responsibe for the tracee
        making this call
    \param api_args Represents a list of pids which must be removed from a tracer's
        virtual schedule queue. A tracee may specify its own pid here as well
    \param num_args Represents the number of pids included in api_args
*/
int HandleTracerResults(tracer * curr_tracer, int * api_args, int num_args);

//! Handles stopping of the virtual time controlled experiment
int HandleStopExpCmd();

//! Handles initialization of the virtual time controlled experiment
/*!
    \param exp_type Type of the experiment EXP_CS or EXP_CBE
    \param num_timelines Number of timeline workers (in case of EXP_CS) or
        number of CPU workers (in case of EXP_CBE)
    \param num_expected_tracers Number of tracers which are expected to register
        before the experiment can start
*/ 
int HandleInitializeExpCmd(
    int exp_type, int num_timelines, int num_expected_tracers);

//! Returns the current virtual time of the task in question
s64 GetCurrentVirtualTime(struct task_struct * task) ;

//! Returns the current virtual time of a pid specified in a string buffer
/*!
    \param write_buffer A string buffer which contains the interested process's
        pid
*/
s64 HandleGettimepid(char * write_buffer);

//! Blocks the caller process until a tracee executes its current run
/*!
    \param curr_tracer Represents the tracer whose tracee is currently running
    \param relevant_task Represents the interested tracee which is currently
        running
*/
void WaitForTaskCompletion(tracer * curr_tracer, 
    struct task_struct * relevant_task);

//! Signals a timeline thread/cpu worker responsible for a tracer to resume execution
/*! \param curr_tracer Represents the relvant tracer is question */
void SignalCpuWorkerResume(tracer * curr_tracer);

//! Wakes up all tracees on a timeline which may be in the middle of a virtual time accorded syscall
/*! \param timelineID Represents the ID of the timeline in question */
void WakeUpProcessesWaitingOnSyscalls(int timelineID);


#endif
