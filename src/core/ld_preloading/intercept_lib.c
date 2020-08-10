#include "includes.h"
#include "socket_utils.h"
#include "vt_management.h"



typedef int (*pthread_create_t)(pthread_t *, const pthread_attr_t *,
                                void *(*)(void*), void *);

void __attribute__ ((noreturn)) (*original_pthread_exit)(void * ret_val) = NULL;

int (*orig_gettimeofday)(struct timeval *tv, struct timezone *tz) = NULL;

int (*orig_clock_gettime)(clockid_t clk_id, struct timespec *tp) = NULL;

unsigned int (*orig_sleep)(unsigned int seconds) = NULL;

int (*orig_select)(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout) = NULL;

int (*orig_poll)(struct pollfd *fds, nfds_t nfds, int timeout) = NULL;

pid_t (*orig_fork)(void) = NULL;

int (*orig_futex)(int *uaddr, int futex_op, int val,
                 const struct timespec *timeout,   /* or: uint32_t val2 */
                 int *uaddr2, int val3) = NULL;

long (*orig_syscall)(long number, ...);


/*** Orig socket fns ***/
ssize_t (*orig_send)(int sockfd, const void *buf, size_t len, int flags);

ssize_t (*orig_sendto)(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);

int (*orig_socket)(int domain, int type, int protocol);

int (*orig_close)(int fd);

int (*orig_accept)(int sockfd, struct sockaddr * addr, socklen_t * addrlen);

int (*orig_accept4)(int sockfd, struct sockaddr * addr, socklen_t * addrlen,
					int flags);

/*** Orig timerfd fns ***/

int (*orig_timerfd_create)(int clockid, int flags);

int (*orig_timerfd_settime)(int fd, int flags, 
							const struct itimerspec *new_value,
                           struct itimerspec *old_value);

int (*orig_timerfd_gettime)(int fd, struct itimerspec *curr_value);

void __attribute__ ((noreturn)) (*orig_exit)(int status);

// externs
extern s64 * globalCurrBurstLength;
extern s64 * globalCurrBBID;
extern s64 * globalCurrBBSize;
extern int * vtInitialized;
extern int * vtAlwaysOn;
extern int * vtGlobalTracerID;
extern int * vtGlobalTimelineID;
extern void (*vtAfterForkInChild)(int ThreadID, int ParendProcessPID);
extern void (*vtThreadStart)(int ThreadID);
extern void (*vtThreadFini)(int ThreadID);
extern void (*vtAppFini)(int ThreadID);
extern void (*vtInitialize)();
extern void (*vtYieldVTBurst)(int ThreadID, int save, long syscall_number);
extern void (*vtForceCompleteBurst)(int ThreadID, int save, long syscall_number);

extern void (*vtTriggerSyscallWait)(int ThreadID, int save);
extern void (*vtTriggerSyscallFinish)(int ThreadID);
extern void (*vtSleepForNS)(int ThreadID, s64 duration);
extern void (*vtGetCurrentTimespec)(struct timespec *ts);
extern void (*vtGetCurrentTimeval)(struct timeval * tv);
extern void (*vtSetPktSendTime)(int payloadHash, int payloadLen, s64 send_tstamp);
extern s64 (*vtGetCurrentTime)();
extern void (*vt_ns_2_timespec)(s64 nsec, struct timespec * ts);

/*** For Socket Handling ***/
extern void  (*vtAddSocket)(int ThreadID, int sockFD, int isNonBlocking);
extern int (*vtIsSocketFd)(int ThreadID, int sockFD);
extern int (*vtIsSocketFdNonBlocking)(int ThreadID, int sockFD);

/*** For TimerFD Handlng ***/
extern void  (*vtAddTimerFd)(int ThreadID, int fd, int isNonBlocking);
extern int (*vtIsTimerFd)(int ThreadID, int fd);
extern int (*vtIsTimerFdNonBlocking)(int ThreadID, int fd);
extern int (*vtIsTimerArmed)(int ThreadID, int fd);
extern int (*vtGetNxtTimerFdNumber)(int ThreadID);


extern void (*vtSetTimerFdParams)(int ThreadID, int fd, s64 absExpiryTime,
                         s64 intervalNS, s64 relExpDuration);
extern void (*vtGetTimerFdParams)(int ThreadID, int fd, s64* absExpiryTime,
                         s64* intervalNS, s64* relExpDuration);
extern int (*vtGetNumNewTimerFdExpiries)(int ThreadID, int fd,
                                s64 * nxtExpiryDurationNS);
extern int (*vtComputeClosestTimerExpiryForSelect)(int ThreadID,
    fd_set * readfs, int nfds, s64 * closestTimerExpiryDurationNS);
extern int (*vtComputeClosestTimerExpiryForPoll)(int ThreadID,
	struct pollfd * fds, int nfds, s64 * closestTimerExpiryDurationNS);


/*** For Sockets and TimerFD ***/
extern void (*vtCloseFd)(int ThreadID, int fd);

/*** For lookahead handling ***/
extern s64 (*vtGetPacketEAT)();
extern long (*vtGetBBLLookahead)();
extern int (*vtSetLookahead)(s64 bulkLookaheadValue, long spLookaheadValue); 

void LoadOrigFunctions();


struct Thunk {
    void *(*start_routine)(void *);
    void *arg;
};

//! Thread function calls are wrapped with this function
static void * WrapThreadFn(void *thunk_vp) {
    
    struct Thunk *thunk_p = (struct Thunk *)thunk_vp;
	int ThreadPID = syscall(SYS_gettid);
	
	vtThreadStart(ThreadPID);

    void *result = (thunk_p->start_routine)(thunk_p->arg);

	free(thunk_p);
	
	vtThreadFini(ThreadPID);
    return result;
}

//! Overloaded pthread_create function
int pthread_create(pthread_t *thread ,const pthread_attr_t *attr,
               	   void *(*start_routine)(void*), void *arg) {
    static pthread_create_t real_pthread_create;
    

    if (!real_pthread_create)
    {
        real_pthread_create
            = (pthread_create_t)dlsym(RTLD_NEXT, "pthread_create");
        if (!real_pthread_create)
            abort(); // "impossible"
    }

	int ParentThreadID = syscall(SYS_gettid);
    struct Thunk *thunk_p = malloc(sizeof(struct Thunk));
    thunk_p->start_routine = start_routine;
    thunk_p->arg = arg;
    return real_pthread_create(thread, attr, WrapThreadFn, thunk_p);
}


/* LD_PRELOAD override that causes normal process termination to instead result
 * in abnormal process termination through a raised SIGABRT signal via abort(3)
 * (even if SIGABRT is ignored, or is caught by a handler that returns).
 */

void MyExit(void) {
   int ThreadPID = syscall(SYS_gettid);
   printf("Main thread exiting. PID = %d\n", ThreadPID);
   fflush(stdout);
   vtAppFini(ThreadPID);
}

/* The address of the real main is stored here for FakeMain to access */
static int (*real_main) (int, char **, char **);

/* FakeMain(), spliced in before the real call to main() in __libc_start_main */
static int FakeMain(int argc, char **argv, char **envp)
{	
	/* Register abort(3) as an atexit(3) handler to be called at normal
	 * process termination */
	atexit(MyExit);
	int ThreadPID = syscall(SYS_gettid);
    printf ("Starting main program: Pid = %d\n", ThreadPID);
	fflush(stdout);
	InitializeVtlLogic();
	printf("Successfully initialized VTL-Logic embedded into the executable\n");
	fflush(stdout);
	*vtInitialized = 1;
	vtAfterForkInChild(ThreadPID, ThreadPID);
	/* Finally call the real main function */
	return real_main(argc, argv, envp);
}

/* LD_PRELOAD override of __libc_start_main.
 *
 * The objective is to splice FakeMain above to be executed instead of the
 * program main function. We cannot use LD_PRELOAD to override main directly as
 * LD_PRELOAD can only be used to override functions in dynamically linked
 * shared libraries whose addresses are determined via the Procedure
 * Linkage Table (PLT). However, main's location is not determined via the PLT,
 * but is statically linked to the executable entry routine at __start which
 * pushes main's address onto the stack, then invokes libc's startup routine,
 * which obtains main's address from the stack. 
 * 
 * Instead, we use LD_PRELOAD to override libc's startup routine,
 * __libc_start_main, which is normally responsible for calling main. We can't
 * just run our setup code *here* because the real __libc_start_main is
 * responsible for setting up the C runtime environment, so we can't rely on
 * standard library functions such as malloc(3) or atexit(3) being available
 * yet. 
 */
int __libc_start_main(int (*main) (int, char **, char **),
		      int argc, char **ubp_av, void (*init) (void),
		      void (*fini) (void), void (*rtld_fini) (void),
		      void (*stack_end))
{
	void *libc_handle, *sym;
    	void * lib_vt_clock_handle;
	/* This type punning is unfortunately necessary in C99 as casting
	 * directly from void* to function pointers is left undefined in C99.
	 * Strictly speaking, the conversion via union is still undefined
	 * behaviour in C99 (C99 Section 6.2.6.1):
	 * 
	 *  "When a value is stored in a member of an object of union type, the
	 *  bytes of the object representation that do not correspond to that
	 *  member but do correspond to other members take unspecified values,
	 *  but the value of the union object shall not thereby become a trap
	 *  representation."
	 * 
	 * However, this conversion is valid in GCC, and dlsym() also in effect
	 * mandates these conversions to be valid in POSIX system C compilers.
	 * 
	 * C11 explicitly allows this conversion (C11 Section 6.5.2.3): 
	 *  
	 *  "If the member used to read the contents of a union object is not
	 *  the same as the member last used to store a value in the object, the
	 *  appropriate part of the object representation of the value is
	 *  reinterpreted as an object representation in the new type as
	 *  described in 6.2.6 (a process sometimes called ‘‘type punning’’).
	 *  This might be a trap representation.
	 * 
	 * Some compilers allow direct conversion between pointers to an object
	 * or void to a pointer to a function and vice versa. C11's annex “J.5.7
	 * Function pointer casts lists this as a common extension:
	 * 
	 *   "1 A pointer to an object or to void may be cast to a pointer to a
	 *   function, allowing data to be invoked as a function (6.5.4).
         * 
	 *   2 A pointer to a function may be cast to a pointer to an object or
	 *   to void, allowing a function to be inspected or modified (for
	 *   example, by a debugger) (6.5.4)."
	 */
	union {
		int (*fn) (int (*main) (int, char **, char **), int argc,
			   char **ubp_av, void (*init) (void),
			   void (*fini) (void), void (*rtld_fini) (void),
			   void (*stack_end));
		void *sym;
	} real_libc_start_main;


	/* Obtain handle to libc shared library. The object should already be
	 * resident in the programs memory space, hence we can attempt to open
	 * it without loading the shared object. If this fails, we are most
	 * likely dealing with another version of libc.so */
	libc_handle = dlopen("libc.so.6", RTLD_NOLOAD | RTLD_NOW);

	if (!libc_handle) {
		fprintf(stderr, "can't open handle to libc.so.6: %s\n",
			dlerror());
		/* We dare not use abort() here because it would run atexit(3)
		 * handlers and try to flush stdio. */
		_exit(EXIT_FAILURE);
	}
	
	/* Our LD_PRELOAD will overwrite the real __libc_start_main, so we have
	 * to look up the real one from libc and invoke it with a pointer to the
	 * fake main we'd like to run before the real main function. */
	sym = dlsym(libc_handle, "__libc_start_main");
	if (!sym) {
		fprintf(stderr, "can't find __libc_start_main():%s\n",
			dlerror());
		_exit(EXIT_FAILURE);
	}


    orig_exit = dlsym(libc_handle, "_exit");
	if (!orig_exit) {
		fprintf(stderr, "can't find _exit():%s\n",
			dlerror());
		_exit(EXIT_FAILURE);
	}


    orig_syscall = dlsym(libc_handle, "syscall");
	if (!orig_syscall) {
		fprintf(stderr, "can't find syscall():%s\n",
			dlerror());
		_exit(EXIT_FAILURE);
	}

	orig_send = dlsym(libc_handle, "send");
	if (!orig_send) {
		fprintf(stderr, "can't find send():%s\n", dlerror());
		_exit(EXIT_FAILURE);
	}


	orig_sendto = dlsym(libc_handle, "sendto");
	if (!orig_sendto) {
		fprintf(stderr, "can't find sendto():%s\n", dlerror());
		_exit(EXIT_FAILURE);
	}


	orig_socket = dlsym(libc_handle, "socket");
	if (!orig_socket) {
		fprintf(stderr, "can't find socket():%s\n", dlerror());
		_exit(EXIT_FAILURE);
	}

	orig_close = dlsym(libc_handle, "close");
	if (!orig_close) {
		fprintf(stderr, "can't find close(int fd):%s\n", dlerror());
		_exit(EXIT_FAILURE);
	}

	orig_accept = dlsym(libc_handle, "accept");
	if (!orig_accept) {
		fprintf(stderr, "can't find accept(...):%s\n", dlerror());
		_exit(EXIT_FAILURE);
	}

	orig_accept4 = dlsym(libc_handle, "accept4");
	if (!orig_accept4) {
		fprintf(stderr, "can't find accept4(...):%s\n", dlerror());
		_exit(EXIT_FAILURE);
	}

	orig_timerfd_create = dlsym(libc_handle, "timerfd_create");
	if (!orig_timerfd_create) {
		fprintf(stderr, "can't find timerfd_create(...):%s\n", dlerror());
		_exit(EXIT_FAILURE);
	}

	orig_timerfd_settime = dlsym(libc_handle, "timerfd_settime");
	if (!orig_timerfd_settime) {
		fprintf(stderr, "can't find timerfd_settime(...):%s\n", dlerror());
		_exit(EXIT_FAILURE);
	}

	orig_timerfd_gettime = dlsym(libc_handle, "timerfd_gettime");
	if (!orig_timerfd_gettime) {
		fprintf(stderr, "can't find timerfd_gettime(...):%s\n", dlerror());
		_exit(EXIT_FAILURE);
	}

	real_libc_start_main.sym = sym;
	real_main = main;
	
	/* Close our handle to dynamically loaded libc. Since the libc object
	 * was already loaded previously, this only decrements the reference
	 * count to the shared object. Hence, we can be confident that the
	 * symbol to the read __libc_start_main remains valid even after we
	 * close our handle. In order to strictly adhere to the API, we could
	 * defer closing the handle to our spliced-in fake main before it call
	 * the real main function. */
	if(dlclose(libc_handle)) {
		fprintf(stderr, "can't close handle to libc.so.6: %s\n",
			dlerror());
		_exit(EXIT_FAILURE);
	}

	printf("Loading orig functions !\n");
	LoadOrigFunctions();

	/* Note that we swap FakeMain in for main - FakeMain should call
	 * real_main after its setup is done. */
	return real_libc_start_main.fn(FakeMain, argc, ubp_av, init, fini,
				       rtld_fini, stack_end);
}

//! Get pointer to original pthread_exit
void LoadOrigPThreadExit() {
    original_pthread_exit = dlsym(RTLD_NEXT, "pthread_exit");
    if (!original_pthread_exit)
            abort();
}


void __attribute__ ((noreturn)) _exit(int status) {
	int ThreadPID = syscall(SYS_gettid);
	printf("I am alternate exiting main thread %d\n", ThreadPID);
	orig_exit(status);
}

//! Overloaded pthread_exit
void  __attribute__ ((noreturn))  pthread_exit(void *retval) {
    if (!original_pthread_exit) {
		LoadOrigPThreadExit();
    }

	int ThreadPID = syscall(SYS_gettid);
    printf ("Calling my pthread exit for PID = %d\n", ThreadPID);
    vtThreadFini(ThreadPID);
    original_pthread_exit(retval);
}

// Load pointers to several original functions such as send, sendto, recv etc
void LoadOrigFunctions() {
	void *libc_handle;
	libc_handle = dlopen("libc.so.6", RTLD_NOLOAD | RTLD_NOW);
	if (!libc_handle) {
		fprintf(stderr, "can't open handle to libc.so.6: %s\n", dlerror());
		/* We dare not use abort() here because it would run atexit(3)
		   handlers and try to flush stdio. */
		_exit(EXIT_FAILURE);
	}
	
	if (!orig_fork) {	
		orig_fork = dlsym(libc_handle, "fork");
		if (!orig_fork) {
			fprintf(stderr, "can't find fork():%s\n", dlerror());
			_exit(EXIT_FAILURE);
		}
	}

	if (!orig_gettimeofday) {
		orig_gettimeofday = dlsym(libc_handle, "gettimeofday");
		if (!orig_gettimeofday) {
			fprintf(stderr, "can't find gettimeofday():%s\n", dlerror());
			_exit(EXIT_FAILURE);
		}
	}

	if (!orig_clock_gettime) {
		orig_clock_gettime = dlsym(libc_handle, "clock_gettime");
		if (!orig_clock_gettime) {
			fprintf(stderr, "can't find orig_clock_gettime():%s\n", dlerror());
			_exit(EXIT_FAILURE);
		}

	}

	if (!orig_sleep) {
		orig_sleep = dlsym(libc_handle, "sleep");
		if (!orig_sleep) {
			fprintf(stderr, "can't find orig_sleep():%s\n", dlerror());
			_exit(EXIT_FAILURE);
		}
	}

	if (!orig_poll) {
		orig_poll = dlsym(libc_handle, "poll");
		if (!orig_poll) {
			fprintf(stderr, "can't find orig_poll():%s\n", dlerror());
			_exit(EXIT_FAILURE);
		}
	}

	if (!orig_select) {
		orig_select = dlsym(libc_handle, "select");
		if (!orig_select) {
			fprintf(stderr, "can't find orig_select():%s\n", dlerror());
			_exit(EXIT_FAILURE);
		}
	}


	if(dlclose(libc_handle)) {
		fprintf(stderr, "can't close handle to libc.so.6: %s\n", dlerror());
		_exit(EXIT_FAILURE);
	}
}

//! OVerloaded fork function
pid_t fork() {
	int ParentPID = syscall(SYS_getpid);
	int ParentTID = syscall(SYS_gettid);
	printf("Before fork in parent: %d\n", ParentTID);
	fflush(stdout);
	pid_t ret;
	
	ret = orig_fork();
	if (ret == 0) {
		int ChildPID = syscall(SYS_gettid);
		printf("After fork in child: PID = %d\n", ChildPID);
		vtAfterForkInChild(ChildPID, ParentPID);
	} else {
		printf("After fork in parent: PID = %d\n", ParentTID);
		vtForceCompleteBurst(ParentTID, 1, SYS_fork);
	}

	return ret;
}

/*** Timerfd functions ***/

//! Overloaded timerfd_create function
int timerfd_create(int clockid, int flags) {
	int ThreadID = syscall(SYS_gettid);
	int alottedFd = vtGetNxtTimerFdNumber(ThreadID);

	
	if (alottedFd) {
		int isNonBlocking = (flags & TFD_NONBLOCK != 0) ? 1: 0;
		vtAddTimerFd(ThreadID, alottedFd, isNonBlocking);
		return alottedFd;
	}
	errno = EMFILE;
	return -1;

}

//! Overloaded timerfd_settime function
int timerfd_settime(int fd, int flags,
                    const struct itimerspec *new_value,
                    struct itimerspec *old_value) {
	int ThreadID = syscall(SYS_gettid);
	s64 currAbsExpiryTime, currIntervalNS, relExpDuration;
	if (!vtIsTimerFd(ThreadID, fd)) {
		errno = -EBADF;
		return -1;
	}

	if (!new_value) {
		errno = -EINVAL;
		return -1;
	}

	if (flags & TFD_TIMER_ABSTIME) {
		currAbsExpiryTime = (s64)new_value->it_value.tv_sec*NSEC_PER_SEC 
			+ new_value->it_value.tv_nsec;
	} else {
		currAbsExpiryTime =
			vtGetCurrentTime() + (s64)new_value->it_value.tv_sec*NSEC_PER_SEC
				+ new_value->it_value.tv_nsec;
	}

	relExpDuration = currAbsExpiryTime - vtGetCurrentTime();

	if (relExpDuration <= 0)
		relExpDuration = 0;
	currIntervalNS = (s64)new_value->it_interval.tv_sec*NSEC_PER_SEC
					 + (s64)new_value->it_interval.tv_nsec;

	if (old_value) {
		vtGetTimerFdParams(ThreadID, fd, &currAbsExpiryTime, &currIntervalNS,
						   &relExpDuration);

		vt_ns_2_timespec(relExpDuration, &old_value->it_value);
		vt_ns_2_timespec(currIntervalNS, &old_value->it_interval);
	}

	vtSetTimerFdParams(ThreadID, fd, currAbsExpiryTime, currIntervalNS,
						relExpDuration);

	return 0;

}

//! Overloaded timerfd_gettime function
int timerfd_gettime(int fd, struct itimerspec *curr_value) {

	int ThreadID = syscall(SYS_gettid);
	if (!vtIsTimerFd(ThreadID, fd)) {
		errno = -EBADF;
		return -1;
	}

	if(!curr_value) {
		errno = -EINVAL;
		return -1;
	}

	s64 currAbsExpiryTime, currIntervalNS, relExpDuration;
	vtGetTimerFdParams(ThreadID, fd, &currAbsExpiryTime, &currIntervalNS,
						&relExpDuration);

	vt_ns_2_timespec(relExpDuration, &curr_value->it_value);
	vt_ns_2_timespec(currIntervalNS, &curr_value->it_interval);

	return 0;
}

/***** Socket Functions ****/

//! Overloaded accept function/syscall
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {

	int ThreadID = syscall(SYS_gettid);
	int ret = orig_accept(sockfd, addr, addrlen);

	if(ret) {
		vtAddSocket(ThreadID, ret, 0);
	}

	return ret;
} 

//! Overloaded accept4 function/syscall
int accept4(int sockfd, struct sockaddr *addr,
                   socklen_t *addrlen, int flags) {
	int ThreadID = syscall(SYS_gettid);
	int ret = orig_accept4(sockfd, addr, addrlen, flags);

	if(ret) {
		int isNonBlocking = (flags & SOCK_NONBLOCK != 0) ? 1: 0; 
		vtAddSocket(ThreadID, ret, isNonBlocking);
	}

	return ret;

}

//! Overloaded send syscall
ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
	int payload_hash;
	ssize_t ret = orig_send(sockfd, buf, len, flags);
	if (len > 0 && ret > 0) {
		payload_hash = GetPayloadHash(buf, ret);
		if (payload_hash) {
			vtSetPktSendTime(payload_hash, ret, vtGetCurrentTime());
		}
	}
	return ret;
}


//! Overloaded sendto syscall
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {

	int payload_hash;
	ssize_t ret = orig_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
	if (len > 0 && ret > 0) {
		payload_hash = GetPayloadHash(buf, ret);
		if (payload_hash) {
			vtSetPktSendTime(payload_hash, ret, vtGetCurrentTime());
		}
	}
	return ret;

}

//! Overloaded socket() syscall
int socket(int domain, int type, int protocol) {
	int retFD = orig_socket(domain, type, protocol);

	if(!vtInitialized || *vtInitialized != 1)
		return retFD;

	if(retFD) {
		int ThreadPID = syscall(SYS_gettid);
		int isNonBlocking = (type & SOCK_NONBLOCK) != 0 ? 1: 0;
		vtAddSocket(ThreadPID, retFD, isNonBlocking);
	}

	return retFD;
}

//! Overloaded close() syscall
int close(int fd) {
	if(!vtInitialized || *vtInitialized != 1)
		return orig_close(fd);

	int ThreadPID = syscall(SYS_gettid);
	int isTimer = vtIsTimerFd(ThreadPID, fd);
	if (vtIsSocketFd(ThreadPID, fd) || isTimer) {
		vtCloseFd(ThreadPID, fd);
	}

	if (!isTimer)
		return orig_close(fd);
	else
		return 0;
}

/***** Time related functions ****/

//! Overloaded gettimeofday syscall
int gettimeofday(struct timeval *tv, struct timezone *tz) {
	int ThreadPID =  syscall(SYS_gettid);
	vtGetCurrentTimeval(tv);
	return 0;
}

//! Overloaded clock_gettime syscall
int clock_gettime(clockid_t clk_id, struct timespec *tp) {
	int ThreadPID =  syscall(SYS_gettid);
	vtGetCurrentTimespec(tp);
	return 0;
}

//! Overloaded sleep syscall
unsigned int sleep(unsigned int seconds) {
	int ThreadPID = syscall(SYS_gettid);
	if (seconds) {
		vtSetLookahead(seconds*NSEC_PER_SEC + vtGetCurrentTime(),
						vtGetBBLLookahead());
		vtSleepForNS(ThreadPID, seconds*NSEC_PER_SEC);
	}
	return 0;
}

//! Overloaded usleep syscall
int usleep(useconds_t usec) {
	int ThreadPID = syscall(SYS_gettid);
	if (usec) {
		vtSetLookahead(usec*NSEC_PER_US + vtGetCurrentTime(),
						vtGetBBLLookahead());
		vtSleepForNS(ThreadPID, usec*NSEC_PER_US);
	}
	
	return 0;
}

//! Overloaded poll syscall
int poll(struct pollfd *fds, nfds_t nfds, int timeout_ms){
	
	int ret, ThreadPID;
	fd_set tfd_store;
	nfds_t actual_nfds = 0;
	int i =0 , j = 0;
	int maxfd = -1;
	int tfd_to_idx[MAX_NUM_FDS_PER_PROCESS + 1]; 
	int relevantTimer;
	s64 start_time, elapsed_time, max_duration, minNxtTimerExpiry;
	s64 timeoutNS;
	int allSpecialFDS = 1;
	ThreadPID = syscall(SYS_gettid);

	if(!vtInitialized || *vtInitialized != 1)
		return orig_poll(fds, nfds, timeout_ms);

	if (nfds > MAX_NUM_FDS_PER_PROCESS) { // only works up 1024 file descriptors
		errno = -EINVAL;
		return -1;
	}

	

	timeoutNS = (s64)timeout_ms * NSEC_PER_MS;
	printf("In overloaded poll: %d, timeoutNS: %llu\n", ThreadPID, timeoutNS);
	FD_ZERO(&tfd_store);
	memset(tfd_to_idx, 0, sizeof(int)*MAX_NUM_FDS_PER_PROCESS + 1);

	for(i = 0; i < nfds; i++) {

		if (fds[i].fd > MAX_NUM_FDS_PER_PROCESS) { // only works up 1024 file descriptors
			errno = -EINVAL;
			return -1;
		}

		if(!vtIsTimerFd(ThreadPID, fds[i].fd)) {
			actual_nfds ++;

			if(!vtIsSocketFd(ThreadPID, fds[i].fd)) {
				allSpecialFDS = 0;
			} else {
				// if there is a socket, it should only be readable
				if(fds[i].events != POLLIN)
					allSpecialFDS = 0;
			}
		} else {
			// this is a TimerFD;
			if (fds[i].fd > maxfd)
				maxfd = fds[i].fd + 1;
			
			FD_SET(fds[i].fd, &tfd_store);
			tfd_to_idx[fds[i].fd] = i;
		}
		
	}


	if (!actual_nfds) {
		// all are timerfds
		relevantTimer = vtComputeClosestTimerExpiryForPoll(ThreadPID, fds, nfds,
							           &minNxtTimerExpiry);
		assert(relevantTimer > 0);

		if (timeoutNS < 0) {
			max_duration = minNxtTimerExpiry;
		} else {
			if (timeoutNS > minNxtTimerExpiry) {
				max_duration = minNxtTimerExpiry;
			} else {
				max_duration = timeoutNS;
			}
		}
		// there is some timer due to expire in minNxtTimerExpiry nanosec
		if (max_duration)
			vtSleepForNS(ThreadPID, max_duration);

		for(i = 0; i < nfds; i++) {
			if(fds[i].fd == relevantTimer)
				fds[i].revents |= POLLIN;
		}

		return 1;

	} else if (actual_nfds < nfds) {
		relevantTimer = vtComputeClosestTimerExpiryForPoll(ThreadPID, fds, nfds,
								   &minNxtTimerExpiry);
		assert(relevantTimer > 0);	

	} else {
		relevantTimer = 0;

		if  (timeoutNS < 0)
			minNxtTimerExpiry = timeoutNS;
		else
			minNxtTimerExpiry = timeoutNS;
	}

	if (timeoutNS == (s64)0) {
		max_duration = 0;
	} else if(timeoutNS < (s64)0) {
		max_duration = minNxtTimerExpiry;
	} else if (minNxtTimerExpiry > timeoutNS) {
		max_duration =  timeoutNS;
	} else {
		max_duration = minNxtTimerExpiry;
	}

	for(i = 0; i < nfds; i++) {
		if(FD_ISSET(fds[i].fd, &tfd_store)) {
			// force-set to zero. All of this is to avoid using malloc here.
			fds[i].fd = 0;
			fds[i].events = 0;
			fds[i].revents = 0;
	
		}
	}
	
	if (max_duration == 0) {
		ret = orig_poll(fds, nfds, 0);
	} else if (max_duration < 0) {
		vtTriggerSyscallWait(ThreadPID, 1);
		ret = orig_poll(fds, nfds, -1);
		vtTriggerSyscallFinish(ThreadPID);
	} else {
		
		start_time = vtGetCurrentTime();
		long bblLA = vtGetBBLLookahead();
	
		vtTriggerSyscallWait(ThreadPID, 1);
		do {
			ret = orig_poll(fds, nfds, 0);
			if (ret != 0)
				break;

 
			elapsed_time = vtGetCurrentTime() - start_time;

			if (elapsed_time < max_duration && allSpecialFDS) {
				s64 minBulkLA = start_time + max_duration;
				s64 pktEAT = vtGetPacketEAT();
				if (minBulkLA > pktEAT)
					minBulkLA = pktEAT;

				vtSetLookahead(minBulkLA, bblLA);
				allSpecialFDS = 0;
			}

			if (elapsed_time < max_duration)
				vtTriggerSyscallWait(ThreadPID, 0);
		} while (elapsed_time < max_duration);
		vtTriggerSyscallFinish(ThreadPID);
	}

	if (ret == 0 && relevantTimer) {
		// Timer has expired first. set its revent.
		for(i = 0; i < maxfd; i++) {
			if (FD_ISSET(i, &tfd_store)) {
				// this is a timerfd descriptor.
				// restore here				
				fds[tfd_to_idx[i]].fd = i;
				fds[tfd_to_idx[i]].events |= POLLIN;
				if (i == relevantTimer) {
					fds[tfd_to_idx[i]].revents |= POLLIN;
					ret = 1;
				}
				
			}

			
		}

	}
	return ret;
}


//! Overloaded select syscall
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout) {

	int ret, ThreadPID, rem;
	int isReadInc, isWriteInc, isExceptInc;
	s64 start_time, elapsed_time, max_duration, orig_max_duration,
		minNxtTimerExpiry;
	fd_set readfds_copy, writefds_copy, exceptfds_copy, readfs_tmp;
	fd_set * readfds_mod = NULL;

	struct timeval timeout_cpy;
	int actual_nfds = 0;
	int i = 0, j = 0, relevantTimer = 0, num_timers = 0, isTimer;
	int allSpecialFDS = 1;

	if(!vtInitialized || *vtInitialized != 1)
		return orig_select(nfds, readfds, writefds, exceptfds, timeout);


	ThreadPID = syscall(SYS_gettid);

	printf("In overloaded select: %d\n", ThreadPID);
	fflush(stdout);

	if (readfds) {
		readfds_mod = &readfs_tmp;
		FD_ZERO(readfds_mod);
		for(i = 0; i < nfds; i++) {
			if(FD_ISSET(i, readfds)) {
				isTimer = vtIsTimerFd(ThreadPID, i);
				if (!isTimer && actual_nfds <= i)
					actual_nfds = i;

				if (!isTimer) {
					if(!vtIsSocketFd(ThreadPID, i))
						allSpecialFDS = 0;
					FD_SET(i, readfds_mod);
				} else {
					FD_CLR(i, readfds);
					FD_SET(i, &readfds_copy);
					num_timers ++;
				}
			}

		}
	} else {
		readfds_mod = NULL;
	}

	if (writefds) {
		allSpecialFDS = 0;
		for(i = 0; i < nfds; i++) {
			if(FD_ISSET(i, writefds))
				if (actual_nfds <= i)
					actual_nfds = i;
		}
	}

	if (exceptfds) {
		allSpecialFDS = 0;
		for(i = 0; i < nfds; i++) {
			if(FD_ISSET(i, exceptfds))
				if (actual_nfds <= i)
					actual_nfds = i;
		}
	}
	
	if(actual_nfds)
		actual_nfds ++;

	if (!actual_nfds && num_timers > 0) {
		// all fds are timerfds
		assert(readfds);
		relevantTimer = vtComputeClosestTimerExpiryForSelect(ThreadPID,
			&readfds_copy, nfds, &minNxtTimerExpiry);
		assert(relevantTimer > 0);
		assert(minNxtTimerExpiry >= 0);

		if (!timeout) {
			max_duration = minNxtTimerExpiry;
		} else if(timeout->tv_sec == 0 && timeout->tv_usec == 0) {
			max_duration = 0;
		} else if (minNxtTimerExpiry > 
					(s64)timeout->tv_sec * NSEC_PER_SEC 
						+ (s64)timeout->tv_usec * NSEC_PER_US) {
			max_duration = (s64)timeout->tv_sec * NSEC_PER_SEC 
							+ (s64)timeout->tv_usec * NSEC_PER_US;
		} else {
			max_duration = minNxtTimerExpiry;
		}


		// there is some timer due to expire in minNxtTimerExpiry nanosec
		if (max_duration)
			vtSleepForNS(ThreadPID, max_duration);

		FD_SET(relevantTimer, readfds);

		return 1;
	} else if (num_timers > 0) {
		// some are timers, there are some non-timer fds as well
		assert(readfds);
		relevantTimer = vtComputeClosestTimerExpiryForSelect(ThreadPID,
			readfds, nfds, &minNxtTimerExpiry);
		assert(relevantTimer > 0);
		assert(minNxtTimerExpiry >= 0);
		

	} else {
		// there are no timers
		relevantTimer = 0;
		if  (!timeout)
			minNxtTimerExpiry = -1;
		else
			minNxtTimerExpiry 
				= timeout->tv_sec * NSEC_PER_SEC 
					+ timeout->tv_usec * NSEC_PER_US;
	}

	if (!timeout) {
		max_duration = minNxtTimerExpiry;
	} else if(timeout->tv_sec == 0 && timeout->tv_usec == 0) {
		max_duration = 0;
	} else if (minNxtTimerExpiry > 
				(s64)timeout->tv_sec * NSEC_PER_SEC 
					+ (s64)timeout->tv_usec * NSEC_PER_US) {
		max_duration = (s64)timeout->tv_sec * NSEC_PER_SEC 
						+ (s64)timeout->tv_usec * NSEC_PER_US;
	} else {
		max_duration = minNxtTimerExpiry;
	}



	isReadInc = 0, isWriteInc = 0, isExceptInc = 0;

	if (max_duration < 0) {
		assert(!timeout);
		vtTriggerSyscallWait(ThreadPID, 1);
		ret = orig_select(actual_nfds, readfds_mod, writefds, exceptfds,
						  NULL);
		vtTriggerSyscallFinish(ThreadPID);
	} else if (max_duration == 0) {
			timeout_cpy.tv_sec = 0, timeout_cpy.tv_usec = 0;
			ret = orig_select(actual_nfds, readfds_mod, writefds, exceptfds,
							  &timeout_cpy); 
	} else {
		FD_ZERO(&readfds_copy);
		FD_ZERO(&writefds_copy);
		FD_ZERO(&exceptfds_copy);

		if (readfds) {
			memcpy(&readfds_copy, readfds_mod, sizeof(fd_set));
			isReadInc = 1;
		}

		if (writefds) {
			memcpy(&writefds, writefds, sizeof(fd_set));
			isWriteInc = 1;
		}

		if (exceptfds) {
			memcpy(&exceptfds_copy, exceptfds, sizeof(fd_set));
			isExceptInc = 1;
		}
		
		start_time = vtGetCurrentTime();

		if (timeout) {
			orig_max_duration = (s64)timeout->tv_sec * NSEC_PER_SEC 
						+ (s64)timeout->tv_usec * NSEC_PER_US;
			timeout->tv_sec = 0, timeout->tv_usec = 0;
		} else {
			orig_max_duration = max_duration;
		}

		long bblLA = vtGetBBLLookahead();
		vtTriggerSyscallWait(ThreadPID, 1);
		timeout_cpy.tv_sec = 0, timeout_cpy.tv_usec = 0;
		do {
			ret = orig_select(actual_nfds, readfds_mod, writefds, exceptfds,
							  &timeout_cpy);
			if (ret != 0)
				break;

			elapsed_time = vtGetCurrentTime() - start_time;

			if (elapsed_time < max_duration && allSpecialFDS) {
				s64 minBulkLA = start_time + max_duration;
				s64 pktEAT = vtGetPacketEAT();
				if (minBulkLA > pktEAT)
					minBulkLA = pktEAT;

				vtSetLookahead(minBulkLA, bblLA);
				allSpecialFDS = 0;
			} 

			if (elapsed_time < max_duration)
				vtTriggerSyscallWait(ThreadPID, 0);
			
			if (isReadInc)
				memcpy(readfds_mod, &readfds_copy, sizeof(fd_set));
			if (isWriteInc)
				memcpy(writefds, &writefds_copy, sizeof(fd_set));
			if (isExceptInc)
				memcpy(exceptfds, &exceptfds_copy, sizeof(fd_set));
		} while (elapsed_time < max_duration);
		vtTriggerSyscallFinish(ThreadPID);
		
		if (timeout && elapsed_time < orig_max_duration) {
			timeout->tv_sec = (orig_max_duration - elapsed_time) / NSEC_PER_SEC;
			rem  = (orig_max_duration - elapsed_time)
					- (s64)timeout->tv_sec * NSEC_PER_SEC;
			timeout->tv_usec = rem / NSEC_PER_US;
		}
	}

	if (readfds) {
		assert(readfds_mod);
		memcpy(readfds, readfds_mod, sizeof(fd_set));
	}

	if (ret == 0 && relevantTimer > 0) {
		// Timer expired
		FD_SET(relevantTimer, readfds);
		ret = 1;
	}

	return ret;
}

//! Generic function to handle filedescriptor reads. Depending on the type of
//  filedescriptor i.e, file, socket, timerfd the action is different.
static void HandleVtReadSyscall(int ThreadPID, int fd, void * buf,
								size_t count, long * result) {
	int goingToBlock = 1;
	if (vtInitialized && *vtInitialized == 1) {
		if (vtIsSocketFd(ThreadPID, fd)) {
			if(!vtIsSocketFdNonBlocking(ThreadPID, fd)) {
				// get eat and set lookahead here after polling if there's 
				// nothing to read right away.

				struct pollfd poll_fd;
				poll_fd.events = POLLIN;
				poll_fd.fd = fd;
				if (syscall_no_intercept(SYS_poll, &poll_fd, 1, 0) <= 0) {
					// nothing to read as of now. set lookahead here.
					s64 bulkLA = vtGetPacketEAT();
					long bblLA = vtGetBBLLookahead();
					vtSetLookahead(bulkLA, bblLA);
					goingToBlock = 1;
					
				} else{
					// there is something to read
					goingToBlock = 0;
				}
				
			} else {
				goingToBlock = 0;
			}
		} else if(vtIsTimerFd(ThreadPID, fd)) {
			if (count < sizeof(uint64_t)) {
				errno = -EINVAL;
				*result =  -1;
				return;
			}
			// it is blocking
			s64 nxtExpiryDuration;
			uint64_t numNewExpiries = 0;
			numNewExpiries = (uint64_t) vtGetNumNewTimerFdExpiries(
							ThreadPID, fd, &nxtExpiryDuration);

			if(!vtIsTimerFdNonBlocking(ThreadPID, fd)) {
				if (!numNewExpiries && nxtExpiryDuration > 0) {
					// its going to block. set lookahead.
					long bblLA = vtGetBBLLookahead();
					vtSetLookahead(nxtExpiryDuration + vtGetCurrentTime(),
								   bblLA);
					vtSleepForNS(ThreadPID, nxtExpiryDuration);
					numNewExpiries = (uint64_t) vtGetNumNewTimerFdExpiries(
							ThreadPID, fd, &nxtExpiryDuration);

				}

				memcpy(buf, &numNewExpiries, sizeof(uint64_t));
				*result = sizeof(uint64_t);
				
			} else {
				if (!numNewExpiries && nxtExpiryDuration > 0) {
					errno = -EAGAIN;
					*result = -1;
				} else
					*result = sizeof(uint64_t);
				memcpy(buf, &numNewExpiries, sizeof(uint64_t));
			}

			return;
		}
	}

	if (goingToBlock && vtInitialized && *vtInitialized == 1)
		vtYieldVTBurst(ThreadPID, 1, SYS_read);

	
	*result = syscall_no_intercept(SYS_read, fd, buf, count);

	if (goingToBlock && vtInitialized && *vtInitialized == 1)
		vtForceCompleteBurst(ThreadPID, 0, SYS_read);
}


//! Common operations to handle packet receive syscalls. Behavior is different
//  based on whether the socket file descriptor is blocking or non-blocking
static void HandleVtPacketReceiveSyscalls(long syscall_number, long arg0,
	long arg1, long arg2, long arg3, long arg4, long arg5, long *result,
	int ThreadPID) {

	if (syscall_number != SYS_recvfrom
		&& syscall_number != SYS_recvmsg
		&& syscall_number != SYS_recvmmsg
		&& syscall_number != SYS_accept
		&& syscall_number != SYS_accept4)
		return;

	int sockfd = (int) arg0;
	int goingToBlock = 1;

	if (!vtIsSocketFd(ThreadPID, sockfd)) {
		*result = syscall_no_intercept(syscall_number, arg0, arg1, arg2, arg3,
									   arg4, arg5);
		return;
	}
	if(!vtIsSocketFdNonBlocking(ThreadPID, sockfd)) {
		// get eat and set lookahead here after polling if there's 
		// nothing to read right away.
		struct pollfd poll_fd;
		poll_fd.events = POLLIN;
		poll_fd.fd = sockfd;
		if (syscall_no_intercept(SYS_poll, &poll_fd, 1, 0) <= 0) {
			// nothing to read as of now. set lookahead here.
			s64 bulkLA = vtGetPacketEAT();
			long bblLA = vtGetBBLLookahead();
			vtSetLookahead(bulkLA, bblLA);
		}
		goingToBlock = 1;
	} else {
		goingToBlock = 0;
	}

	if (goingToBlock && vtInitialized && *vtInitialized == 1)
		vtYieldVTBurst(ThreadPID, 1, SYS_read);

	
	*result = syscall_no_intercept(syscall_number, arg0, arg1, arg2, arg3, arg4,
								   arg5);

	if (goingToBlock && vtInitialized && *vtInitialized == 1)
		vtForceCompleteBurst(ThreadPID, 0, SYS_read);
}


//! Called before each syscall which may voluntariliy yield the cpu. Eg: futex
static void ExecuteCpuYieldingSyscall(long syscall_number, long arg0,
	long arg1, long arg2, long arg3, long arg4, long arg5, long *result,
	int ThreadPID) {


	switch(syscall_number) {
	
		case SYS_accept:
		case SYS_accept4:
		case SYS_recvmsg:
		case SYS_recvfrom:
		case SYS_recvmmsg:  	
					HandleVtPacketReceiveSyscalls(syscall_number,
						arg0, arg1, arg2, arg3, arg4, arg5, result, ThreadPID);
					break;
		case SYS_read:
					HandleVtReadSyscall(ThreadPID, (int)arg0, (void *)arg1,
						(size_t)arg2, result);
					break;
		default:	
					if (vtInitialized && *vtInitialized == 1)
						vtYieldVTBurst(ThreadPID, 1, syscall_number);

					*result = syscall_no_intercept(
						syscall_number, arg0, arg1, arg2, arg3, 
						arg4, arg5);

					if (vtInitialized && *vtInitialized == 1)
						vtForceCompleteBurst(ThreadPID, 0, syscall_number);

					break;
	}

}

static int hook(long syscall_number, long arg0, long arg1, long arg2, long arg3,
	long arg4, long arg5, long *result) {

	int ThreadPID = 0;
	
	if (syscall_number == SYS_futex
		|| syscall_number == SYS_connect
		|| syscall_number == SYS_accept
		|| syscall_number == SYS_accept4
		|| syscall_number == SYS_read
		|| syscall_number == SYS_wait4
		|| syscall_number == SYS_waitid
		|| syscall_number == SYS_recvfrom
		|| syscall_number == SYS_recvmmsg
		|| syscall_number == SYS_recvmsg) {

		

		if (!vtInitialized || *vtInitialized != 1)
			return 1; 

		ThreadPID = syscall_no_intercept(SYS_gettid);
		// Before cpu yielding syscall
		ExecuteCpuYieldingSyscall(syscall_number, arg0, arg1, arg2, arg3,
					    arg4, arg5, result, ThreadPID);
		// After cpu yielding syscall
		return 0;
                
	}  else {
		/*
		 * Ignore any other syscalls
		 * i.e.: pass them on to the kernel
		 * as would normally happen.
		 */
		return 1;
	}
}

static __attribute__((constructor)) void init(void) {
	// Set up the callback function

	vtInitialized = NULL;
	intercept_hook_point = hook;
}
