#ifndef TIMER_H_
#define TIMER_H_

#include "syshead.h"
#include "utils.h"
#include "list.h"
#include "../VT_functions.h"
#define NS_PER_MS 1000000

#define timer_dbg(msg, t)                                               \
    do {                                                                \
        print_debug("Timer at %d: "msg": expires %d", tick, t->expires); \
    } while (0)

struct timer {
    struct list_head list;
    int refcnt;
    s64 expires;
    int cancelled;
    int oneshot;
    void *(*handler)(void *);
    void *arg;
};

struct timer *TimerAdd(uint32_t expire, void *(*handler)(void *), void *arg);
void TimerOneshot(uint32_t expire, void *(*handler)(void *), void *arg);
void TimerRelease(struct timer *t);
void TimerCancel(struct timer *t);
void TimersProcessTick();
void TimersCleanup();
s64 TimerGetTick();
                   
#endif
