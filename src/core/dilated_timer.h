#ifndef __DILATED_TIMER_H
#define __DILATED_TIMER_H

#include <linux/rbtree.h>
#include <linux/ktime.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/percpu.h>
#include <linux/timer.h>
#include <linux/timerqueue.h>

/**
 This file describes API provided to enqueue/dequeue virtual time accorded hrtimers
 Only a subset of this API is currently used though. There are plans for further extensions
**/

struct hrtimer_dilated;
struct hrtimer_dilated_clock_base;
struct dilated_timer_timeline_base;
struct dilated_task_struct;

struct hrtimer_dilated {
	struct timerqueue_node node;
	ktime_t _softexpires;
	enum hrtimer_restart (*function)(struct hrtimer_dilated *);
	struct hrtimer_dilated_clock_base *base;
	u8 active;
	u8 is_rel;
	u8 state;
};

struct DilatedHrtimerSleeper {
	struct hrtimer_dilated timer_dilated;
	struct dilated_task_struct * associated_dilated_task_struct;
	struct task_struct * task;
};


struct hrtimer_dilated_clock_base {
	struct dilated_timer_timeline_base * timeline_base;
	int clock_active;// indicates whether dilated clock is active i.e whether
                     // VT experiment is running. If not new dilated hrtimer
                     // init should fail and existing hrtimer callbacks should
                     // end.
	struct timerqueue_head active;
};


struct dilated_timer_timeline_base {
	spinlock_t	lock;
	int timeline_id;
	int total_num_timelines;
	s64 curr_virtual_time;
	struct hrtimer_dilated_clock_base dilated_clock_base;
	ktime_t nxt_dilated_expiry;
	struct hrtimer_dilated *running_dilated_timer;
} ;


//! Initializes all dilated timer bases (one for each timeline/CPU worker)
void InitGlobalDilatedTimerTimelineBases(int total_num_timelines);

//! Frees all allotted dilated timer bases (one for each timeline/CPU worker)
void FreeGlobalDilatedTimerTimelineBases(int total_num_timelines);

//! Cancels an already queued dilated hrtimer
/*! \param timer Represents the dilated hrtimer to cancel */
int DilatedTimerCancel(struct hrtimer_dilated *timer);

//! Try to cancel an existing dilated hrtimer
/*! \param timer Represents the dilated hrtimer to cancel */
int DilatedHrtimerTryToCancel(struct hrtimer_dilated *timer);

//! Enqueue a new dilated hrtimer
/*!
	\param timer Represents the hrtimer to enqueue
	\param expiry_time Represents the virtual time at which this timer should
		expire
	\param mode Represents whether the expiry time is an absolute virtual
		timestamp or whether it is specified relative to current virtual time 
*/
void DilatedHrtimerStart(struct hrtimer_dilated *timer, ktime_t expiry_time,
	const enum hrtimer_mode mode);

//! Initializes a new hrtimer on the specified timeline's base
/*!
	\param timer Represents the timer to initialize
	\param timeline_id Represents the base to add this timer to
	\param mode Represents the default expiry mode (ABS OR REL)
*/
int DilatedHrtimerInit(struct hrtimer_dilated *timer, int timeline_id,
	enum hrtimer_mode mode);

//! Changes the expiry time of an enqueued hrtimer
/*!
	\param timer Represents the relevant timer
	\param interval Represents the time to advance by
*/
int DilatedHrtimerForward(struct hrtimer_dilated *timer, ktime_t interval);

//! Similar to dilater_hrtimer_start
void DilatedHrtimerStartRangeNs(struct hrtimer_dilated *timer,
	ktime_t expiry_time, const enum hrtimer_mode mode);

//! Makes the calling process sleep for specified duration in virtual time
/*!
	\param duration Duration to sleep in NS
	\param dilated_task A wrapper which provides links to the virtual time
		related information associated with the calling task
*/
int DilatedHrtimerSleep(
	ktime_t duration, struct dilated_task_struct * dilated_task);


//! Dequeues all dilated timers initializes/queued on a specific timeline's base
/*! \param timeline_id ID of the associated timeline */
void DilatedHrtimerRunQueuesFlush(int timeline_id);

// Invokes the callback functions associated with any timers which may have expired
/*! \param timeline_id ID of the associated timeline */
void DilatedHrtimerRunQueues(int timeline_id);

//! Returns the base associated with a timeline of interest
/*! \param timeline_id ID of the associated timeline */
struct dilated_timer_timeline_base *get_timeline_base(int timeline_id);

//! Sets the timeline base's clock to the specified value
/*!
	\param timeline_id ID of the associated timeline
	\param time_to_set The timeline base's clock is set to this value
*/
void SetTimerbaseClock(int timeline_id, ktime_t time_to_set);

//! Increments the timeline base's clock by the specified value
/*!
	\param timeline_id ID of the associated timeline
	\param inc The timeline base's clock is incremented by this value
*/
void IncTimerbaseClock(int timeline_id, ktime_t inc);

#endif
