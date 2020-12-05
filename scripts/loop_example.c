#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>        /* Definition of uint64_t */

#define handle_error(msg) \
       do { perror(msg); exit(EXIT_FAILURE); } while (0)

static void print_elapsed_time(i) {
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
   printf("%d.%03d: %d\n", secs, (nsecs + 500000) / 1000000, i);
}

struct temp_struct {
    int x;
    int y;
};

int main(int argc, char *argv[]) {
    int i;
    i = 0;
    struct temp_struct a[100];
    if (argc != 4) {
        fprintf(stderr, "%s init max step\n",
                argv[0]);
        exit(-1);
    }

    int init = atoi(argv[1]);
    int max = atoi(argv[2]);
    int step = atoi(argv[3]);
    int counter = 0;
    for(i = 0; i < 99; i++) {
        a[i].x = i;
        //printf("a[i].x = %c, &a[i].x = %p\n", a[i].x, (void *)&a[i].x);
        //printf("a[i+1].x = %c, &a[i+1].x = %p\n", a[i+1].x, (void *)&a[i+1].x);
    }
   
   
    print_elapsed_time(0);
    /*for(i = init; i >= max; i-=1) {
            if (counter < 100)
                continue;
            counter ++;
            fprintf(stderr, "Hello %d\n", counter);
    }
    
    for(i= init-1; i >= max; i-=2) {
        for(int j = init-2; j >= max; j-=3) {
            for (int k = 3; k < max; k++) {
                if (counter < 100)
                    continue;
                printf("Hello %d, %d, %d\n", i, j, k);
		print_elapsed_time(k);
		print_elapsed_time(k);
                
            }
            counter ++;
            printf("Hello %d, %d\n", i, j);
        }
        counter ++;
        print_elapsed_time(i);
        
    }*/

    for(i= init; i < max; i++) {	
        for(int j = 0; j < max; j+=2) {
            for (int k = 3; k < max; k++) {
                //if (counter < 100)
                //    continue;
                printf("Hello %d, %d, %d\n", i, j, k);
		print_elapsed_time(k);
		print_elapsed_time(k);
                
            }
            //counter ++;
            //printf("Hello %d, %d\n", i, j);
        }
        //counter ++;
        //print_elapsed_time(i);
        
    }


    print_elapsed_time(0);
    printf("Counter = %d\n", counter);
    return 0;
}
