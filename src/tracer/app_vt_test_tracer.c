#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <pthread.h> 
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* uintmax_t */
#include <string.h>
#include <sys/mman.h>
#include <unistd.h> /* sysconf */
#include <sys/types.h>
#include <sys/syscall.h>



#include "VT_functions.h"
#define APP_VT_TEST

#define NUM_THREADS 5
 
int total_number_of_tracers;
int my_tracer_id;

s64 * tracer_clock_array = NULL;
s64 * my_clock = NULL;

void thread_exit() {
        printf("Alternate exit for thread !\n");
	exit(0);
}

int fibonacci(int max, s64 * thread_target_clock, s64 * thread_increment) {
   


   int r0 = 0;
   int r1 = 1;
   int sum = 0, i = 0;

   for (i = 1; i < max; i++) {
	sum = sum + r1 + r0;
        r0 = r1;
        r1 = i + 1;

	#ifdef APP_VT_TEST
	if (*thread_increment <= 0 || *my_clock >= *thread_target_clock) {
	   *thread_increment = write_tracer_results(my_tracer_id, NULL, 0);
	   if (*thread_increment <= 0)
	       thread_exit();
	   *thread_target_clock = *my_clock + *thread_increment;
        } else {
		*thread_increment -= 100;
        }
        #endif
   }
   
   return sum;
}
// A normal C function that is executed as a thread  
// when its name is specified in pthread_create() 
void *myThreadFun(void *vargp) 
{ 
    int *myid = (int *)vargp;
    int fib_value;
    int x = (int)syscall(__NR_gettid);
    int ret;
    s64 thread_increment, thread_target_clock;
    printf("Thread: %d, PID = %d\n", *myid, x);
    #ifdef APP_VT_TEST
    ret = add_processes_to_tracer_sq(my_tracer_id, &x, 1);
    if (ret < 0) {
	printf("Thread: %d Add to TRACER SQ failed !\n", *myid);
        return NULL;
    }
    #endif

    #ifdef APP_VT_TEST
    thread_increment = write_tracer_results(my_tracer_id, NULL, 0);
    if (thread_increment <= 0)
	 thread_exit();
    thread_target_clock = *my_clock + thread_increment;
    #endif

    printf("Starting Fibonacci from Thread: %d\n", *myid); 
    fib_value = fibonacci(10000, &thread_target_clock, &thread_increment);
    printf("Finished Fibonacci from Thread: %d. Value = %d\n", *myid, fib_value); 
    sleep(1); 
    printf("Finished from Thread: %d\n", *myid); 

    #ifdef APP_VT_TEST
    thread_increment = write_tracer_results(my_tracer_id, &x, 1);
    assert(thread_increment <= 0);
    #endif
    return NULL; 
} 
   
int main(int argc, char **argv) 
{ 
    pthread_t thread_id[NUM_THREADS]; 
    int i;
    int id[NUM_THREADS];
    int fd;
    int num_pages, ret, page_size;

    if (argc < 3) {
        printf("Usage: %s <total_num_tracers> <my_tracer_id>\n", argv[0]);
        return EXIT_FAILURE;
    }
    page_size = sysconf(_SC_PAGE_SIZE);


    total_number_of_tracers = atoi(argv[1]);
    my_tracer_id = atoi(argv[2]);

    if (my_tracer_id <= 0 || my_tracer_id > total_number_of_tracers) {
	printf("Incorrect arguments to APP-VT tracer\n");
	exit(-1);
    }

    #ifdef APP_VT_TEST

    num_pages = (total_number_of_tracers * sizeof(s64))/page_size;
    num_pages ++;
    

    ret = register_tracer(my_tracer_id, TRACER_TYPE_APP_VT, SIMPLE_REGISTRATION, 0);
    if (ret < 0) {
	printf("TracerID: %d, Registration Error. Errno: %d\n", my_tracer_id, errno);
	exit(-1);
    } else {
	printf("TracerID: %d, Assigned CPU: %d\n", my_tracer_id, ret);
    }


    fd = open("/proc/dilation/status", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        assert(0);
    }
    printf("fd = %d, attempting to MMAP: %d pages\n", fd, num_pages);
    puts("mmaping /proc/dilation/status");
    tracer_clock_array = mmap(NULL, page_size*num_pages, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (tracer_clock_array == MAP_FAILED) {
        perror("mmap failed !");
        assert(0);
    }
    my_clock = &tracer_clock_array[my_tracer_id - 1];
    #endif


    for (i = 0; i < NUM_THREADS; i++) {
        id[i] = i;
        pthread_create(&thread_id[i], NULL, myThreadFun, (void *)&id[i]); 
    }

    for (i = 0; i < NUM_THREADS; i++) {
    	pthread_join(thread_id[i], NULL);
    } 
    close(fd);
    exit(0); 
}
