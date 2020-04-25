#ifndef __FD_HANDLING_H
#define __FD_HANDLING_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <sys/select.h>
#include <poll.h>
#include "utility_functions.h"


/*** General Fd Handling ***/
void addFd(int ThreadID, int fd, int fdType, int isNonBlocking);
int isFdTypeMatch(int ThreadID, int fd, int fdType);
int isFdNonBlocking(int ThreadID, int fd);
void setFdBlockingMode(int ThreadID, int fd, int isNonBlocking);
void closeFd(int ThreadID, int fd);

/*** For Socket Handling ***/
void addSocket(int ThreadID, int sockFD, int isNonBlocking);
int isSocketFd(int ThreadID, int sockFD);
int isSocketFdNonBlocking(int ThreadID, int sockFD);

/*** For TimerFd Handlng ***/
void  addTimerFd(int ThreadID, int fd, int isNonBlocking);
int isTimerFd(int ThreadID, int fd);
int isTimerFdNonBlocking(int ThreadID, int fd);
int isTimerArmed(int ThreadID, int fd);

void setTimerFdParams(int ThreadID, int fd, s64 absExpiryTime, s64 intervalNS);
int getNumNewTimerFdExpiries(int ThreadID, int fd, s64 * nxtExpiryDurationNS);
int computeClosestTimerExpiryForSelect(int ThreadID, fd_set * readfs, int nfds,
                                      s64 * closestTimerExpiryDurationNS);
int computeClosestTimerExpiryForPoll(int ThreadID, struct pollfd * fds,
                            int nfds, s64 * closestTimerExpiryDurationNS);

#endif