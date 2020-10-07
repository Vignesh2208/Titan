

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

extern int interesting;

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

void sleep_with_select(int timeout_secs) {
    struct timeval tv;   
    tv.tv_sec = timeout_secs;
    tv.tv_usec = 0;
    if (select(0, NULL, NULL, NULL, &tv) < 0) perror("select");

};



int main(int argc, char *argv[]) {
    print_time();
    sleep_with_select(1);
    print_time();
        int i = 0;
        int j = 0;
    long nevents;
    while(1);
    return 0;
}
