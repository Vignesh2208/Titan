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

int getNxtTimerFdNumber(int ThreadID);

void setTimerFdParams(int ThreadID, int fd, s64 absExpiryTime, s64 intervalNS,
                    s64 relExpiryDuration);
void getTimerFdParams(int ThreadID, int fd, s64* absExpiryTime, s64* intervalNS,
                    s64* relExpiryDuration);
int getNumNewTimerFdExpiries(int ThreadID, int fd, s64 * nxtExpiryDurationNS);
int computeClosestTimerExpiryForSelect(int ThreadID, fd_set * readfs, int nfds,
                                      s64 * closestTimerExpiryDurationNS);
int computeClosestTimerExpiryForPoll(int ThreadID, struct pollfd * fds,
                            int nfds, s64 * closestTimerExpiryDurationNS);

#endif