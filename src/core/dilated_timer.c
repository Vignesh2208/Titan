

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


extern struct DilatedTimerTimelineBase ** globalDilatedTimerTimelineBases;
extern hashmap get_dilated_task_struct_by_pid;
extern int experiment_status;

struct DilatedTimerTimelineBase * __alloc_global_DilatedTimerTimelineBase(
    int timeline_id, int total_num_timelines) {
  struct DilatedTimerTimelineBase * timeline_base;
  timeline_base = (struct DilatedTimerTimelineBase *)kmalloc(
      sizeof(struct DilatedTimerTimelineBase), GFP_KERNEL);
  if (!timeline_base) {
    PDEBUG_E(" Failed to Allot Timeline Base for timeline : %d\n", timeline_id);
    return NULL;
  }

  timeline_base->timeline_id = timeline_id;
  timeline_base->total_num_timelines = total_num_timelines;
  timeline_base->curr_virtual_time = START_VIRTUAL_TIME;
  timeline_base->dilated_clock_base.clock_active = 1;
  timeline_base->dilated_clock_base.timeline_base = timeline_base;
  timerqueue_init_head(&timeline_base->dilated_clock_base.active);
  #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
  timeline_base->nxt_dilated_expiry.tv64 = 0;
  #else
  timeline_base->nxt_dilated_expiry = 0;
  #endif
  timeline_base->running_dilated_timer = NULL;
  spin_lock_init(&timeline_base->lock);
  PDEBUG_I("Succesfully Initialized Timeline: %d Timer Base\n", timeline_id);
  return timeline_base;
}

void InitGlobalDilatedTimerTimelineBases(int total_num_timelines) {

    int i;
    if (total_num_timelines <= 0) {
      PDEBUG_E("Cannot Initialize timeline bases. "
        "Total Number of Timelines must be positive !\n");
      return;

    }
    globalDilatedTimerTimelineBases = (
      struct DilatedTimerTimelineBase **)kmalloc(
        sizeof(struct dilated_timer_base *)*total_num_timelines, GFP_KERNEL);
    if (!globalDilatedTimerTimelineBases) {
      PDEBUG_E("Failed to Allot Memory for Global Dilated Timer "
               "Timeline Bases\n");
      return;
    }

    for (i = 0; i < total_num_timelines; i++) {
      globalDilatedTimerTimelineBases[i] = 
        __alloc_global_DilatedTimerTimelineBase(i, total_num_timelines);
    }
    PDEBUG_I("Successfully Initialized All Timer Timeline Bases\n");
}

void FreeGlobalDilatedTimerTimelineBases(int total_num_timelines) {

  int i;
  if (total_num_timelines <= 0) {
      return;
  }

  for (i = 0; i < total_num_timelines; i++) {
    kfree(globalDilatedTimerTimelineBases[i]);
  }
  kfree(globalDilatedTimerTimelineBases);
  PDEBUG_I("Successfully Cleaned up all Timer Timeline Bases\n");
}

int DilatedHrtimerForward(struct HrtimerDilated *timer, ktime_t interval) {
  #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
  if (!timer || (timer->state & HRTIMER_STATE_ENQUEUED) || interval.tv64 < 0)
    return -1;
  timer->_softexpires.tv64 = timer->_softexpires.tv64 + interval.tv64;
  #else
  if (!timer || (timer->state & HRTIMER_STATE_ENQUEUED) || interval < 0)
    return -1;
  timer->_softexpires = timer->_softexpires + interval;
  #endif
  
  timer->node.expires = timer->_softexpires;
  return 1;
}

struct DilatedTimerTimelineBase *get_timeline_base(int timeline_id) {
  if (timeline_id < 0)
    return NULL;
  if (timeline_id > 
    globalDilatedTimerTimelineBases[0]->total_num_timelines) {
    PDEBUG_E("Specified Timeline does not exist !\n");
    return NULL;
  }
  return globalDilatedTimerTimelineBases[timeline_id];
}

ktime_t __dilated_hrtimer_get_next_event(
    struct DilatedTimerTimelineBase *timeline_base) {

  struct HrtimerDilatedClockBase *base = &timeline_base->dilated_clock_base;

  #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
  ktime_t expires_next = {.tv64 = KTIME_MAX};
  #else
  ktime_t expires_next = KTIME_MAX;
  #endif
  unsigned int active = base->clock_active;
  struct timerqueue_node *next;
  struct HrtimerDilated *timer;

  if (!active) return expires_next;

  next = timerqueue_getnext(&base->active);

  if (!next) {
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
    expires_next.tv64 = 0;
    #else
    expires_next = 0;
    #endif
    return expires_next;
  }

  timer = container_of(next, struct HrtimerDilated, node);
  expires_next = timer->_softexpires;
  #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
  if (expires_next.tv64 < 0) expires_next.tv64 = 0;
  #else
  if (expires_next < 0) return expires_next;
  #endif
  return expires_next;
}

int __DilatedHrtimerInit(struct HrtimerDilated *timer, int timeline_id,
                           enum hrtimer_mode mode) {
  struct DilatedTimerTimelineBase *timeline_base 
    = get_timeline_base(timeline_id);
  int base;
  if (!timeline_base || timeline_base->dilated_clock_base.clock_active == 0)
    return -1;

  memset(timer, 0, sizeof(struct HrtimerDilated));
  timer->base = &timeline_base->dilated_clock_base;
  timerqueue_init(&timer->node);
  timer->state = HRTIMER_STATE_INACTIVE;

  return 1;
}

/**
 * DilatedHrtimerInit - initialize a timer to the given clock
 * @timer:	the timer to be initialized
 * @mode:	timer mode abs/rel
 *
 */
int DilatedHrtimerInit(struct HrtimerDilated *timer, int timeline_id,
                         enum hrtimer_mode mode) {
  return __DilatedHrtimerInit(timer, timeline_id, mode);
}

/*
 * enqueue_dilated_hrtimer - internal function to (re)start a timer
 *
 * The timer is inserted in expiry order. Insertion into the
 * red black tree is O(log(n)). Must hold the base lock.
 *
 * Returns 1 when the new timer is the leftmost timer in the tree.
 */
int enqueue_dilated_hrtimer(struct HrtimerDilated *timer,
                            struct HrtimerDilatedClockBase *base) {
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
void __remove_dilated_hrtimer(struct HrtimerDilated *timer,
                              struct HrtimerDilatedClockBase *base,
                              u8 newstate) {
  // struct DilatedTimerTimelineBase *timeline_base = base->timeline_base;
  u8 state = timer->state;

  timer->state = newstate;
  if (!(state & HRTIMER_STATE_ENQUEUED)) return;

  timerqueue_del(&base->active, &timer->node);
}

/*
 * remove dilated hrtimer, called with base lock held
 */
int remove_dilated_hrtimer(struct HrtimerDilated *timer,
                           struct HrtimerDilatedClockBase *base,
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
void DilatedHrtimerStartRangeNs(struct HrtimerDilated *timer,
                                    ktime_t expiry_time,
                                    const enum hrtimer_mode mode) {
  struct HrtimerDilatedClockBase *base = timer->base;
  unsigned long flags;
  int leftmost;
  struct DilatedTimerTimelineBase *timeline_base = base->timeline_base;
  ktime_t expires_next;

  raw_spin_lock_irqsave(&timeline_base->lock, flags);

  /* Remove an active timer from the queue: */
  remove_dilated_hrtimer(timer, base, true);

  if (mode & HRTIMER_MODE_REL) {
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
    expiry_time.tv64 = expiry_time.tv64 + timeline_base->curr_virtual_time;
    #else
    expiry_time += timeline_base->curr_virtual_time;
    #endif
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

void DilatedHrtimerStart(struct HrtimerDilated *timer, ktime_t expiry_time,
                           const enum hrtimer_mode mode) {
  DilatedHrtimerStartRangeNs(timer, expiry_time, mode);
}

/**
 * DilatedHrtimerTryToCancel - try to deactivate a timer
 * @timer:	hrtimer to stop
 *
 * Returns:
 *  0 when the timer was not active
 *  1 when the timer was active
 * -1 when the timer is currently excuting the callback function and
 *    cannot be stopped
 */
int DilatedHrtimerTryToCancel(struct HrtimerDilated *timer) {
  struct HrtimerDilatedClockBase *base = timer->base;
  unsigned long flags;
  struct DilatedTimerTimelineBase *timeline_base = base->timeline_base;
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
 * DilatedTimerCancel - cancel a timer and wait for the handler to finish.
 * @timer:	the timer to be cancelled
 *
 * Returns:
 *  0 when the timer was not active
 *  1 when the timer was active
 */
int DilatedTimerCancel(struct HrtimerDilated *timer) {
  for (;;) {
    int ret = DilatedHrtimerTryToCancel(timer);

    if (ret >= 0) return ret;
        cpu_relax();
  }
}

void a_run_dilated_hrtimer(struct DilatedTimerTimelineBase *timeline_base,
                           struct HrtimerDilatedClockBase *base,
                           struct HrtimerDilated *timer,
         int ignore) {
  enum hrtimer_restart (*fn)(struct HrtimerDilated *);
  int restart = HRTIMER_NORESTART;

  lockdep_assert_held(&timeline_base->lock);

  __remove_dilated_hrtimer(timer, base, HRTIMER_STATE_INACTIVE);
  fn = timer->function;
  timeline_base->running_dilated_timer = timer;

  raw_spin_unlock(&timeline_base->lock);
  if (fn) restart = fn(timer);
  raw_spin_lock(&timeline_base->lock);

  if (!ignore && restart != HRTIMER_NORESTART && !(timer->state & HRTIMER_STATE_ENQUEUED))
    enqueue_dilated_hrtimer(timer, base);

  timeline_base->running_dilated_timer = NULL;
}

void a_DilatedHrtimerRunQueues(
  struct DilatedTimerTimelineBase *timeline_base, ktime_t now) {
  struct HrtimerDilatedClockBase *base = &timeline_base->dilated_clock_base;
  struct HrtimerDilated *timer;
  int active = base->clock_active;

  struct timerqueue_node *next;
  while ((next = timerqueue_getnext(&base->active))) {
    if (!next) break;

    timer = container_of(next, struct HrtimerDilated, node);

    if (!timer) break;
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
    if (now.tv64 < timer->_softexpires.tv64) break;
    #else
    if (now < timer->_softexpires) break;
    #endif

    a_run_dilated_hrtimer(timeline_base, base, timer, 0);
  }
}

void DilatedHrtimerRunQueuesFlush(int timeline_id) {
  struct DilatedTimerTimelineBase *timeline_base = get_timeline_base(
      timeline_id);

  if (!timeline_base)
    return;
    
  struct HrtimerDilatedClockBase *base = &timeline_base->dilated_clock_base;
  struct HrtimerDilated *timer;
  int active = base->clock_active;

  struct timerqueue_node *next;
  while ((next = timerqueue_getnext(&base->active))) {
    if (!next) break;

    timer = container_of(next, struct HrtimerDilated, node);

    if (!timer) break;

    //timer->state = HRTIMER_STATE_ENQUEUED;
    //__remove_dilated_hrtimer(timer, base, HRTIMER_STATE_INACTIVE);

    a_run_dilated_hrtimer(timeline_base, base, timer, 1);
  }
}


void SetTimerbaseClock(int timeline_id, ktime_t time_to_set) {
  struct DilatedTimerTimelineBase * base = get_timeline_base(
    timeline_id);
  #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
  if (!base || time_to_set.tv64 <= 0)
    return;
  base->curr_virtual_time  = time_to_set.tv64;
  #else
  if (!base || time_to_set <= 0) return;

  base->curr_virtual_time = time_to_set;
  #endif
} 

void IncTimerbaseClock(int timeline_id, ktime_t inc) {
  struct DilatedTimerTimelineBase * base = get_timeline_base(
    timeline_id);
  #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
  if (!base || inc.tv64 <= 0)
    return;
  base->curr_virtual_time  += inc.tv64;
  #else
  if (!base || inc <= 0) return;
  base->curr_virtual_time += inc;
  #endif
}

/*
 * Called from Kronos with interrupts disabled
 */
void DilatedHrtimerRunQueues(int timeline_id) {
  struct DilatedTimerTimelineBase *timeline_base = get_timeline_base(
    timeline_id);
  ktime_t curr_virt_time;
  ktime_t expires_next;

  if (!timeline_base ||
      timeline_base->dilated_clock_base.clock_active == 0) return;

  #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
  curr_virt_time.tv64 = timeline_base->curr_virtual_time;
  #else
  curr_virt_time = timeline_base->curr_virtual_time;
  #endif
  raw_spin_lock(&timeline_base->lock);

  /* Reevaluate the clock bases for the next expiry */
  expires_next = __dilated_hrtimer_get_next_event(timeline_base);
  timeline_base->nxt_dilated_expiry = expires_next;

  #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
  if (timeline_base->nxt_dilated_expiry.tv64 != 0 &&
      timeline_base->nxt_dilated_expiry.tv64 > curr_virt_time.tv64)
    goto skip;
  #else
  if (timeline_base->nxt_dilated_expiry != 0 &&
      timeline_base->nxt_dilated_expiry > curr_virt_time)
    goto skip;
  #endif

  a_DilatedHrtimerRunQueues(timeline_base, curr_virt_time);

skip:
  raw_spin_unlock(&timeline_base->lock);
  return;
}


static enum hrtimer_restart dilated_sleep_wakeup(
    struct HrtimerDilated *timer) {

  struct DilatedHrtimerSleeper *sleeper =
    container_of(timer, struct DilatedHrtimerSleeper, timer_dilated);

  if (sleeper && sleeper->task) {
    sleeper->associated_dilated_task_struct->resumed_by_dilated_timer = 1;
    wake_up_process(sleeper->task);
  }
  return HRTIMER_NORESTART;
}

// sleeper should be allocated on heap using kmalloc
int DilatedHrtimerSleep(ktime_t duration, struct dilated_task_struct * dilated_task) {

  struct DilatedHrtimerSleeper * sleeper;
  #if LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,5)
  if (duration.tv64 <= 0) {
  #else
  if (duration <= 0) {
  #endif
    PDEBUG_V("Warning: Dilated hrtimer sleep. 0 Duration.\n");
    return 0;
  }

  if (!dilated_task)
    return 0;

  tracer * associated_tracer = dilated_task->associated_tracer;
  if (!associated_tracer)
    return 0;

  sleeper = (struct DilatedHrtimerSleeper *)kmalloc(
         sizeof(struct DilatedHrtimerSleeper), GFP_KERNEL);
  if (!sleeper) {
    PDEBUG_E("Dilated hrtimer sleep. NOMEM\n");
    return -ENOMEM;
  }
  
  sleeper->task = current;
  sleeper->associated_dilated_task_struct = dilated_task;
  DilatedHrtimerInit(&sleeper->timer_dilated,
                       associated_tracer->timeline_assignment,
                       HRTIMER_MODE_REL);
  sleeper->timer_dilated.function = dilated_sleep_wakeup;
  set_current_state(TASK_INTERRUPTIBLE);

    // Wake-up tracer timeline worker

  PDEBUG_A("Starting dilated sleep. Sleep Duration: %llu.\n",
           duration);
  DilatedHrtimerStartRangeNs(&sleeper->timer_dilated, duration,HRTIMER_MODE_REL);

  dilated_task->ready = 0;
  dilated_task->syscall_waiting = 0;
  dilated_task->burst_target = 0;
  associated_tracer->w_queue_wakeup_pid = 1;
  wake_up_interruptible(associated_tracer->w_queue);
  
  schedule();
  kfree(sleeper);
  PDEBUG_A("Resuming from dilated sleep. Sleep Duration: %llu.\n",
           duration);
  if (experiment_status != RUNNING)
    return -EFAULT;
  return 0;
} 

