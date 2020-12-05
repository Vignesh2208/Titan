#ifndef __GENERAL_COMMANDS_H
#define __GENERAL_COMMANDS_H

#include "vt_module.h"


//! Handles any updates to make to a specific tracer's virtual clock
int HandleVtUpdateTracerClock(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Handles any results reported by a tracee after it completes its run each round
/*!
    \param arg A pointer to a string buffer containing status results returned by a
        tracee after it completes a run
    \param dilated_task A pointer to a structure containing virtual time related
        information about the tracee
*/
int HandleVtWriteResults(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Initiates registration of a new tracer
/*!
    \param arg A pointer to invoked_api struct
    \param dilated_task For a new registration, this would be NULL. It it is
        not NULL, then this call does nothing
*/
int HandleVtRegisterTracer(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Adds a new tracee to the schedule queue of a tracer
/*!
    \param arg A pointer to invoked_api struct
    \param dilated_task Points a tracee. It is used to identify the relavant
        tracer
*/
int HandleVtAddProcessesToSchQueue(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Handles synchronize and freeze operation
int HandleVtSyncFreeze();

//! Handles experiment initialization
/*! \param arg A pointer to invoked_api struct */
int HandleVtInitializeExp(unsigned long arg);

//! Returns the current virtual time associated with a pid
/*! \param arg A pointer to invoked_api struct containing the pid in question */
int HandleVtGettimePid(unsigned long arg);

//! Returns the current virtual time associated with a tracer
/*!
    \param arg A pointer to invoked_api struct containing details of the
        tracer in question
*/
int HandleVtGettimeTracer(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Handles stopping the virtual time managed experiment
int HandleVtStopExp();

//! Handles progressing the whole virtual time experiment by a specified duration
/*!
    \param arg A pointer to invoked_api struct containing details of the
        duration to advance by
*/
int HandleVtProgressBy(unsigned long arg);

//! Handles advancing a specific timeline/cpu worker thread by a specified duration
/*!
    \param arg A pointer to invoked_api struct containing details of the
        duration to advance by
*/
int HandleVtProgressTimelineBy(unsigned long arg);

//! Invoked by the main tracer process to wait until the experiment finishes
/*!
    \param arg A pointer to invoked_api struct containing details of the
        id of the invoking tracer
    \param dilated_task Must be NULL, if not then this function does nothing
*/
int HandleVtWaitForExit(
    unsigned long arg, struct dilated_task_struct * dilated_task);


//! Make a tracee sleep for a specified duration in virtual time
/*!
    \param arg A pointer to invoked_api struct containing details of
        sleep duration
    \param dilated_task Represents the tracee to be put to sleep
*/
int HandleVtSleepFor(
    unsigned long arg, struct dilated_task_struct * dilated_task); 

//! Resumes a blocked timeline/cpu worker
/*!
    \param arg Uused
    \param dilated_task Represents the tracee which invoked this call
*/
int HandleVtReleaseWorker(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Invoked by a tracee and marks the specific tracee as runnable and waits until it needs to run
/*!
    \param arg A pointer to invoked_api struct which upon return
        would contain amount of duration to run for
    \param dilated_task Represents the tracee which invokes this call
*/
int HandleVtSetRunnable(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Handles gettime pid to return the current virtual time for the calling process
/*!
    \param arg A pointer to invoked_api struct which upon return
        would contain amount the calling process's current virtual time
*/
int HandleVtGettimeMyPid(unsigned long arg);

//! Adds the invoking process to a tracer's schedule queue
/*!
    \param arg A pointer to invoked_api struct containing details of the tracer
        in question
*/
int HandleVtAddToSchQueue(unsigned long arg);

//! Invoked by a tracee when making a time-related syscall such as poll, select, sleep
/*!
    \param arg A pointer to invoked_api struct which would hold return value
        of success/failure
    \param dilated_task Represents the relevant tracee and implicitly, the
        relevant tracer
*/
int HandleVtSyscallWait(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Sets the send time of a packet
/*!
    \param arg A pointer to invoked_api struct containing a hash of the payload
        of the packet and its length
    \param dilated_task Represents the tracee which sent the packet
*/
int HandleVtSetPacketSendTime(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Gets the send time of a packet
/*!
    \param arg A pointer to invoked_api struct containing a hash of the payload
        of a packet
*/
int HandleVtGetPacketSendTime(unsigned long arg);

//! Returns total number of enqueued bytes (sum of packet lengths) sent from a
//  tracer (i.e its tracees) so far
/*!
    \param arg A pointer to invoked_api struct containing details of the tracer
*/
int HandleVtGetNumQueuedBytes(unsigned long arg);

//! Sets the earliest arrival time for a specific tracer (used in lookahead estimation)
/*!
    \param arg A pointer to invoked_api struct containing details of the tracer
*/
int HandleVtSetEat(unsigned long arg);

//! Returns the earlies arrival for a specific tracer (used in lookahead estimation)
/*!
    \param arg A pointer to invoked_api struct to hold return value
    \param dilated_task Imlicitly represents the tracer in question

*/
int HandleVtGetEat(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Sets lookahead for a tracee (used by the network simulator)
/*!
    \param arg A pointer to invoked_api struct containing lookahead value
    \param dilated_task Represents the tracee in question.
*/
int HandleVtSetProcessLookahead(
    unsigned long arg, struct dilated_task_struct * dilated_task);

//! Invoked by the network simulator to get minimum lookahead among all tracees of a tracer
/*!
    \param arg A pointer to invoked_api struct containing details of the tracer.
        Upon return it would hold the lookahead value
    \param ignore_eat_anchors If set to 1, all tasks whose lookahead are tied to
    earliest arrival times are ignored when computing the lookahead. In other words
    it only returns lookahead selectively from tasks which are not blocked on a packet
    receive.
*/
int HandleVtGetTracerLookahead(unsigned long arg, int ignore_eat_anchors);


 
//! Marks a tcp stack as active
/*!
    \param arg A pointer to invoked_api struct containing details of the processID
        associated with this stack
*/
int HandleVtMarkStackActive(unsigned long arg);


//! Marks a tcp stack as in-active
/*!
    \param arg A pointer to invoked_api struct containing details of the processID
        associated with this stack
*/
int HandleVtMarkStackInActive(unsigned long arg);



//! Marks a tcp stack rx-loop as complete in this round
/*!
    \param arg A pointer to invoked_api struct containing details of the processID
        associated with this stack
*/
int HandleVtMarkStackRxLoopActive(unsigned long arg);


//! Make thread stack wait until it needs to be woken-up 
/*!
    \param arg A pointer to invoked_api struct containing details of the processID
        associated with this stack
*/
int HandleVtThreadStackWait(unsigned long arg);



//! Updates a process's tcp stack's latest next re-transmit time
/*!
    \param arg A pointer to invoked_api struct containing tracerID, processID
        and latest next re-transmit time
*/
int HandleVtUpdateStackRtxSendTime(unsigned long arg);

#endif

