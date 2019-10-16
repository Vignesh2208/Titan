#ifndef __INCLUDES__
#define __INCLUDES__
#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netdevice.h>
#include <asm/siginfo.h>
#include <net/sock.h>
#include <linux/errno.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/hashtable.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/ptrace.h>

#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/mm.h>

/* user defined headers */
#include "../utils/linkedlist.h"
#include "../utils/hashmap.h"
#include "vt_api_cmds.h"


#define VT_IOC_MAGIC  'k'

#define VT_UPDATE_TRACER_CLOCK _IOW(VT_IOC_MAGIC,  1, int)
#define VT_WRITE_RESULTS _IOW(VT_IOC_MAGIC,  2, int)
#define VT_GET_CURRENT_VIRTUAL_TIME _IOW(VT_IOC_MAGIC,  3, int)
#define VT_REGISTER_TRACER _IOW(VT_IOC_MAGIC,  4, int)
#define VT_ADD_PROCESSES_TO_SQ _IOW(VT_IOC_MAGIC,  5, int)
#define VT_RM_PROCESSES_FROM_SQ _IOW(VT_IOC_MAGIC,  6, int)
#define VT_SYNC_AND_FREEZE _IOW(VT_IOC_MAGIC,  7, int)
#define VT_INITIALIZE_EXP _IOW(VT_IOC_MAGIC,  8, int)
#define VT_GETTIME_PID _IOW(VT_IOC_MAGIC,  9, int)
#define VT_START_EXP _IOW(VT_IOC_MAGIC,  10, int)
#define VT_STOP_EXP _IOW(VT_IOC_MAGIC,  11, int)
#define VT_PROGRESS_BY _IOW(VT_IOC_MAGIC,  12, int)
#define VT_SET_NETDEVICE_OWNER _IOW(VT_IOC_MAGIC,  13, int)
#define VT_FREE_TRACER _IOW(VT_IOC_MAGIC,  14, int)


#define IGNORE_BLOCKED_PROCESS_SCHED_MODE

#define TRACER_TYPE_INS_VT 1
#define TRACER_TYPE_APP_VT 2

#define PROCESS_MIN_QUANTA_NS 100000


#define DEBUG_LEVEL_NONE 0
#define DEBUG_LEVEL_INFO 1
#define DEBUG_LEVEL_VERBOSE 2

#define SUCCESS 1
#define FAIL -1

//Number of Instructions per uSec or 1 instruction per nano sec
#define REF_CPU_SPEED	1000


#define MSEC_PER_SEC    1000L
#define USEC_PER_MSEC   1000L
#define NSEC_PER_USEC   1000L
#define NSEC_PER_MSEC   1000000L
#define USEC_PER_SEC    1000000L
#define NSEC_PER_SEC    1000000000L
#define FSEC_PER_SEC    1000000000000000L

#define EXP_CBE 1
#define EXP_CS 2


#ifdef KRONOS_DEBUG_INFO
#define PDEBUG_I(fmt, args...) printk(KERN_INFO "Titan: <INFO> " fmt, ## args)
#else
#define PDEBUG_I(fmt,args...)
#endif



#ifdef KRONOS_DEBUG_VERBOSE
#define PDEBUG_V(fmt,args...) printk(KERN_INFO "Titan: <VERBOSE> " fmt, ## args)
#else
#define PDEBUG_V(fmt,args...) 
#endif

#define PDEBUG_A(fmt, args...) printk(KERN_INFO "Titan: <NOTICE> " fmt, ## args)
#define PDEBUG_E(fmt, args...) printk(KERN_ERR "Titan: <ERROR> " fmt, ## args)



#ifdef ENABLE_IRQ_LOCKING

#define acquire_irq_lock(lock,flags) \
	do {															 \
		spin_lock_irqsave(lock,flags);								 \
	} while(0)


#define release_irq_lock(lock, flags) \
	do {															 \
		spin_unlock_irqrestore(lock,flags);								 \
	} while(0)

#else

#ifdef ENABLE_LOCKING

#define acquire_irq_lock(lock,flags) \
		do {							\
			spin_lock(lock);				\
		} while(0)


#define release_irq_lock(lock, flags) \
		do {															 \
		spin_unlock(lock);				\
		} while(0)

#else

#define acquire_irq_lock(lock,flags) \
		do {							\
		} while(0)


#define release_irq_lock(lock, flags) \
		do {															 \
		} while(0)


#endif

#endif

typedef unsigned long long u64;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef short int int16_t;
typedef unsigned short int uint16_t;
typedef char int8_t;

#endif
