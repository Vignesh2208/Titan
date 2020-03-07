

#include <linux/cpu.h>
#include <linux/export.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/notifier.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/interrupt.h>
#include <linux/tick.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/debugobjects.h>
#include <linux/sched.h>
#include <linux/sched/sysctl.h>
#include <linux/sched/rt.h>
#include <linux/sched/deadline.h>
#include <linux/timer.h>
#include <linux/freezer.h>
#include <linux/slab.h> 
#include <linux/spinlock.h>
#include <asm/uaccess.h>

#include "includes.h"
#include "vt_module.h"
#include "dilated_timer.h"


extern struct dilated_timer_timeline_base ** global_dilated_timer_timeline_bases;
extern hashmap get_dilated_task_struct_by_pid;

struct dilated_timer_timeline_base * __alloc_global_dilated_timer_timeline_base(
    int timeline_id, int total_num_timelines) {
  struct dilated_timer_timeline_base * timeline_base;
  timeline_base = (struct dilated_timer_timeline_base *)kmalloc(
      sizeof(struct dilated_timer_timeline_base), GFP_KERNEL);
  if (!timeline_base) {
    PDEBUG_E(" Failed to Allot Timeline Base for timeline : %d\n", timeline_id);
    return NULL;
  }

  timeline_base->timeline_id = timeline_id;
  timeline_base->total_num_timelines = total_num_timelines;
  timeline_base->curr_virtual_time = START_VIRTUAL_TIME;
  timeline_base->dilated_clock_base.clock_active = 1;
  timeline_base->nxt_dilated_expiry = 0;
  timeline_base->running_dilated_timer = NULL;
  spin_lock_init(&timeline_base->lock);
  PDEBUG_I("Succesfully Initialized Timeline: %d Timer Base\n", timeline_id);
  return timeline_base;
}

void init_global_dilated_timer_timeline_bases(int total_num_timelines) {

    int i;
    if (total_num_timelines <= 0) {
      PDEBUG_E("Cannot Initialize timeline bases. "
        "Total Number of Timelines must be positive !\n");
      return;

    }
    global_dilated_timer_timeline_bases = (
      struct dilated_timer_timeline_base **)kmalloc(
        sizeof(struct dilated_timer_base *)*total_num_timelines, GFP_KERNEL);
    if (!global_dilated_timer_timeline_bases) {
      PDEBUG_E("Failed to Allot Memory for Global Dilated Timer "
               "Timeline Bases\n");
      return;
    }

    for (i = 0; i < total_num_timelines; i++) {
      global_dilated_timer_timeline_bases[i] = 
        __alloc_global_dilated_timer_timeline_base(i, total_num_timelines);
    }
    PDEBUG_I("Successfully Initialized All Timer Timeline Bases\n");
}

void free_global_dilated_timer_timeline_bases(int total_num_timelines) {

  int i;
  if (total_num_timelines <= 0) {
      return;
  }

  for (i = 0; i < total_num_timelines; i++) {
    kfree(global_dilated_timer_timeline_bases[i]);
  }
  kfree(global_dilated_timer_timeline_bases);
  PDEBUG_I("Successfully Cleaned up all Timer Timeline Bases\n");
}

int dilated_hrtimer_forward(struct hrtimer_dilated *timer, ktime_t interval) {
  if (!timer || (timer->state & HRTIMER_STATE_ENQUEUED) || interval < 0)
    return -1;

  timer->_softexpires = timer->_softexpires + interval;
  timer->node.expires = timer->_softexpires;
  return 1;
}

struct dilated_timer_timeline_base *get_timeline_base(int timeline_id) {
  if (timeline_id < 0)
    return NULL;
  if (timeline_id > 
    global_dilated_timer_timeline_bases[0]->total_num_timelines) {
    PDEBUG_E("Specified Timeline does not exist !\n");
    return NULL;
  }
  return global_dilated_timer_timeline_bases[timeline_id];
}

ktime_t __dilated_hrtimer_get_next_event(
    struct dilated_timer_timeline_base *timeline_base) {

  struct hrtimer_dilated_clock_base *base = &timeline_base->dilated_clock_base;
  ktime_t expires_next = KTIME_MAX;
  unsigned int active = base->clock_active;
  struct timerqueue_node *next;
  struct hrtimer_dilated *timer;

  if (!active) return expires_next;

  next = timerqueue_getnext(&base->active);

  if (!next) {
    expires_next = 0;
    return expires_next;
  }

  timer = container_of(next, struct hrtimer_dilated, node);
  expires_next = timer->_softexpires;

  if (expires_next < 0) expires_next = 0;
  return expires_next;
}

int __dilated_hrtimer_init(struct hrtimer_dilated *timer, int timeline_id,
                           enum hrtimer_mode mode) {
  struct dilated_timer_timeline_base *timeline_base 
    = get_timeline_base(timeline_id);
  int base;
  if (!timeline_base || timeline_base->dilated_clock_base.clock_active == 0)
    return -1;

  memset(timer, 0, sizeof(struct hrtimer_dilated));
  timer->base = &timeline_base->dilated_clock_base;
  timerqueue_init(&timer->node);
  timer->state = HRTIMER_STATE_INACTIVE;

  return 1;
}

/**
 * dilated_hrtimer_init - initialize a timer to the given clock
 * @timer:	the timer to be initialized
 * @mode:	timer mode abs/rel
 *
 */
int dilated_hrtimer_init(struct hrtimer_dilated *timer, int timeline_id,
                         enum hrtimer_mode mode) {
  return __dilated_hrtimer_init(timer, timeline_id, mode);
}

/*
 * enqueue_dilated_hrtimer - internal function to (re)start a timer
 *
 * The timer is inserted in expiry order. Insertion into the
 * red black tree is O(log(n)). Must hold the base lock.
 *
 * Returns 1 when the new timer is the leftmost timer in the tree.
 */
int enqueue_dilated_hrtimer(struct hrtimer_dilated *timer,
                            struct hrtimer_dilated_clock_base *base) {
  timer->state = HRTIMER_STATE_ENQUEUED;
  return timerqueue_add(&base->active, &timer->node);
}

/*
 * __remove_dilated_hrtimer - internal function to remove a timer
 *
 * Caller must hold the base lock.
 *
 * High resolution timer mode reprograms the clock event device when the
 * timer is the one which expires next. The caller can disable this by setting
 * reprogram to zero. This is useful, when the context does a reprogramming
 * anyway (e.g. timer interrupt)
 */
void __remove_dilated_hrtimer(struct hrtimer_dilated *timer,
                              struct hrtimer_dilated_clock_base *base,
                              u8 newstate) {
  // struct dilated_timer_timeline_base *timeline_base = base->timeline_base;
  u8 state = timer->state;

  timer->state = newstate;
  if (!(state & HRTIMER_STATE_ENQUEUED)) return;

  timerqueue_del(&base->active, &timer->node);
}

/*
 * remove dilated hrtimer, called with base lock held
 */
int remove_dilated_hrtimer(struct hrtimer_dilated *timer,
                           struct hrtimer_dilated_clock_base *base,
                           bool restart) {
  if (timer->state & HRTIMER_STATE_ENQUEUED) {
    u8 state = timer->state;

    // if restart is true, timer state will remain unchanged
    // else it will be set to inactive
    if (!restart) state = HRTIMER_STATE_INACTIVE;

    __remove_dilated_hrtimer(timer, base, state);
    return 1;
  }
  return 0;
}

/**
**	Expiry time may be relative or absolute depending on the mode.
**/
void dilated_hrtimer_start_range_ns(struct hrtimer_dilated *timer,
                                    ktime_t expiry_time,
                                    const enum hrtimer_mode mode) {
  struct hrtimer_dilated_clock_base *base = timer->base;
  unsigned long flags;
  int leftmost;
  struct dilated_timer_timeline_base *timeline_base = base->timeline_base;
  ktime_t expires_next;

  raw_spin_lock_irqsave(&timeline_base->lock, flags);

  /* Remove an active timer from the queue: */
  remove_dilated_hrtimer(timer, base, true);

  if (mode & HRTIMER_MODE_REL) {
    expiry_time = expiry_time + timeline_base->curr_virtual_time;
  }

  timer->_softexpires = expiry_time;
  timer->node.expires = timer->_softexpires;
  leftmost = enqueue_dilated_hrtimer(timer, base);
  if (!leftmost) {
    PDEBUG_E("HRTIMER DILATED: Enqueue error\n");
    goto unlock;
  }

  expires_next = __dilated_hrtimer_get_next_event(timeline_base);
  timeline_base->nxt_dilated_expiry = expires_next;

unlock:
  raw_spin_unlock_irqrestore(&timeline_base->lock, flags);
}

void dilated_hrtimer_start(struct hrtimer_dilated *timer, ktime_t expiry_time,
                           const enum hrtimer_mode mode) {
  dilated_hrtimer_start_range_ns(timer, expiry_time, mode);
}

/**
 * dilated_hrtimer_try_to_cancel - try to deactivate a timer
 * @timer:	hrtimer to stop
 *
 * Returns:
 *  0 when the timer was not active
 *  1 when the timer was active
 * -1 when the timer is currently excuting the callback function and
 *    cannot be stopped
 */
int dilated_hrtimer_try_to_cancel(struct hrtimer_dilated *timer) {
  struct hrtimer_dilated_clock_base *base = timer->base;
  unsigned long flags;
  struct dilated_timer_timeline_base *timeline_base = base->timeline_base;
  int ret = -1;

  /*
   * Check lockless first. If the timer is not active (neither
   * enqueued nor running the callback, nothing to do here.  The
   * base lock does not serialize against a concurrent enqueue,
   * so we can avoid taking it.
   */
  raw_spin_lock_irqsave(&base->timeline_base->lock, flags);

  if (timeline_base->running_dilated_timer != timer)
    ret = remove_dilated_hrtimer(timer, base, false);

  raw_spin_unlock_irqrestore(&base->timeline_base->lock, flags);

  return ret;
}

/**
 * dilated_hrtimer_cancel - cancel a timer and wait for the handler to finish.
 * @timer:	the timer to be cancelled
 *
 * Returns:
 *  0 when the timer was not active
 *  1 when the timer was active
 */
int dilated_hrtimer_cancel(struct hrtimer_dilated *timer) {
  for (;;) {
    int ret = dilated_hrtimer_try_to_cancel(timer);

    if (ret >= 0) return ret;
        cpu_relax();
  }
}

void a_run_dilated_hrtimer(struct dilated_timer_timeline_base *timeline_base,
                           struct hrtimer_dilated_clock_base *base,
                           struct hrtimer_dilated *timer) {
  enum hrtimer_restart (*fn)(struct hrtimer_dilated *);
  int restart = HRTIMER_NORESTART;

  lockdep_assert_held(&timeline_base->lock);

  __remove_dilated_hrtimer(timer, base, HRTIMER_STATE_INACTIVE);
  fn = timer->function;
  timeline_base->running_dilated_timer = timer;

  raw_spin_unlock(&timeline_base->lock);
  if (fn) restart = fn(timer);
  raw_spin_lock(&timeline_base->lock);

  if (restart != HRTIMER_NORESTART && !(timer->state & HRTIMER_STATE_ENQUEUED))
    enqueue_dilated_hrtimer(timer, base);

  timeline_base->running_dilated_timer = NULL;
}

void a_dilated_hrtimer_run_queues(
  struct dilated_timer_timeline_base *timeline_base, ktime_t now) {
  struct hrtimer_dilated_clock_base *base = &timeline_base->dilated_clock_base;
  struct hrtimer_dilated *timer;
  int active = base->clock_active;

  struct timerqueue_node *next;
  while ((next = timerqueue_getnext(&base->active))) {
    if (!next) break;

    timer = container_of(next, struct hrtimer_dilated, node);

    if (!timer) break;

    if (now < timer->_softexpires) break;

    a_run_dilated_hrtimer(timeline_base, base, timer);
  }
}

void dilated_hrtimer_run_queues_flush(int timeline_id) {
  struct dilated_timer_timeline_base *timeline_base = get_timeline_base(
      timeline_id);

  if (!timeline_base)
    return;
    
  struct hrtimer_dilated_clock_base *base = &timeline_base->dilated_clock_base;
  struct hrtimer_dilated *timer;
  int active = base->clock_active;

  struct timerqueue_node *next;
  while ((next = timerqueue_getnext(&base->active))) {
    if (!next) break;

    timer = container_of(next, struct hrtimer_dilated, node);

    if (!timer) break;

    timer->state = HRTIMER_STATE_ENQUEUED;
    __remove_dilated_hrtimer(timer, base, HRTIMER_STATE_INACTIVE);
  }
}


/*
 * Called from Kronos with interrupts disabled
 */
void dilated_hrtimer_run_queues(int timeline_id) {
  struct dilated_timer_timeline_base *timeline_base = get_timeline_base(
    timeline_id);
  ktime_t curr_virt_time;
  ktime_t expires_next;

  if (!timeline_base ||
      timeline_base->dilated_clock_base.clock_active == 0) return;

  curr_virt_time = timeline_base->curr_virtual_time;
  raw_spin_lock(&timeline_base->lock);

  /* Reevaluate the clock bases for the next expiry */
  expires_next = __dilated_hrtimer_get_next_event(timeline_base);
  timeline_base->nxt_dilated_expiry = expires_next;

  if (timeline_base->nxt_dilated_expiry != 0 &&
      timeline_base->nxt_dilated_expiry > curr_virt_time)
    goto skip;

  a_dilated_hrtimer_run_queues(timeline_base, curr_virt_time);

skip:
  raw_spin_unlock(&timeline_base->lock);
  return;
}


static enum hrtimer_restart dilated_sleep_wakeup(
    struct hrtimer_dilated *timer) {

	struct dilated_hrtimer_sleeper *sleeper =
		container_of(timer, struct dilated_hrtimer_sleeper, timer_dilated);

	if (sleeper && sleeper->task) {
		wake_up_process(sleeper->task);
	}
	return HRTIMER_NORESTART;
}

// sleeper should be allocated on heap using kmalloc
int dilated_hrtimer_sleep(ktime_t duration) {

	struct dilated_hrtimer_sleeper * sleeper;
	if (duration <= 0) {
		PDEBUG_V("Warning: Dilated hrtimer sleep. 0 Duration.\n");
		return 0;
	}

  struct dilated_task_struct * dilated_task = hmap_get_abs(
    &get_dilated_task_struct_by_pid, (int)current->pid);
  
  if (!dilated_task)
    return 0;

  tracer * associated_tracer = dilated_task->associated_tracer;
	if (!associated_tracer)
		return 0;

	sleeper = (struct dilated_hrtimer_sleeper *)kmalloc(
			   sizeof(struct dilated_hrtimer_sleeper), GFP_KERNEL);
	if (!sleeper) {
		PDEBUG_E("Dilated hrtimer sleep. NOMEM\n");
		return -ENOMEM;
	}
	
	sleeper->task = current;
	dilated_hrtimer_init(&sleeper->timer_dilated,
                       associated_tracer->timeline_assignment,
                       HRTIMER_MODE_REL);
	sleeper->timer_dilated.function = dilated_sleep_wakeup;
	set_current_state(TASK_INTERRUPTIBLE);

  // Wake-up tracer timeline worker


	dilated_hrtimer_start_range_ns(&sleeper->timer_dilated, duration,
                                   HRTIMER_MODE_REL);

  dilated_task->ready = 0;
  dilated_task->syscall_waiting = 0;
  dilated_task->burst_target = 0;
  associated_tracer->w_queue_wakeup_pid = 1;
	wake_up_interruptible(associated_tracer->w_queue);
	
  schedule();
	kfree(sleeper);
	PDEBUG_V("Resuming from dilated sleep. Sleep Duration: %llu.\n",
           duration);
	return 0;
} 

