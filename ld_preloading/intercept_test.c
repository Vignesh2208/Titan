

#define _GNU_SOURCE

#include <libsyscall_intercept_hook_point.h>
#include <syscall.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>
#include <sys/select.h>
#include <linux/futex.h>


int insn_counter;
int * global_insn_ctr = NULL;

typedef int (*pthread_create_t)(pthread_t *, const pthread_attr_t *,
                                void *(*)(void*), void *);
void (*original_pthread_exit)(void * ret_val) = NULL;

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



void (*orig__exit)(int status);


int futex_pid = 0, futex_pid_after = 0, futex_count;
int getpid_count;

struct Thunk
{
    void *(*start_routine)(void *);
    void *arg;
};

static void * wrap_threadFn(void *thunk_vp)
{
    
    printf ("Start of thread: PID = %lu\n", syscall(SYS_gettid));
    struct Thunk *thunk_p = (struct Thunk *)thunk_vp;
    void *result = (thunk_p->start_routine)(thunk_p->arg);
    free(thunk_p);
    printf ("Thread Exiting. PID = %lu. futex_pid = %d, futex_after_pid = %d, futex_count = %d, getpid_count = %d\n", syscall(SYS_gettid), futex_pid, futex_pid_after, futex_count, getpid_count);
    return result;
}

int
pthread_create(pthread_t *thread ,const pthread_attr_t *attr,
               void *(*start_routine)(void*), void *arg)
{
    static pthread_create_t real_pthread_create;
    

    if (!real_pthread_create)
    {
        real_pthread_create
            = (pthread_create_t)dlsym(RTLD_NEXT, "pthread_create");
        if (!real_pthread_create)
            abort(); // "impossible"
    }

    
    if (global_insn_ctr) {
	    printf("My pthread_create called. Global insn_counter = %d. futex_pid = %d, futex_after_pid = %d, futex_count = %d\n", *global_insn_ctr, futex_pid, futex_pid_after, futex_count);
	    (*global_insn_ctr)++;
    }
    struct Thunk *thunk_p = malloc(sizeof(struct Thunk));
    thunk_p->start_routine = start_routine;
    thunk_p->arg = arg;
    return real_pthread_create(thread, attr, wrap_threadFn, thunk_p);
}


/* LD_PRELOAD override that causes normal process termination to instead result
 * in abnormal process termination through a raised SIGABRT signal via abort(3)
 * (even if SIGABRT is ignored, or is caught by a handler that returns).
 * 
 * Loosely based on libminijailpreload.c by Chromium OS authors: 
 * https://android.googlesource.com/platform/external/minijail/+/master/libminijailpreload.c
 */

void my_exit(void) {

   printf("I am exiting main thread. PID = %d. Getpid count = %d\n",syscall(SYS_gettid), getpid_count);
   /*if (original_pthread_exit == NULL) {
        load_original_pthread_exit();
    }
    printf("I am exiting thread from my pthread_exit\n");
    original_pthread_exit(retval);*/
   //exit(0);
}

/* The address of the real main is stored here for fake_main to access */
static int (*real_main) (int, char **, char **);

/* Fake main(), spliced in before the real call to main() in __libc_start_main */
static int fake_main(int argc, char **argv, char **envp)
{	
	/* Register abort(3) as an atexit(3) handler to be called at normal
	 * process termination */
	atexit(my_exit);
        printf ("Starting main program: Pid = %d. Getpidcount = %d\n", syscall(SYS_gettid), getpid_count);

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


        orig__exit = dlsym(libc_handle, "_exit");
	if (!orig__exit) {
		fprintf(stderr, "can't find _exit():%s\n",
			dlerror());
		_exit(EXIT_FAILURE);
	}


        if (!global_insn_ctr) {
		lib_vt_clock_handle = dlopen("libvt_clock.so", RTLD_NOLOAD | RTLD_NOW);

		if (lib_vt_clock_handle) {
			global_insn_ctr = dlsym(lib_vt_clock_handle, "global_insn_counter");
			if (!global_insn_ctr) {
			    printf("GLOBAL insn counter not found !\n");
			    abort();

			}
		}

		if(lib_vt_clock_handle && dlclose(lib_vt_clock_handle)) {
			fprintf(stderr, "can't close handle to libvt_clock.so: %s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

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
	 printf("I am alternate exiting main thread. PID = %d. Getpid count = %d\n",syscall(SYS_gettid), getpid_count);
	orig__exit(status);
}

void  __attribute__ ((noreturn))  pthread_exit(void *retval) {
    if (!original_pthread_exit) {
	load_original_pthread_exit();
    }
    printf ("Calling my pthread exit for PID = %d\n", syscall(SYS_gettid)); 
    original_pthread_exit(retval);
}


pid_t fork() {
	printf("Before fork in parent: %d\n", syscall(SYS_gettid));
	pid_t ret;
        

	if (!orig_fork) {
		void *libc_handle;
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
		orig_fork = dlsym(libc_handle, "fork");
		if (!orig_fork) {
			fprintf(stderr, "can't find fork():%s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		
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

		
	}

	

	ret = orig_fork();
	if (ret == 0) {

		if (global_insn_ctr) {
                	(*global_insn_ctr)++;

			printf("After fork in child: PID = %d. Global insn counter = %d\n", syscall(SYS_gettid), *global_insn_ctr);
	                (*global_insn_ctr)++;
		} else {
			printf("After fork in child: PID = %d\n", syscall(SYS_gettid));
		}
	} else {
		printf("After fork in parent: PID = %d\n", syscall(SYS_gettid));
	}

	return ret;
}




int gettimeofday(struct timeval *tv, struct timezone *tz) {
	printf("In my gettimeofday: %d\n", syscall(SYS_gettid));
	
	if (!orig_gettimeofday) {
		void *libc_handle;
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
		orig_gettimeofday = dlsym(libc_handle, "gettimeofday");
		if (!orig_gettimeofday) {
			fprintf(stderr, "can't find gettimeofday():%s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		
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

		
	}

	return orig_gettimeofday(tv, tz);
}


int clock_gettime(clockid_t clk_id, struct timespec *tp) {
	//printf("In my clock_gettime: %d\n", syscall(SYS_gettid));
	
	if (!orig_clock_gettime) {
		void *libc_handle;
		libc_handle = dlopen("libc.so.6", RTLD_NOLOAD | RTLD_NOW);

		if (!libc_handle) {
			fprintf(stderr, "can't open handle to libc.so.6: %s\n",
				dlerror());
			/* We dare not use abort() here because it would run atexit(3)
			 * handlers and try to flush stdio. */
			_exit(EXIT_FAILURE);
		}
		orig_clock_gettime = dlsym(libc_handle, "clock_gettime");
		if (!orig_clock_gettime) {
			fprintf(stderr, "can't find orig_clock_gettime():%s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		if(dlclose(libc_handle)) {
			fprintf(stderr, "can't close handle to libc.so.6: %s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		
	}

	return orig_clock_gettime(clk_id, tp);
}


unsigned int sleep(unsigned int seconds) {
	printf("In my sleep: %d. Getpid count = %d\n", syscall(SYS_gettid), getpid_count);
	
	if (!orig_sleep) {
		void *libc_handle;
		libc_handle = dlopen("libc.so.6", RTLD_NOLOAD | RTLD_NOW);

		if (!libc_handle) {
			fprintf(stderr, "can't open handle to libc.so.6: %s\n",
				dlerror());
			/* We dare not use abort() here because it would run atexit(3)
			 * handlers and try to flush stdio. */
			_exit(EXIT_FAILURE);
		}
		orig_sleep = dlsym(libc_handle, "sleep");
		if (!orig_sleep) {
			fprintf(stderr, "can't find orig_sleep():%s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		if(dlclose(libc_handle)) {
			fprintf(stderr, "can't close handle to libc.so.6: %s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		
	}

	return orig_sleep(seconds);
}


int poll(struct pollfd *fds, nfds_t nfds, int timeout){
	printf("In my poll: %d\n", syscall(SYS_gettid));
	
	if (!orig_poll) {
		void *libc_handle;
		libc_handle = dlopen("libc.so.6", RTLD_NOLOAD | RTLD_NOW);

		if (!libc_handle) {
			fprintf(stderr, "can't open handle to libc.so.6: %s\n",
				dlerror());
			/* We dare not use abort() here because it would run atexit(3)
			 * handlers and try to flush stdio. */
			_exit(EXIT_FAILURE);
		}
		orig_poll = dlsym(libc_handle, "poll");
		if (!orig_poll) {
			fprintf(stderr, "can't find orig_poll():%s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		if(dlclose(libc_handle)) {
			fprintf(stderr, "can't close handle to libc.so.6: %s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		
	}

	return orig_poll(fds, nfds, timeout);
}

int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout) {
	printf("In my select: %d\n", syscall(SYS_gettid));
	
	if (!orig_select) {
		void *libc_handle;
		libc_handle = dlopen("libc.so.6", RTLD_NOLOAD | RTLD_NOW);

		if (!libc_handle) {
			fprintf(stderr, "can't open handle to libc.so.6: %s\n",
				dlerror());
			/* We dare not use abort() here because it would run atexit(3)
			 * handlers and try to flush stdio. */
			_exit(EXIT_FAILURE);
		}
		orig_select = dlsym(libc_handle, "select");
		if (!orig_select) {
			fprintf(stderr, "can't find orig_select():%s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		if(dlclose(libc_handle)) {
			fprintf(stderr, "can't close handle to libc.so.6: %s\n",
				dlerror());
			_exit(EXIT_FAILURE);
		}

		
	}

	return orig_select(nfds, readfds, writefds, exceptfds, timeout);
}

/*
int futex(int *uaddr, int futex_op, int val,
          const struct timespec *timeout,
          int *uaddr2, int val3) {
	printf("In my futex: %d\n", syscall(SYS_gettid));
	
	return syscall(SYS_futex, uaddr, futex_op, val,
                          timeout, uaddr, val3);
}*/


static int
hook(long syscall_number,
			long arg0, long arg1,
			long arg2, long arg3,
			long arg4, long arg5,
			long *result)
{

	int pid = 0;
        //char  output_buf[100];
        //memset(output_buf, 0, 100);
	if (syscall_number == SYS_getdents) {
		/*
		 * Prevent the application from
		 * using the getdents syscall. From
		 * the point of view of the calling
		 * process, it is as if the kernel
		 * would return the ENOTSUP error
		 * code from the syscall.
		 */
		*result = -ENOTSUP;
		return 0;
	} if (syscall_number == SYS_futex) {
		futex_pid = syscall_no_intercept(SYS_gettid);
		*result = syscall_no_intercept(SYS_futex, arg0, arg1, arg2, arg3, arg4, arg5);
                futex_pid_after = syscall_no_intercept(SYS_gettid);
		futex_count ++;
		return 0;
                
	} if (syscall_number == SYS_getpid) {
		getpid_count ++;
		return 1;
	}
	else {
		/*
		 * Ignore any other syscalls
		 * i.e.: pass them on to the kernel
		 * as would normally happen.
		 */
		return 1;
	}
}

static __attribute__((constructor)) void
init(void)
{
	// Set up the callback function
	intercept_hook_point = hook;
}
