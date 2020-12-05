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
//! Adds a file descriptor opened by a thread into an internal-datastore
void AddFd(int ThreadID, int fd, int fdType, int fdProtoType, int isNonBlocking);

//! Returns a positive number if the a file descriptor matches the specified type
int IsFdTypeMatch(int ThreadID, int fd, int fdType);

//! Returns a positive number if the file descriptor is non-blocking
int IsFdNonBlocking(int ThreadID, int fd);


//! Returns a positive number if the file descriptor is a tcp-socket
int IsTCPSockFd(int ThreadID, int fd);

//! Sets the blocking mode of the file descriptor
void SetFdBlockingMode(int ThreadID, int fd, int isNonBlocking);

//! Removes the file descriptor from internal bookkeeping datastore
void CloseFd(int ThreadID, int fd);

//! Returns an available filedescriptor which can be assigned to a new timerfd
int GetNxtTimerFdNumber(int ThreadID);

//! Sets the expiry parameters for a timerfd file descriptor
void SetTimerFdParams(int ThreadID, int fd, s64 absExpiryTime, s64 intervalNS,
                      s64 relExpiryDuration);

//! Gets the expiry parameters for a timerfd file descriptor
void GetTimerFdParams(int ThreadID, int fd, s64* absExpiryTime, s64* intervalNS,
                      s64* relExpiryDuration);

// Returns the number of timerfds which have expired by this time
int GetNumNewTimerFdExpiries(int ThreadID, int fd, s64 * nxtExpiryDurationNS);

//! Compute earliest time at which a select containing timerfds should return
int ComputeClosestTimerExpiryForSelect(int ThreadID, fd_set * readfs, int nfds,
                                       s64 * closestTimerExpiryDurationNS);

//! Compute earliest time at which a poll containing timerfds should return
int ComputeClosestTimerExpiryForPoll(int ThreadID, struct pollfd * fds, int nfds,
                                     s64 * closestTimerExpiryDurationNS);

#endif