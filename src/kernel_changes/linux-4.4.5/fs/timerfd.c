/*
 *  fs/timerfd.c
 *
 *  Copyright (C) 2007  Davide Libenzi <davidel@xmailserver.org>
 *
 *
 *  Thanks to Thomas Gleixner for code reviews and useful comments.
 *
 */

#include <linux/alarmtimer.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/anon_inodes.h>
#include <linux/timerfd.h>
#include <linux/syscalls.h>
#include <linux/compat.h>
#include <linux/rcupdate.h>

struct timerfd_ctx {
	union {
		struct hrtimer tmr;
		struct alarm alarm;
	} t;
	ktime_t tintv;
	ktime_t moffs;
	wait_queue_head_t wqh;
	u64 ticks;
	int clockid;
	short unsigned expired;
	short unsigned settime_flags; /* to show in fdinfo */
	struct rcu_head rcu;
	struct list_head clist;
	bool might_cancel;
	struct task_struct *owner_task;
	s64 wakeup_time;
	struct hrtimer_dilated dilated_tmr;
};

static LIST_HEAD(cancel_list);
static DEFINE_SPINLOCK(cancel_lock);

s64 get_dilated_task_time(struct task_struct *task)
{
	struct timeval tv;
	s64 now;
	do_gettimeofday(&tv);
	now = timeval_to_ns(&tv);

	if (task->virt_start_time != 0) {
		return task->curr_virt_time;
	}
	return now;
}

static inline bool isalarm(struct timerfd_ctx *ctx)
{
	return ctx->clockid == CLOCK_REALTIME_ALARM ||
		ctx->clockid == CLOCK_BOOTTIME_ALARM;
}

/*
 * This gets called when the timer event triggers. We set the "expired"
 * flag, but we do not re-arm the timer (in case it's necessary,
 * tintv.tv64 != 0) until the timer is accessed.
 */
void timerfd_triggered(struct timerfd_ctx *ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->wqh.lock, flags);
	ctx->expired = 1;
	ctx->ticks++;
	wake_up_locked(&ctx->wqh);
	spin_unlock_irqrestore(&ctx->wqh.lock, flags);

	if (ctx->owner_task && ctx->owner_task->virt_start_time > 0)
		trace_printk("Fired TimerFD timer at %llu, pid: %d\n",
			     ctx->owner_task->curr_virt_time,
			     ctx->owner_task->pid);
}

enum hrtimer_restart dilated_timerfd_tmrproc(struct hrtimer_dilated *htmr)
{
	struct timerfd_ctx *ctx =
		container_of(htmr, struct timerfd_ctx, dilated_tmr);

	s64 dilated_running_time;
	struct task_struct *owner_task = ctx->owner_task;
	trace_printk("Entered Dilated TimerFD fire, pid: %d\n",
		     owner_task->pid);
	timerfd_triggered(ctx);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart timerfd_tmrproc(struct hrtimer *htmr)
{
	struct timerfd_ctx *ctx = container_of(htmr, struct timerfd_ctx,
					       t.tmr);
	timerfd_triggered(ctx);
	return HRTIMER_NORESTART;
}

static enum alarmtimer_restart timerfd_alarmproc(struct alarm *alarm,
	ktime_t now)
{
	struct timerfd_ctx *ctx =
		container_of(alarm, struct timerfd_ctx, t.alarm);

	s64 dilated_running_time;
	struct task_struct *owner_task = ctx->owner_task;
	if (owner_task->virt_start_time != 0) {
		dilated_running_time = get_dilated_task_time(owner_task);
		if (ctx->wakeup_time <= dilated_running_time) {
			timerfd_triggered(ctx);
			return ALARMTIMER_NORESTART;
		} else {
			alarm_forward_now(&ctx->t.alarm, ns_to_ktime(100000));
			return ALARMTIMER_RESTART;
		}

	} else {
		timerfd_triggered(ctx);
	}
	return ALARMTIMER_NORESTART;
}

/*
 * Called when the clock was set to cancel the timers in the cancel
 * list. This will wake up processes waiting on these timers. The
 * wake-up requires ctx->ticks to be non zero, therefore we increment
 * it before calling wake_up_locked().
 */
void timerfd_clock_was_set(void)
{
	ktime_t moffs = ktime_mono_to_real((ktime_t){ .tv64 = 0 });
	struct timerfd_ctx *ctx;
	unsigned long flags;

	rcu_read_lock();
	list_for_each_entry_rcu(ctx, &cancel_list, clist) {
		if (!ctx->might_cancel)
			continue;
		spin_lock_irqsave(&ctx->wqh.lock, flags);
		if (ctx->moffs.tv64 != moffs.tv64) {
			ctx->moffs.tv64 = KTIME_MAX;
			ctx->ticks++;
			wake_up_locked(&ctx->wqh);
		}
		spin_unlock_irqrestore(&ctx->wqh.lock, flags);
	}
	rcu_read_unlock();
}

static void timerfd_remove_cancel(struct timerfd_ctx *ctx)
{
	if (ctx->might_cancel) {
		ctx->might_cancel = false;
		spin_lock(&cancel_lock);
		list_del_rcu(&ctx->clist);
		spin_unlock(&cancel_lock);
	}
}

static bool timerfd_canceled(struct timerfd_ctx *ctx)
{
	if (!ctx->might_cancel || ctx->moffs.tv64 != KTIME_MAX)
		return false;
	ctx->moffs = ktime_mono_to_real((ktime_t){ .tv64 = 0 });
	return true;
}

static void timerfd_setup_cancel(struct timerfd_ctx *ctx, int flags)
{
	if ((ctx->clockid == CLOCK_REALTIME ||
	     ctx->clockid == CLOCK_REALTIME_ALARM) &&
	    (flags & TFD_TIMER_ABSTIME) && (flags & TFD_TIMER_CANCEL_ON_SET)) {
		if (!ctx->might_cancel) {
			ctx->might_cancel = true;
			spin_lock(&cancel_lock);
			list_add_rcu(&ctx->clist, &cancel_list);
			spin_unlock(&cancel_lock);
		}
	} else if (ctx->might_cancel) {
		timerfd_remove_cancel(ctx);
	}
}

static ktime_t timerfd_get_remaining(struct timerfd_ctx *ctx)
{
	ktime_t remaining;

	if (isalarm(ctx))
		remaining = alarm_expires_remaining(&ctx->t.alarm);
	else {
		if (ctx->owner_task != NULL &&
		    ctx->owner_task->virt_start_time != 0) {
			remaining.tv64 = ctx->wakeup_time -
					 get_dilated_task_time(ctx->owner_task);
		} else {
			remaining =
				hrtimer_expires_remaining_adjusted(&ctx->t.tmr);
		}
	}

	return remaining.tv64 < 0 ? ktime_set(0, 0): remaining;
}

static int timerfd_setup(struct timerfd_ctx *ctx, int flags,
			 const struct itimerspec *ktmr)
{
	enum hrtimer_mode htmode;
	ktime_t texp;
	int clockid = ctx->clockid;
	s64 relative_expiry_duration = 1000000;
	s64 curr_dilated_time = 0;

	htmode = (flags & TFD_TIMER_ABSTIME) ? HRTIMER_MODE_ABS :
					       HRTIMER_MODE_REL;

	if (ctx->owner_task->virt_start_time != 0) {
		if (flags & TFD_TIMER_ABSTIME) {
			ctx->wakeup_time = timespec_to_ns(&ktmr->it_value);
			curr_dilated_time =
				get_dilated_task_time(ctx->owner_task);
			if (curr_dilated_time >= ctx->wakeup_time) {
				relative_expiry_duration = 0;
			} else {
				relative_expiry_duration =
					ctx->wakeup_time - curr_dilated_time;
			}

		} else {
			curr_dilated_time =
				get_dilated_task_time(ctx->owner_task);
			relative_expiry_duration =
				timespec_to_ns(&ktmr->it_value);
			ctx->wakeup_time =
				curr_dilated_time + relative_expiry_duration;
		}
	}

	texp = timespec_to_ktime(ktmr->it_value);
	ctx->expired = 0;
	ctx->ticks = 0;
	ctx->tintv = timespec_to_ktime(ktmr->it_interval);

	if (isalarm(ctx)) {
		alarm_init(&ctx->t.alarm,
			   ctx->clockid == CLOCK_REALTIME_ALARM ?
			   ALARM_REALTIME : ALARM_BOOTTIME,
			   timerfd_alarmproc);
	} else {
		if (ctx->owner_task->virt_start_time != 0) {
			// hrtimer_init(&ctx->t.tmr, clockid, HRTIMER_MODE_REL);
			// hrtimer_set_expires(&ctx->t.tmr,
			// ns_to_ktime(relative_expiry_duration));
			dilated_hrtimer_init(&ctx->dilated_tmr, 0,
					     HRTIMER_MODE_REL);
			ctx->dilated_tmr.function = dilated_timerfd_tmrproc;
		} else {
			hrtimer_init(&ctx->t.tmr, clockid, htmode);
			hrtimer_set_expires(&ctx->t.tmr, texp);
			ctx->t.tmr.function = timerfd_tmrproc;
		}
	}

	if (texp.tv64 != 0) {
		if (isalarm(ctx)) {
			if (flags & TFD_TIMER_ABSTIME)
				alarm_start(&ctx->t.alarm, texp);
			else
				alarm_start_relative(&ctx->t.alarm, texp);
		} else {
			if (ctx->owner_task->virt_start_time != 0) {
				// hrtimer_start(&ctx->t.tmr, ns_to_ktime(relative_expiry_duration),
				// HRTIMER_MODE_REL);
				trace_printk(
					"Started TimerFD timer at : %llu, for : %llu, pid: %d\n",
					ctx->owner_task->curr_virt_time,
					relative_expiry_duration,
					ctx->owner_task->pid);
				dilated_hrtimer_start(
					&ctx->dilated_tmr,
					ns_to_ktime(relative_expiry_duration),
					HRTIMER_MODE_REL);
			} else {
				hrtimer_start(&ctx->t.tmr, texp, htmode);
			}
		}

		if (timerfd_canceled(ctx))
			return -ECANCELED;
	}

	ctx->settime_flags = flags & TFD_SETTIME_FLAGS;
	return 0;
}

static int timerfd_release(struct inode *inode, struct file *file)
{
	struct timerfd_ctx *ctx = file->private_data;

	timerfd_remove_cancel(ctx);

	if (isalarm(ctx))
		alarm_cancel(&ctx->t.alarm);
	else {
		if (ctx->owner_task && ctx->owner_task->virt_start_time != 0) {
			dilated_hrtimer_cancel(&ctx->dilated_tmr);
		} else {
			hrtimer_cancel(&ctx->t.tmr);
		}
	}
	kfree_rcu(ctx, rcu);
	return 0;
}

static unsigned int timerfd_poll(struct file *file, poll_table *wait)
{
	struct timerfd_ctx *ctx = file->private_data;
	unsigned int events = 0;
	unsigned long flags;

	// if(ctx->owner_task->virt_start_time != 0)
	poll_wait(file, &ctx->wqh, wait);

	spin_lock_irqsave(&ctx->wqh.lock, flags);
	if (ctx->ticks)
		events |= POLLIN;
	spin_unlock_irqrestore(&ctx->wqh.lock, flags);

	return events;
}

static ssize_t timerfd_read(struct file *file, char __user *buf, size_t count,
			    loff_t *ppos)
{
	struct timerfd_ctx *ctx = file->private_data;
	ssize_t res;
	u64 ticks = 0;
	struct task_struct *owner_task;
	s64 intervalns;

	if (count < sizeof(ticks))
		return -EINVAL;
	spin_lock_irq(&ctx->wqh.lock);
	if (file->f_flags & O_NONBLOCK)
		res = -EAGAIN;
	else
		res = wait_event_interruptible_locked_irq(ctx->wqh, ctx->ticks);

	/*
	 * If clock has changed, we do not care about the
	 * ticks and we do not rearm the timer. Userspace must
	 * reevaluate anyway.
	 */
	if (timerfd_canceled(ctx)) {
		ctx->ticks = 0;
		ctx->expired = 0;
		res = -ECANCELED;
	}

	if (ctx->ticks) {
		ticks = ctx->ticks;

		if (ctx->expired && ctx->tintv.tv64) {
			/*
			 * If tintv.tv64 != 0, this is a periodic timer that
			 * needs to be re-armed. We avoid doing it in the timer
			 * callback to avoid DoS attacks specifying a very
			 * short timer period.
			 */

			owner_task = ctx->owner_task;

			if (owner_task && owner_task->virt_start_time != 0) {
				// printk(KERN_INFO "TimerFD: Read operation woken up for dilated task
				// %d, actual wakeup time = %llu\n", owner_task->pid,
				// get_dilated_task_time(owner_task));
				intervalns = ktime_to_ns(ctx->tintv);

				////ctx->wakeup_time = ctx->wakeup_time + intervalns;
				ctx->wakeup_time =
					get_dilated_task_time(ctx->owner_task) +
					intervalns;
			}

			if (isalarm(ctx)) {
				ticks += alarm_forward_now(&ctx->t.alarm,
							   ctx->tintv) -
					 1;
				alarm_restart(&ctx->t.alarm);
			} else {
				if (ctx->owner_task &&
				    ctx->owner_task->virt_start_time != 0) {
					dilated_hrtimer_cancel(
						&ctx->dilated_tmr);
					dilated_hrtimer_init(&ctx->dilated_tmr,
							     0,
							     HRTIMER_MODE_REL);
					ctx->dilated_tmr.function =
						dilated_timerfd_tmrproc;
					dilated_hrtimer_start(&ctx->dilated_tmr,
							      ctx->tintv,
							      HRTIMER_MODE_REL);
				} else {
					ticks += hrtimer_forward_now(
							 &ctx->t.tmr,
							 ctx->tintv) -
						 1;
					hrtimer_restart(&ctx->t.tmr);
				}
			}

			if (owner_task->virt_start_time != 0)
				ticks = ctx->ticks;
		}
		ctx->expired = 0;
		ctx->ticks = 0;
	}
	spin_unlock_irq(&ctx->wqh.lock);
	if (ticks)
		res = put_user(ticks, (u64 __user *) buf) ? -EFAULT: sizeof(ticks);
	return res;
}

#ifdef CONFIG_PROC_FS
static void timerfd_show(struct seq_file *m, struct file *file)
{
	struct timerfd_ctx *ctx = file->private_data;
	struct itimerspec t;

	spin_lock_irq(&ctx->wqh.lock);
	t.it_value = ktime_to_timespec(timerfd_get_remaining(ctx));
	t.it_interval = ktime_to_timespec(ctx->tintv);
	spin_unlock_irq(&ctx->wqh.lock);

	seq_printf(m,
		   "clockid: %d\n"
		   "ticks: %llu\n"
		   "settime flags: 0%o\n"
		   "it_value: (%llu, %llu)\n"
		   "it_interval: (%llu, %llu)\n",
		   ctx->clockid,
		   (unsigned long long)ctx->ticks,
		   ctx->settime_flags,
		   (unsigned long long)t.it_value.tv_sec,
		   (unsigned long long)t.it_value.tv_nsec,
		   (unsigned long long)t.it_interval.tv_sec,
		   (unsigned long long)t.it_interval.tv_nsec);
}
#else
#define timerfd_show NULL
#endif

#ifdef CONFIG_CHECKPOINT_RESTORE
static long timerfd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct timerfd_ctx *ctx = file->private_data;
	int ret = 0;

	switch (cmd) {
	case TFD_IOC_SET_TICKS: {
		u64 ticks;

		if (copy_from_user(&ticks, (u64 __user *)arg, sizeof(ticks)))
			return -EFAULT;
		if (!ticks)
			return -EINVAL;

		spin_lock_irq(&ctx->wqh.lock);
		if (!timerfd_canceled(ctx)) {
			ctx->ticks = ticks;
			wake_up_locked(&ctx->wqh);
		} else
			ret = -ECANCELED;
		spin_unlock_irq(&ctx->wqh.lock);
		break;
	}
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}
#else
#define timerfd_ioctl NULL
#endif

static const struct file_operations timerfd_fops = {
	.release	= timerfd_release,
	.poll		= timerfd_poll,
	.read		= timerfd_read,
	.llseek		= noop_llseek,
	.show_fdinfo	= timerfd_show,
	.unlocked_ioctl	= timerfd_ioctl,
};

static int timerfd_fget(int fd, struct fd *p)
{
	struct fd f = fdget(fd);
	if (!f.file)
		return -EBADF;
	if (f.file->f_op != &timerfd_fops) {
		fdput(f);
		return -EINVAL;
	}
	*p = f;
	return 0;
}

SYSCALL_DEFINE2(timerfd_create, int, clockid, int, flags)
{
	int ufd;
	struct timerfd_ctx *ctx;

	/* Check the TFD_* constants for consistency.  */
	BUILD_BUG_ON(TFD_CLOEXEC != O_CLOEXEC);
	BUILD_BUG_ON(TFD_NONBLOCK != O_NONBLOCK);

	if ((flags & ~TFD_CREATE_FLAGS) ||
	    (clockid != CLOCK_MONOTONIC &&
	     clockid != CLOCK_REALTIME &&
	     clockid != CLOCK_REALTIME_ALARM &&
	     clockid != CLOCK_BOOTTIME &&
	     clockid != CLOCK_BOOTTIME_ALARM))
		return -EINVAL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	init_waitqueue_head(&ctx->wqh);
	ctx->clockid = clockid;

	if (isalarm(ctx))
		alarm_init(&ctx->t.alarm,
			   ctx->clockid == CLOCK_REALTIME_ALARM ?
			   ALARM_REALTIME : ALARM_BOOTTIME,
			   timerfd_alarmproc);
	else {
		if (current->virt_start_time != 0) {
			dilated_hrtimer_init(&ctx->dilated_tmr, 0,
					     HRTIMER_MODE_REL);
		}
		hrtimer_init(&ctx->t.tmr, clockid, HRTIMER_MODE_ABS);
	}

	ctx->moffs = ktime_mono_to_real((ktime_t){ .tv64 = 0 });
	ctx->owner_task = current;
	ctx->wakeup_time = 0;

	ufd = anon_inode_getfd("[timerfd]", &timerfd_fops, ctx,
			       O_RDWR | (flags & TFD_SHARED_FCNTL_FLAGS));
	if (ufd < 0)
		kfree(ctx);

	return ufd;
}

static int do_timerfd_settime(int ufd, int flags, 
		const struct itimerspec *new,
		struct itimerspec *old)
{
	struct fd f;
	struct timerfd_ctx *ctx;
	int ret;

	if ((flags & ~TFD_SETTIME_FLAGS) ||
	    !timespec_valid(&new->it_value) ||
	    !timespec_valid(&new->it_interval))
		return -EINVAL;

	ret = timerfd_fget(ufd, &f);
	if (ret)
		return ret;
	ctx = f.file->private_data;

	timerfd_setup_cancel(ctx, flags);

	/*
	 * We need to stop the existing timer before reprogramming
	 * it to the new values.
	 */
	for (;;) {
		spin_lock_irq(&ctx->wqh.lock);

		if (isalarm(ctx)) {
			if (alarm_try_to_cancel(&ctx->t.alarm) >= 0)
				break;
		} else {
			if (ctx->owner_task &&
			    ctx->owner_task->virt_start_time != 0) {
				if (dilated_hrtimer_try_to_cancel(
					    &ctx->dilated_tmr) >= 0)
					break;
			} else {
				if (hrtimer_try_to_cancel(&ctx->t.tmr) >= 0)
					break;
			}
		}
		spin_unlock_irq(&ctx->wqh.lock);
		cpu_relax();
	}

	/*
	 * If the timer is expired and it's periodic, we need to advance it
	 * because the caller may want to know the previous expiration time.
	 * We do not update "ticks" and "expired" since the timer will be
	 * re-programmed again in the following timerfd_setup() call.
	 */
	if (ctx->expired && ctx->tintv.tv64) {
		if (isalarm(ctx))
			alarm_forward_now(&ctx->t.alarm, ctx->tintv);
		else if (ctx->owner_task &&
			 ctx->owner_task->virt_start_time != 0) {
			// dilated_hrtimer_forward(&ctx->dil)
		} else {
			hrtimer_forward_now(&ctx->t.tmr, ctx->tintv);
		}
	}

	old->it_value = ktime_to_timespec(timerfd_get_remaining(ctx));
	old->it_interval = ktime_to_timespec(ctx->tintv);

	/*
	 * Re-program the timer to the new value ...
	 */
	ret = timerfd_setup(ctx, flags, new);

	spin_unlock_irq(&ctx->wqh.lock);
	fdput(f);
	return ret;
}

static int do_timerfd_gettime(int ufd, struct itimerspec *t)
{
	struct fd f;
	struct timerfd_ctx *ctx;
	struct task_struct *owner_task;
	s64 intervalns;
	int ret = timerfd_fget(ufd, &f);
	if (ret)
		return ret;
	ctx = f.file->private_data;
	owner_task = ctx->owner_task;

	spin_lock_irq(&ctx->wqh.lock);
	if (ctx->expired && ctx->tintv.tv64) {
		ctx->expired = 0;

		if (owner_task && owner_task->virt_start_time != 0) {
			intervalns = ktime_to_ns(ctx->tintv);
			ctx->wakeup_time = ctx->wakeup_time + intervalns;
		}

		if (isalarm(ctx)) {
			ctx->ticks +=
				alarm_forward_now(
					&ctx->t.alarm, ctx->tintv) - 1;
			alarm_restart(&ctx->t.alarm);
		}
		if (ctx->owner_task && ctx->owner_task->virt_start_time != 0) {
			ctx->wakeup_time =
				get_dilated_task_time(ctx->owner_task) +
				ktime_to_ns(ctx->tintv);
			dilated_hrtimer_start(&ctx->dilated_tmr, ctx->tintv,
					      HRTIMER_MODE_REL);
		} else {
			ctx->ticks +=
				hrtimer_forward_now(&ctx->t.tmr, ctx->tintv)
				- 1;
			hrtimer_restart(&ctx->t.tmr);
		}
	}

	if (owner_task && owner_task->virt_start_time != 0) {
		s64 curr_time = get_dilated_task_time(owner_task);
		s64 rem_time;
		if (ctx->wakeup_time <= curr_time) {
			rem_time = 0;
		} else {
			rem_time = ctx->wakeup_time - curr_time;
		}

		t->it_value = ns_to_timespec(rem_time);
	} else
		t->it_value = ktime_to_timespec(timerfd_get_remaining(ctx));

	t->it_interval = ktime_to_timespec(ctx->tintv);
	spin_unlock_irq(&ctx->wqh.lock);
	fdput(f);
	return 0;
}

SYSCALL_DEFINE4(timerfd_settime, int, ufd, int, flags,
		const struct itimerspec __user *, utmr,
		struct itimerspec __user *, otmr)
{
	struct itimerspec new, old;
	int ret;

	if (copy_from_user(&new, utmr, sizeof(new)))
		return -EFAULT;
	ret = do_timerfd_settime(ufd, flags, &new, &old);
	if (ret)
		return ret;
	if (otmr && copy_to_user(otmr, &old, sizeof(old)))
		return -EFAULT;

	return ret;
}

SYSCALL_DEFINE2(timerfd_gettime, int, ufd, struct itimerspec __user *, otmr)
{
	struct itimerspec kotmr;
	int ret = do_timerfd_gettime(ufd, &kotmr);
	if (ret)
		return ret;
	return copy_to_user(otmr, &kotmr, sizeof(kotmr)) ? -EFAULT: 0;
}

#ifdef CONFIG_COMPAT
COMPAT_SYSCALL_DEFINE4(timerfd_settime, int, ufd, int, flags,
		const struct compat_itimerspec __user *, utmr,
		struct compat_itimerspec __user *, otmr)
{
	struct itimerspec new, old;
	int ret;

	if (get_compat_itimerspec(&new, utmr))
		return -EFAULT;
	ret = do_timerfd_settime(ufd, flags, &new, &old);
	if (ret)
		return ret;
	if (otmr && put_compat_itimerspec(otmr, &old))
		return -EFAULT;
	return ret;
}

COMPAT_SYSCALL_DEFINE2(timerfd_gettime, int, ufd,
		struct compat_itimerspec __user *, otmr)
{
	struct itimerspec kotmr;
	int ret = do_timerfd_gettime(ufd, &kotmr);
	if (ret)
		return ret;
	return put_compat_itimerspec(otmr, &kotmr) ? -EFAULT: 0;
}
#endif
