#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details. 
#include <time.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/select.h>
#include <syscall.h>

extern int global_insn_counter;

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
    printf("Test: gettimeofday: %s%ld\n",buffer,tv.tv_usec);
    printf("Hello World\n");

    printf("Test: Global insn counter value = %d\n", global_insn_counter);

    return 0;
}
