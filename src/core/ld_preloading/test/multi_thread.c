#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details. 
#include <pthread.h> 
#include <time.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/select.h>
#include <syscall.h>

extern int global_insn_counter;

pthread_mutex_t lock;



// A normal C function that is executed as a thread  
// when its name is specified in pthread_create() 
void *myThreadFun(void *vargp) 
{ 

    char buffer[30];
    struct timeval tv;

    time_t curtime;
    gettimeofday(&tv, NULL); 
    curtime=tv.tv_sec;

    strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
    printf("Thread gettimeofday: %s%ld\n",buffer,tv.tv_usec);


    sleep(1); 
    printf("Printing GeeksQuiz from Thread \n"); 
    printf("Acquiring lock !\n");
    pthread_mutex_lock(&lock);
    printf("Releasing lock !\n");
    pthread_mutex_unlock(&lock);

    return NULL; 
} 
   
int main() 
{ 
    pthread_t thread_id;
    pid_t ret; 
     clock_t start_t;

    char buffer[30];
    struct timeval tv;

    time_t curtime;
    gettimeofday(&tv, NULL); 
    curtime=tv.tv_sec;
    strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
    printf("gettimeofday: %s%ld\n",buffer,tv.tv_usec);


    struct timespec requestStart;
    clock_gettime(CLOCK_REALTIME, &requestStart);

    printf("clock_gettime: %lu.%lu\n",requestStart.tv_sec, requestStart.tv_nsec);

    start_t = clock();
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    printf("Select sleeping 1 sec\n");
    select(0, NULL, NULL, NULL, &tv);


    printf("Poll sleeping 5 sec\n");
    poll(NULL, 0, 5);


    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }


    printf("Acquiring lock !\n");
    pthread_mutex_lock(&lock);
    printf("Releasing lock !\n");
    pthread_mutex_unlock(&lock);

    printf("Acquiring lock2 !\n");
    pthread_mutex_lock(&lock);
    printf("Releasing lock2 !\n");
    pthread_mutex_unlock(&lock);


    printf("Before Thread\n"); 
    global_insn_counter = 10;
    pthread_create(&thread_id, NULL, myThreadFun, NULL); 
    pthread_join(thread_id, NULL); 
    printf("After Thread\n");
    

    
    ret = fork();
    if (ret == 0) {

	char *const args[] = {"./test",NULL};
    	char *const envs[] = {"LD_PRELOAD=./intercept_test.so","LD_LIBRARY_PATH=/home/vignesh/Desktop/Lookahead_Testing/ld_preloading", NULL};
    	
	printf("Executing child \n");
	syscall(SYS_getpid);
    	syscall(SYS_getpid);
    	printf("Child: MY PID = %d. Starting execve\n", syscall(SYS_getpid));

        
        sleep(1);
        printf("Child finishing !\n");
        execve("./test", args, envs);
        exit(0);
    } if (ret > 0) {
    	printf("Resuming parent after fork\n");
	sleep(2);
    }

  
    //system("/home/vignesh/Desktop/Lookahead_Testing/ld_preloading/test");
    printf("Parent finishing !\n");
    printf("Global insn counter value = %d\n", global_insn_counter);

    pthread_mutex_destroy(&lock);
    exit(0); 
}
