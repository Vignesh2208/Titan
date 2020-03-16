


#include "includes.h"
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

void __attribute__ ((noreturn)) (*orig_exit)(int status);

// externs
extern s64 * globalCurrBurstLength;
extern s64 * globalCurrBBID;
extern s64 * globalPrevBBID;
extern s64 * globalCurrBBSize;
extern int * vtInitialized;
extern int * vtAlwaysOn;
extern int * vtGlobalTracerID;
extern int * vtGlobalTimelineID;
extern void (*vtAfterForkInChild)(int ThreadID);
extern void (*vtThreadStart)(int ThreadID);
extern void (*vtThreadFini)(int ThreadID);
extern void (*vtAppFini)(int ThreadID);
extern void (*vtMarkCurrBBL)(int ThreadID);
extern void (*vtInitialize)();
extern void (*vtYieldVTBurst)(int ThreadID, int save, long syscall_number);
extern void (*vtForceCompleteBurst)(int ThreadID, int save, long syscall_number);

extern void (*vtTriggerSyscallWait)(int ThreadID, int save);
extern void (*vtTriggerSyscallFinish)(int ThreadID);
extern void (*vtSleepForNS)(int ThreadID, s64 duration);
extern void (*vtGetCurrentTimespec)(struct timespec *ts);
extern void (*vtGetCurrentTimeval)(struct timeval * tv);
extern s64 (*vtGetCurrentTime)();

void load_orig_functions();


struct Thunk {
    void *(*start_routine)(void *);
    void *arg;
};


static void * wrap_threadFn(void *thunk_vp) {
    
    struct Thunk *thunk_p = (struct Thunk *)thunk_vp;
	int ThreadPID = syscall(SYS_gettid);
	printf ("New Thread Starting: PID = %d\n", ThreadPID);
        fflush(stdout);

	vtThreadStart(ThreadPID);

    void *result = (thunk_p->start_routine)(thunk_p->arg);

	free(thunk_p);
	printf ("Thread Exiting. PID = %d\n", ThreadPID);
         fflush(stdout);

	vtThreadFini(ThreadPID);
    return result;
}


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


    struct Thunk *thunk_p = malloc(sizeof(struct Thunk));
    thunk_p->start_routine = start_routine;
    thunk_p->arg = arg;
    printf("Calling pthread create\n");
    fflush(stdout);
    return real_pthread_create(thread, attr, wrap_threadFn, thunk_p);
}


/* LD_PRELOAD override that causes normal process termination to instead result
 * in abnormal process termination through a raised SIGABRT signal via abort(3)
 * (even if SIGABRT is ignored, or is caught by a handler that returns).
 */

void my_exit(void) {
   int ThreadPID = syscall(SYS_gettid);
   printf("I am exiting main thread. PID = %d\n", ThreadPID);
   fflush(stdout);

   vtAppFini(ThreadPID);
}

/* The address of the real main is stored here for fake_main to access */
static int (*real_main) (int, char **, char **);

/* Fake main(), spliced in before the real call to main() in __libc_start_main */
static int fake_main(int argc, char **argv, char **envp)
{	
	/* Register abort(3) as an atexit(3) handler to be called at normal
	 * process termination */
	atexit(my_exit);
	int ThreadPID = syscall(SYS_gettid);
    	printf ("Starting main program: Pid = %d\n", ThreadPID);
	initialize_vtl();
	*vtInitialized = 1;
	vtAfterForkInChild(ThreadPID);
	/* Finally call the real main function */
	return real_main(argc, argv, envp);
}

/* LD_PRELOAD override of __libc_start_main.
 *
 * The objective is to splice fake_main above to be executed instead of the
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
	load_orig_functions();

	/* Note that we swap fake_main in for main - fake_main should call
	 * real_main after its setup is done. */
	return real_libc_start_main.fn(fake_main, argc, ubp_av, init, fini,
				       rtld_fini, stack_end);
}


//void  __attribute__ ((noreturn)) pthread_exit(void *retval);

void load_original_pthread_exit() {
    original_pthread_exit = dlsym(RTLD_NEXT, "pthread_exit");
    if (!original_pthread_exit)
            abort();
}


void __attribute__ ((noreturn)) _exit(int status) {
	int ThreadPID = syscall(SYS_gettid);
	printf("I am alternate exiting main thread %d\n", ThreadPID);
	orig_exit(status);
}


void  __attribute__ ((noreturn))  pthread_exit(void *retval) {
    if (!original_pthread_exit) {
		load_original_pthread_exit();
    }

	int ThreadPID = syscall(SYS_gettid);
    printf ("Calling my pthread exit for PID = %d\n", ThreadPID);
    vtThreadFini(ThreadPID);
    original_pthread_exit(retval);
}


void load_orig_functions() {
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


pid_t fork() {
	int ParentPID = syscall(SYS_gettid);
	printf("Before fork in parent: %d\n", ParentPID);
        fflush(stdout);
	pid_t ret;
	
	ret = orig_fork();
	if (ret == 0) {
		int ChildPID = syscall(SYS_gettid);
		printf("After fork in child: PID = %d\n", ChildPID);
		vtAfterForkInChild(ChildPID);
	} else {
		printf("After fork in parent: PID = %d\n", ParentPID);
		vtForceCompleteBurst(ParentPID, 1, SYS_fork);
	}

	return ret;
}


int gettimeofday(struct timeval *tv, struct timezone *tz) {
	int ThreadPID =  syscall(SYS_gettid);
	//printf("In my gettimeofday: %d\n", ThreadPID);
	//fflush(stdout);
	vtGetCurrentTimeval(tv);
	return 0;
}


int clock_gettime(clockid_t clk_id, struct timespec *tp) {
	int ThreadPID =  syscall(SYS_gettid);
	//printf("In my clock_gettime: %d\n", ThreadPID);
        //fflush(stdout);
	vtGetCurrentTimespec(tp);
	return 0;
}


unsigned int sleep(unsigned int seconds) {
	int ThreadPID = syscall(SYS_gettid);
	//printf("In my sleep: %d\n", ThreadPID);
        //fflush(stdout);
	vtSleepForNS(ThreadPID, seconds*NSEC_PER_SEC);
	return 0;
}

int usleep(useconds_t usec) {
	int ThreadPID = syscall(SYS_gettid);
	//printf("In my usleep: %d\n", ThreadPID);
        //fflush(stdout);
	vtSleepForNS(ThreadPID, usec*NSEC_PER_US);
	//printf("Returning from my usleep: %d\n", ThreadPID);
        //fflush(stdout);
	return 0;
}


int poll(struct pollfd *fds, nfds_t nfds, int timeout_ms){
	
	int ret, ThreadPID;
	s64 start_time, elapsed_time, max_duration;
	ThreadPID = syscall(SYS_gettid);
	printf("In my poll: %d\n", ThreadPID);

	//return orig_poll(fds, nfds, timeout_ms);
	
	if (timeout_ms == 0)
		return orig_poll(fds, nfds, timeout_ms);

	if (timeout_ms < 0) {
		vtTriggerSyscallWait(ThreadPID, 1);
		ret = orig_poll(fds, nfds, timeout_ms);
		vtTriggerSyscallFinish(ThreadPID);
	} else {
		max_duration = timeout_ms * NSEC_PER_MS;
		start_time = vtGetCurrentTime();
		vtTriggerSyscallWait(ThreadPID, 1);
		do {
			ret = orig_poll(fds, nfds, 0);
			if (ret != 0)
				break;
			elapsed_time = vtGetCurrentTime() - start_time;

			if (elapsed_time < max_duration)
				vtTriggerSyscallWait(ThreadPID, 0);
		} while (elapsed_time < max_duration);
		vtTriggerSyscallFinish(ThreadPID);
	}
	return ret;
}

int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout) {

	int ret, ThreadPID, rem;
	int isReadInc, isWriteInc, isExceptInc;
	s64 start_time, elapsed_time, max_duration;
	fd_set readfds_copy, writefds_copy, exceptfds_copy;
	ThreadPID = syscall(SYS_gettid);

	printf("In my select: %d\n", ThreadPID);
	fflush(stdout);
	isReadInc = 0, isWriteInc = 0, isExceptInc = 0;

	//return orig_select(nfds, readfds, writefds, exceptfds, timeout);

	
	if (!timeout) {
		vtTriggerSyscallWait(ThreadPID, 1);
		ret = orig_select(nfds, readfds, writefds, exceptfds, timeout);
		vtTriggerSyscallFinish(ThreadPID);
	} else {
		if (timeout->tv_sec == 0 && timeout->tv_usec == 0)
			return orig_select(nfds, readfds, writefds, exceptfds, timeout);

		if (readfds) {
			readfds_copy = *readfds;
			isReadInc = 1;
		}

		if (writefds) {
			writefds_copy = *writefds;
			isWriteInc = 1;
		}

		if (exceptfds) {
			exceptfds_copy = *exceptfds;
			isExceptInc = 1;
		}
		max_duration 
			= timeout->tv_sec * NSEC_PER_SEC + timeout->tv_usec * NSEC_PER_US;
		start_time = vtGetCurrentTime();
		vtTriggerSyscallWait(ThreadPID, 1);
		timeout->tv_sec = 0, timeout->tv_usec = 0;
		do {
			ret = orig_select(nfds, readfds, writefds, exceptfds, timeout);
			if (ret != 0)
				break;
			elapsed_time = vtGetCurrentTime() - start_time;
			if (elapsed_time < max_duration)
				vtTriggerSyscallWait(ThreadPID, 0);
			
			if (isReadInc)
				*readfds = readfds_copy;
			if (isWriteInc)
				*writefds = writefds_copy;
			if (isExceptInc)
				*exceptfds = exceptfds_copy;
		} while (elapsed_time < max_duration);
		vtTriggerSyscallFinish(ThreadPID);
		
		if (elapsed_time < max_duration) {
			timeout->tv_sec = (max_duration - elapsed_time) / NSEC_PER_SEC;
			rem  = (max_duration - elapsed_time)
					- timeout->tv_sec * NSEC_PER_SEC;
			timeout->tv_usec = rem / NSEC_PER_US;
		}
	}

	return ret;
}

static void execute_cpu_yielding_syscall(long syscall_number, long arg0,
	long arg1, long arg2, long arg3, long arg4, long arg5, long *result,
	int ThreadPID) {

	if (vtInitialized && *vtInitialized == 1)
		vtYieldVTBurst(ThreadPID, 1, syscall_number);

	
	*result = syscall_no_intercept(syscall_number, arg0, arg1, arg2, arg3, arg4,
								   arg5);

	if (vtInitialized && *vtInitialized == 1)
		vtForceCompleteBurst(ThreadPID, 0, syscall_number);
}

static int hook(long syscall_number, long arg0, long arg1, long arg2, long arg3,
	long arg4, long arg5, long *result) {

	int ThreadPID = 0;
	

	if (syscall_number == SYS_sendto
		|| syscall_number == SYS_sendmsg
		|| syscall_number == SYS_sendmmsg) {
		ThreadPID = syscall_no_intercept(SYS_gettid);

		if (vtInitialized && *vtInitialized == 1)
			vtMarkCurrBBL(ThreadPID);
	}

	if (syscall_number == SYS_futex
		|| syscall_number == SYS_read
		|| syscall_number == SYS_wait4
		|| syscall_number == SYS_waitid
		|| syscall_number == SYS_recvfrom
		|| syscall_number == SYS_recvmmsg
		|| syscall_number == SYS_recvmsg) {

		ThreadPID = syscall_no_intercept(SYS_gettid);

		// Before cpu yielding syscall
		execute_cpu_yielding_syscall(syscall_number, arg0, arg1, arg2, arg3,
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
