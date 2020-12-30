#include "syshead.h"
#include "timer.h"
#include "socket.h"
#include "netdev.h"
#include "../VT_functions.h"

static LIST_HEAD(timers);
extern int running;

#ifdef DEBUG_TIMER
static void TimerDebug() {
    struct list_head *item;
    int cnt = 0;
    list_for_each(item, &timers) {
        cnt++;
    }
    print_debug("TIMERS: Total amount currently %d", cnt);
}
#else
static void TimerDebug() {
    return;
}
#endif

static void TimerFree(struct timer *t) {
    free(t);
}

static struct timer *TimerAlloc() {
    struct timer *t = calloc(sizeof(struct timer), 1);
    return t;
}

void TimersProcessTick() {
    struct list_head *item, *tmp = NULL;
    struct timer *t = NULL;
    s64 curr_time = GetCurrentTimeTracer(GetStackTracerID());
    list_for_each_safe(item, tmp, &timers) {
        if (!item) continue;
        t = list_entry(item, struct timer, list);
        
        if (!t->cancelled && t->expires < curr_time) {
            t->cancelled = 1;
            t->handler(t->arg);
        }

        if (t->cancelled && t->refcnt == 0) {
            ListDel(&t->list);
            TimerFree(t);
        }
    }
}

void TimersCleanup() {
    struct list_head *item, *tmp = NULL;
    struct timer *t = NULL;

    
    list_for_each_safe(item, tmp, &timers) {
        if (!item) continue;
        t = list_entry(item, struct timer, list);
        ListDel(&t->list);

        /*if (!t->cancelled) {
            printf ("Calling handler at clean-up!\n");
            t->handler(t->arg);
        }*/
        TimerFree(t);
    }
}

void TimerOneshot(uint32_t expire_ms, void *(*handler)(void *), void *arg) {
    struct timer *t = TimerAlloc();
    t->refcnt = 0;
    t->expires = GetCurrentTimeTracer(GetStackTracerID()) + expire_ms * NS_PER_MS;
    t->cancelled = 0;
    t->handler = handler;
    t->arg = arg;
    ListAddTail(&t->list, &timers);
    
}

struct timer *TimerAdd(uint32_t expire_ms, void *(*handler)(void *), void *arg) {
    struct timer *t = TimerAlloc();
    s64 expires_ns = expire_ms;
    expires_ns = expires_ns * NS_PER_MS;
    t->refcnt = 1;
    t->expires = GetCurrentTimeTracer(GetStackTracerID()) + expires_ns;
    t->cancelled = 0;
    t->handler = handler;
    t->arg = arg;
    ListAddTail(&t->list, &timers);
    
    return t;
}

void TimerRelease(struct timer *t) {
    if (!t) return;
    t->refcnt--;
}

void TimerCancel(struct timer *t) {
    if (!t) return;
    t->refcnt--;
    t->cancelled = 1;      
}

s64 TimerGetTick() {
    return GetCurrentTimeTracer(GetStackTracerID());
}
