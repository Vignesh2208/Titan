#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>        /* Definition of uint64_t */

#define handle_error(msg) \
       do { perror(msg); exit(EXIT_FAILURE); } while (0)

static void
print_elapsed_time(i)
{
   static struct timespec start;
   struct timespec curr;
   static int first_call = 1;
   int secs, nsecs;

   if (first_call) {
       first_call = 0;
       if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
           handle_error("clock_gettime");
   }

   if (clock_gettime(CLOCK_MONOTONIC, &curr) == -1)
       handle_error("clock_gettime");

   secs = curr.tv_sec - start.tv_sec;
   nsecs = curr.tv_nsec - start.tv_nsec;
   if (nsecs < 0) {
       secs--;
       nsecs += 1000000000;
   }
   printf("%d.%03d: %d", secs, (nsecs + 500000) / 1000000, i);
}

int
main(int argc, char *argv[])
{
   int i;
   i = 0;
   if (argc != 4) {
       fprintf(stderr, "%s init max step\n",
               argv[0]);
       exit(-1);
   }

   int init = atoi(argv[1]);
   int max = atoi(argv[2]);
   int step = atoi(argv[3]);
   int counter = 0;

   print_elapsed_time(0);

   
   for(i= init; i < max; i++) {
	for(int j = 0; j < 1000; j++) {
		counter ++;
		printf("Hello %d\n", j);
	}
	counter ++;
	print_elapsed_time(i);
   };

 
   print_elapsed_time(0);
   printf("Counter = %d\n", counter);
   return 0;
}
