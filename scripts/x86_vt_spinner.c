

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

void print_time () {
 struct timeval tv;
 struct tm* ptm;
 char time_string[40];
 long milliseconds;

 /* Obtain the time of day, and convert it to a tm struct. */
 gettimeofday (&tv, NULL);
 ptm = localtime (&tv.tv_sec);
 /* Format the date and time, down to a single second. */
 strftime (time_string, sizeof (time_string), "%Y-%m-%d %H:%M:%S", ptm);
 /* Compute milliseconds from microseconds. */
 milliseconds = tv.tv_usec / 1000;
 /* Print the formatted time, in seconds, followed by a decimal point
   and the milliseconds. */
 printf ("%s.%03ld\n", time_string, milliseconds);

}



int main(int argc, char *argv[]) {
	//print_time();
	//usleep(10000);
	//print_time();
        int i = 0;
        int j = 0;
	long nevents;
        for (i = 0; i < 128; i++) {
		for (j = 0; j < 1024; j ++) {
			
		      printf("Invoking ck_pr_fas_64 for the %d time. CurrBurstLength:\n", i);
		      fflush(stdout);
		      long t = 1;
		      //t = ck_pr_fas_64(&h->interm_slots[s][i], 0);
		      printf("Finished Invoking ck_pr_fas_64 for the %d time. Value = %llu\n", i, t);
		      fflush(stdout);
		      //array[i] += t;
		      nevents += t;
		      printf("Finished Assigning: %llu for the %d time. CurrBurstLength\n", t, i);
		      fflush(stdout);
		}
        }
	while(1);
	return 0;
}
