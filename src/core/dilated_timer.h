

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

struct hrtimer_dilated;
struct hrtimer_dilated_clock_base;
struct dilated_timer_timeline_base;

struct hrtimer_dilated {
	struct timerqueue_node node;
	ktime_t _softexpires;
	enum hrtimer_restart (*function)(struct hrtimer_dilated *);
	struct hrtimer_dilated_clock_base *base;
	u8 active;
	u8 is_rel;
	u8 state;
};

struct dilated_hrtimer_sleeper {
	struct hrtimer_dilated timer_dilated;
	struct task_struct * task;
};


struct hrtimer_dilated_clock_base {
	struct dilated_timer_timeline_base * timeline_base;
	int clock_active; // indicates whether dilated clock is active i.e whether
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


void init_global_dilated_timer_timeline_bases(int total_num_timelines);
void free_global_dilated_timer_timeline_bases(int total_num_timelines);
int dilated_hrtimer_cancel(struct hrtimer_dilated *timer);
int dilated_hrtimer_try_to_cancel(struct hrtimer_dilated *timer);
void dilated_hrtimer_start(struct hrtimer_dilated *timer, ktime_t expiry_time,
                            const enum hrtimer_mode mode);
int dilated_hrtimer_init(struct hrtimer_dilated *timer, int timeline_id,
                         enum hrtimer_mode mode);
int dilated_hrtimer_forward(struct hrtimer_dilated *timer, ktime_t interval);
void dilated_hrtimer_start_range_ns(struct hrtimer_dilated *timer,
                                    ktime_t expiry_time,
                                    const enum hrtimer_mode mode);
int dilated_hrtimer_sleep(ktime_t duration);
void dilated_hrtimer_run_queues_flush(int timeline_id);
void dilated_hrtimer_run_queues(int timeline_id);
#endif