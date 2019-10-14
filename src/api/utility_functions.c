#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <sys/syscall.h>
#include "utility_functions.h"

const char *FILENAME = "/proc/status";
/*
Sends a specific command to the Kronos Kernel Module.
To communicate with the TLKM, you send messages to the location
specified by FILENAME
*/
int send_to_kronos(char * cmd) {
    int fp = open(FILENAME, O_RDWR);
    int ret = 0;
    if (fp < 0) {
        printf("Error communicating with Kronos\n");
        return -1;
    }
    ret = write(fp, cmd, strlen(cmd));
    fflush(stdout);
    close(fp);
    return ret;
}

int get_stats(ioctl_args * args) {

    int fp = open(FILENAME, O_RDWR);
    int ret = 0;
    if (fp < 0) {
        printf("Error communicating with Kronos\n");
        return -1;
    }


    ret = ioctl(fp, TK_IO_GET_STATS, args);
    if (ret == -1) {
        perror("ioctl");
        close(fp);
        return -1;
    }
    close(fp);

    return 0;

}

/*
Returns the thread id of a process
*/
int gettid() {
    return syscall(SYS_gettid);
}

/*
Checks if it is being ran as root or not.
*/
int is_root() {
    if (geteuid() == 0)
        return 1;
    return 0;

}

/*
Returns 1 if module is loaded, 0 otherwise
*/
int isModuleLoaded() {
    if ( access( FILENAME, F_OK ) != -1 ) {
        return 1;
    } else {
        printf("Kronos kernel module is not loaded\n");
        return 0;
    }
}

void flush_buffer(char * buf, int size) {
    if (size)
        memset(buf, 0, size * sizeof(char));
}
