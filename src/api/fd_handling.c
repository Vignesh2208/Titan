#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <sys/select.h>
#include <poll.h>
#include "VT_functions.h"
#include "utility_functions.h"
#include "vtl_logic.h"

extern hashmap thread_info_map;

void addFd(int ThreadID, int fd, int fdType, int isNonBlocking) {
    if (fd <= 0)
        return;

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    fdInfo * currFdInfo;
    assert(currThreadInfo != NULL);

    if (currThreadInfo->in_callback)
	    return;

    currFdInfo = (fdInfo *)malloc(sizeof(fdInfo));
    assert(currFdInfo != NULL);

    currFdInfo->fd = fd;
    currFdInfo->fdType = fdType;
    currFdInfo->isNonBlocking = isNonBlocking;
    currFdInfo->absExpiryTime = 0;
    currFdInfo->intervalNS = 0;
    currFdInfo->numExpiriesProcessed = 0;

    llist_append(&currThreadInfo->special_fds, currFdInfo);
}

int isFdTypeMatch(int ThreadID, int fd, int fdType) {
    if (fd <= 0)
        return FALSE;

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    if (currThreadInfo->in_callback)
	    return FALSE;

    llist_elem * head;
    fdInfo * currFdInfo;
	head = currThreadInfo->special_fds.head;
	while (head != NULL) {
        currFdInfo = (fdInfo *)head->item;

        if (currFdInfo && currFdInfo->fd == fd
            && currFdInfo->fdType == fdType) {
            return TRUE;
        }
		head = head->next;
	}

    return FALSE;
}


int isFdNonBlocking(int ThreadID, int fd) {
    if (fd <= 0)
        return FALSE;

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    if (currThreadInfo->in_callback)
	    return FALSE;

    llist_elem * head;
    fdInfo * currFdInfo;
	head = currThreadInfo->special_fds.head;
	while (head != NULL) {
        currFdInfo = (fdInfo *)head->item;

        if (currFdInfo && currFdInfo->fd == fd) {
            return currFdInfo->isNonBlocking;
        }
		head = head->next;
	}

    return FALSE;
}


void setFdBlockingMode(int ThreadID, int fd, int isNonBlocking) {
    if (fd <= 0)
        return;

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    if (currThreadInfo->in_callback)
	    return;

    llist_elem * head;
    fdInfo * currFdInfo;
	head = currThreadInfo->special_fds.head;
	while (head != NULL) {
        currFdInfo = (fdInfo *)head->item;

        if (currFdInfo && currFdInfo->fd == fd) {
            currFdInfo->isNonBlocking = isNonBlocking;
            return;
        }
		head = head->next;
	}
}


void setTimerFdParams(int ThreadID, int fd, s64 absExpiryTime, s64 intervalNS) {
    if (fd <= 0 || absExpiryTime < 0 || intervalNS < 0)
        return;

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    if (currThreadInfo->in_callback)
	    return;

    llist_elem * head;
    fdInfo * currFdInfo;
	head = currThreadInfo->special_fds.head;
	while (head != NULL) {
        currFdInfo = (fdInfo *)head->item;

        if (currFdInfo && currFdInfo->fd == fd
            && currFdInfo->fdType == FD_TYPE_TIMERFD) {

            if (absExpiryTime == 0)
                currFdInfo->absExpiryTime = GetCurrentTime();
            else
                currFdInfo->absExpiryTime = absExpiryTime;
            currFdInfo->intervalNS = intervalNS;
            currFdInfo->numExpiriesProcessed = 0;
            return;
        }
		head = head->next;
	}
}

int __getNumNewTimerFdExpires(fdInfo * currFdInfo, s64 * nxtExpiryDurationNS) {

    s64 currTime = GetCurrentTime();
    s64 diff, rem;
    int numNewExpiries;

    assert(currFdInfo != NULL && nxtExpiryDurationNS != NULL);

    if (currFdInfo->absExpiryTime > currTime) {
        currFdInfo->numExpiriesProcessed = 0;
        *nxtExpiryDurationNS = currFdInfo->absExpiryTime - currTime;
        return 0;
    } 

    if (currFdInfo->intervalNS == 0) {
        numNewExpiries = 1;
        *nxtExpiryDurationNS = 0;
    } else {
        diff = currTime - currFdInfo->absExpiryTime;
        numNewExpiries = (diff/currFdInfo->intervalNS) + 1;
        rem = diff - (numNewExpiries - 1)*currFdInfo->intervalNS;
    }

    numNewExpiries -= currFdInfo->numExpiriesProcessed; 
    currFdInfo->numExpiriesProcessed += numNewExpiries;

    if (numNewExpiries == 0 && currFdInfo->intervalNS) {
        *nxtExpiryDurationNS = currFdInfo->intervalNS - rem;
    } else {
        *nxtExpiryDurationNS = 0;
    }
    return numNewExpiries;

}

int getNumNewTimerFdExpiries(int ThreadID, int fd, s64 * nxtExpiryDurationNS) {
    assert(isFdTypeMatch(ThreadID, fd, FD_TYPE_TIMERFD) == TRUE);

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);
    assert(nxtExpiryDurationNS != NULL);

    assert(!currThreadInfo->in_callback);

    llist_elem * head;
    fdInfo * currFdInfo;
	head = currThreadInfo->special_fds.head;
    
	while (head != NULL) {
        currFdInfo = (fdInfo *)head->item;

        if (currFdInfo && currFdInfo->fd == fd
            && currFdInfo->fdType == FD_TYPE_TIMERFD) {
                return __getNumNewTimerFdExpires(currFdInfo,
                        nxtExpiryDurationNS);
            
        }
		head = head->next;
	}

    assert(FALSE);
    return 0;

}

int computeClosestTimerExpiryForSelect(int ThreadID, fd_set * readfs, int nfds,
                                      s64 * closestTimerExpiryDurationNS) {
    if (!nfds || !readfs || !closestTimerExpiryDurationNS)
        return FALSE;

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    assert(currThreadInfo != NULL);

    llist_elem * head;
    fdInfo * currFdInfo;
    s64 currTimerFDExpiryDurationNS;
    *closestTimerExpiryDurationNS = -1;
	head = currThreadInfo->special_fds.head;

    
	while (head != NULL) {
        currFdInfo = (fdInfo *)head->item;

        if (currFdInfo && currFdInfo->fdType == FD_TYPE_TIMERFD
            && currFdInfo->fd < nfds
            && FD_ISSET(currFdInfo->fd, readfs)) {
            if (__getNumNewTimerFdExpires(currFdInfo,
                                &currTimerFDExpiryDurationNS)) {
                *closestTimerExpiryDurationNS = 0;
                return TRUE;
            } else {
                if (*closestTimerExpiryDurationNS < 0) {
                    *closestTimerExpiryDurationNS = currTimerFDExpiryDurationNS;
                } else if (currTimerFDExpiryDurationNS 
                            < *closestTimerExpiryDurationNS) {
                    *closestTimerExpiryDurationNS = currTimerFDExpiryDurationNS;
                }
            }
        }
		head = head->next;
	}

    if (*closestTimerExpiryDurationNS)
        return TRUE;
    return FALSE;
}

int computeClosestTimerExpiryForPoll(
    int ThreadID, struct pollfd * fds, int nfds,
    s64 * closestTimerExpiryDurationNS) {
    
    fd_set equivalentFDSet;

    *closestTimerExpiryDurationNS = 0;

    int equivalentNfds = 0, atleast_one_read = FALSE;
    if (!nfds || !fds || !closestTimerExpiryDurationNS)
        return FALSE;

    for(int i = 0; i < nfds; i++) {
        if (fds[i].events & POLLIN) {
            atleast_one_read = TRUE;
            FD_SET(fds[i].fd, &equivalentFDSet);
            if (fds[i].fd >= equivalentNfds) {
                equivalentNfds = fds[i].fd + 1;
            }
        }
    }

    if (!atleast_one_read)
        return FALSE;

    return computeClosestTimerExpiryForSelect(ThreadID, &equivalentFDSet,
                            equivalentNfds, closestTimerExpiryDurationNS);
}

void closeFd(int ThreadID, int fd) {
    if (fd <= 0)
        return;

    ThreadInfo * currThreadInfo = hmap_get_abs(&thread_info_map, ThreadID);
    if (!currThreadInfo)
	    return;

    if (currThreadInfo->in_callback)
	    return;

    llist_elem * head;
    fdInfo * currFdInfo;
    int pos = 0;
	head = currThreadInfo->special_fds.head;
	while (head != NULL) {
        currFdInfo = (fdInfo *)head->item;

        if (currFdInfo && currFdInfo->fd == fd) {
            llist_remove_at(&currThreadInfo->special_fds, pos);
            return;
        }
		head = head->next;
        pos ++;
	}
}