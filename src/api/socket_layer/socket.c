#include "syshead.h"
#include "utils.h"
#include "socket.h"
#include "inet.h"
#include "wait.h"
#include "timer.h"
#include "tcp.h"
#include "../vtl_logic.h"

static int sock_amount = 0;
static int fd = 4097;
static LIST_HEAD(sockets);

// Lock to control allocation of file descriptors and appending allocated vsockets
// to a global list of sockets.
static pthread_rwlock_t slock = PTHREAD_RWLOCK_INITIALIZER;

extern struct net_family inet;

static struct net_family *families[128] = {
    [AF_INET] = &inet,
};

//! Allocates a vsocket and associates a file-descriptor with it. Socket
//  state is initially set to unconnected. Sets socket refcnt to 1
struct vsocket *AllockSocket() {
    // TODO: Figure out a way to not shadow kernel file descriptors.
    // Now, we'll just expect the fds for a process to never exceed this.
    
    struct vsocket *sock = malloc(sizeof (struct vsocket));
    ListInit(&sock->list);
    sock->refcnt = 0;
    sock->active = 1;
    sock->send_buf_size = 8192; // linux: default 8kb
    sock->rcv_buf_size = 87380; // linux default 
    //pthread_rwlock_wrlock(&slock);
    sock->fd = fd++;
    //pthread_rwlock_unlock(&slock);

    sock->state = SS_UNCONNECTED;
    sock->ops = NULL;
    sock->flags = O_RDWR;
    sock->sleep_wait_wakeup = 0;
    sock->sched_for_delete = 0;
    pthread_rwlock_init(&sock->lock, NULL);
    
    return sock;
}



//! Acquires read lock on vsocket
int SocketRdAcquire(struct vsocket *sock) {
    sock->refcnt++;
    return 0;
}

//! Acquires write lock on vsocket
int SocketWrAcquire(struct vsocket *sock) {
    sock->refcnt++;
    return 0;
}

//! Attempts to free a vsocket if nothing references it.
int SocketRelease(struct vsocket *sock) {
    int rc = 0;
    sock->refcnt--;
    
    if (sock->refcnt == 0 && !sock->active) {
        printf ("Freeing socket: %d\n", sock->fd);
        fflush(stdout);
        sock_amount--;
        sock->ops->free(sock);
        free(sock);
    }
    return rc;
}

//! Returns number of active sockets
int NumActiveSockets() {
    //pthread_rwlock_wrlock(&slock);
    return sock_amount;
    //pthread_rwlock_unlock(&slock);
}


s64 GetEarliestRtxSendTime() {
    struct list_head *item;
    struct vsocket *sock = NULL;
    struct vsock *sk = NULL;
    s64 earliest_rtx_send_time = 0;
    struct tcp_sock * tsk;

    //pthread_rwlock_rdlock(&slock);
    
    list_for_each(item, &sockets) {
        sock = list_entry(item, struct vsocket, list);

        if (sock == NULL || sock->sk == NULL || sock->sched_for_delete)
            continue;

        tsk = tcp_sk(sock->sk);
        if (tsk && tsk->retransmit != NULL && tsk->retransmit->expires > 0) {
            if (earliest_rtx_send_time == 0 ||
                tsk->retransmit->expires  < earliest_rtx_send_time)
                earliest_rtx_send_time = tsk->retransmit->expires;
        }   
    }
    //pthread_rwlock_unlock(&slock);

    if (earliest_rtx_send_time < TimerGetTick())
        return 0;
        
    return earliest_rtx_send_time;
}

//! Frees the vsocket and removes it from list of active vsockets.
int SocketFree(struct vsocket *sock) {
   
    
    //pthread_rwlock_wrlock(&slock);
    SocketWrAcquire(sock);
    ListDel(&sock->list);
    printf ("Socket: %d garbage collection\n", sock->fd);
    fflush(stdout);
    sock->active = 0;
    //pthread_rwlock_unlock(&slock);

    // triggers wake-up of any process which might still be waiting on
    // sleep condition variable (only processes in connect syscall may be)
    // waiting. vsocket state would have been set to SS_DISCONNECTING
    SocketRelease(sock);
    
    return 0;
}

//! Called after timer expiry to completely free a socket
void SocketGarbageCollect(struct vsocket * sock) {
    if (!SocketFind((struct vsocket *)sock))
        return;
    
    SocketFree(sock);
}

void GarbageCollectSockets() {
    struct list_head *item, *tmp;
    struct vsocket *sock;
    list_for_each_safe(item, tmp, &sockets) {
        sock = list_entry(item, struct vsocket, list);
        if (sock && sock->sched_for_delete) {
            SocketGarbageCollect(sock);
        }
    }
}

//! Sets socket state to disconnecting and starts a one-shot timer to garbage
//  collect the socket after some-time. This is done to give enough time to
//  any waiting processes to resume.
int ScheduleSocketDelete(struct vsocket *sock) {
    int rc = 0;
    if (sock->state == SS_DISCONNECTING) goto out;
    sock->state = SS_DISCONNECTING;
    sock->sched_for_delete = 1;
out:
    return rc;
}

//! This function is only called if the entire stack is to be free'ed. It is not
//  invoked during normal operation.
void AbortSockets() {
    struct list_head *item, *tmp;
    struct vsocket *sock;
    list_for_each_safe(item, tmp, &sockets) {
        sock = list_entry(item, struct vsocket, list);
        sock->ops->abort(sock);
    }
}

//! Returns a pointer to a vsocket with matching pid and file descriptor.
static struct vsocket *GetSocket(uint32_t fd) {
    struct list_head *item;
    struct vsocket *sock = NULL;

    //pthread_rwlock_rdlock(&slock);
    list_for_each(item, &sockets) {
        sock = list_entry(item, struct vsocket, list);
        if (sock->fd == fd) {
            if (sock->sched_for_delete)
                return NULL;
            goto out;
        }
    }
    
    sock = NULL;

out:
    //pthread_rwlock_unlock(&slock);
    return sock;
}

//! Returns a vsocket associated with a tcp socket in established state. A
//  tcp socket in established state would have src-ip, dst-ip, src-port and
//  dst-port all set to valid values.
struct vsocket *TcpLookupSockEstablish(
                unsigned int src, unsigned int dst,
				unsigned short src_port, unsigned short dst_port) {
    struct list_head *item;
    struct vsocket *sock = NULL;
    struct vsock *sk = NULL;

    //pthread_rwlock_rdlock(&slock);
    
    list_for_each(item, &sockets) {
        sock = list_entry(item, struct vsocket, list);

        if (sock == NULL || sock->sk == NULL) continue;
        sk = sock->sk;

        if (sk->saddr == dst && sk->daddr == src && sk->sport == dst_port
            && sk->dport == src_port) {
            goto found;
        }
    }

    sock = NULL;
    found:
    //pthread_rwlock_unlock(&slock);
    return sock;
}

//! Returns a vsocket associated with a tcp socket in listen state. A tcp socket
//  in listen state will only have src-ip and src-port set through a prior bind
//  syscall
static struct vsocket *TcpLookupSockListen(unsigned int addr, unsigned int nport)
{
	struct list_head *item;
    struct vsocket *sock = NULL;
    struct vsock *sk = NULL;

    //pthread_rwlock_rdlock(&slock);
    
    list_for_each(item, &sockets) {
        sock = list_entry(item, struct vsocket, list);

        if (sock == NULL || sock->sk == NULL) continue;
        sk = sock->sk;

        if (sk->state == TCP_LISTEN && sk->saddr == addr && sk->sport == nport) {
            goto found;
        }
    }

    sock = NULL;
    found:
    //pthread_rwlock_unlock(&slock);
    return sock;
}

//! Returns a matching vsocket which is supposed to process a packet. The arguments
//  are extracted from a received packet. The vsocket may be in established or
//  listen state. So we check both.
struct vsocket *SocketLookup(uint32_t saddr, uint32_t daddr,
                              uint16_t sport, uint16_t dport) {
    
    struct vsocket *sock = NULL;
    sock = TcpLookupSockEstablish(saddr, daddr, sport, dport);
    if (sock)
        return sock;
    sock = TcpLookupSockListen(daddr, dport);
    return sock;
}

//! Checks if the specified argument vsocket is present in the currently tracked
//  list of sockets.
struct vsocket *SocketFind(struct vsocket *find) {
    struct list_head *item;
    struct vsocket *sock = NULL;

    //pthread_rwlock_rdlock(&slock);
    list_for_each(item, &sockets) {
        sock = list_entry(item, struct vsocket, list);
        if (sock == find) goto out;
    }
    
    sock = NULL;

out:
    //pthread_rwlock_unlock(&slock);
    return sock;
}

#ifdef DEBUG_SOCKET
void SocketDebug() {
    struct list_head *item;
    struct vsocket *sock = NULL;

    //pthread_rwlock_rdlock(&slock);

    list_for_each(item, &sockets) {
        sock = list_entry(item, struct vsocket, list);
        SocketRdAcquire(sock);
        socket_dbg(sock, "");
        SocketRelease(sock);
    }

    //pthread_rwlock_unlock(&slock);
}
#else
void SocketDebug() {
    return;
}
#endif


void AddVSocketToList(struct vsocket * sock) {
    if (!sock)
        return;
    //pthread_rwlock_wrlock(&slock);
    ListAddTail(&sock->list, &sockets);
    sock_amount++;
    //pthread_rwlock_unlock(&slock);
}


//! Allocates a vsocket for a given pid and returns its file descriptor
int _socket(int domain, int type, int protocol) {
    struct vsocket *sock;
    struct net_family *family;

    if ((sock = AllockSocket()) == NULL) {
        print_err("Could not alloc socket\n");
        return -1;
    }
     printf ("Creating vt-tcp socket !\n");
     fflush(stdout);

    sock->type = type;
    family = families[domain];

    if (!family) {
        print_err("Domain not supported: %d\n", domain);
        goto abort_socket;
    }
    
    if (family->create(sock, protocol) != 0) {
        print_err("Creating domain failed\n");
        goto abort_socket;
    }

    //pthread_rwlock_wrlock(&slock);
    ListAddTail(&sock->list, &sockets);
    sock_amount++;
    //pthread_rwlock_unlock(&slock);

    SocketRdAcquire(sock);
    int rc = sock->fd;
    SocketRelease(sock);

    return rc;

abort_socket:
    SocketFree(sock);
    return -1;
}

//! Initiates a connect operation on the provided socket file-descriptor to a remote
//  host which is presumably listening on the specified addr.
int _connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen, int * did_block) {
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Connect: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }
    //printf ("Connecting through vt-tcp !\n");
    //fflush(stdout);
    SocketWrAcquire(sock);
    IncSocketRef(sock);

    int rc = sock->ops->connect(sock, addr, addrlen, 0, did_block);
    switch (rc) {
    case -EINVAL:
    case -EAFNOSUPPORT:
    case -ECONNREFUSED:
    case -ETIMEDOUT:
        DecSocketRef(sock);
        SocketRelease(sock);
        break;
    default:
        DecSocketRef(sock);
        SocketRelease(sock);
    }
    printf ("Connecting through vt-tcp success !\n");
    fflush(stdout);
    
    return rc;
}

//! Writes data present in the buffer to a write queue under the hood for the
//  given socket.
int _write(int sockfd, const void *buf, const unsigned int count, int * did_block) {
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Write: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }

    //printf ("send through vt-tcp !\n");
    //fflush(stdout);

    SocketWrAcquire(sock);
    IncSocketRef(sock);
    int rc = sock->ops->write(sock, buf, count, did_block);
    DecSocketRef(sock);
    SocketRelease(sock);

    return rc;
}


//! Reads from an internal receive queue of the socket and populates the specified
//  buffer.
int _read(int sockfd, void *buf, const unsigned int count, int * did_block) {
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Read: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }

    //printf ("recv through vt-tcp !\n");
    //fflush(stdout);

    SocketWrAcquire(sock);
    IncSocketRef(sock);
    int rc = sock->ops->read(sock, buf, count, did_block);
    DecSocketRef(sock);
    SocketRelease(sock);

    return rc;
}

//! If the socket is a tcp socket, indicates there is no more data to send to the
//  remote.
int _close(int sockfd) {
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Close: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }

    //printf ("close through vt-tcp !\n");
    //fflush(stdout);

    SocketWrAcquire(sock);
    IncSocketRef(sock);
    int rc = sock->ops->close(sock);
    DecSocketRef(sock);
    SocketRelease(sock);

    return rc;
}

//! Polling on a list of socket file descriptors. No timeout support
int _poll(struct pollfd fds[], nfds_t nfds, int timeout_ms) {
    
    int polled = 0;
    int timeout = 0;

    s64 start_time = TimerGetTick();
    if (timeout_ms == 0)
        timeout = 1;

    do {
        polled = 0;
        for (int i = 0; i < nfds; i++) {
            struct vsocket *sock;
            struct pollfd *poll = &fds[i];
            if ((sock = GetSocket(poll->fd)) == NULL) {
                print_err("Poll: could not find socket (fd %u)\n", poll->fd);
                poll->revents |= POLLNVAL;
                return -1;
            }
            SocketRdAcquire(sock);
            IncSocketRef(sock);
            poll->revents = 0;
            if ((poll->events & POLLIN) || (poll->events & POLLRDNORM)) {
                poll->revents = sock->sk->poll_events & (poll->events | POLLHUP | POLLERR | POLLNVAL);
            }

            if ((poll->events & POLLOUT) || (poll->events & POLLWRNORM)) {
                if (sock->rcv_buf_size - tcp_sk(sock->sk)->rcv_queue_size > 0) {
                    // there is some space left in write buffer
                    poll->revents |= ((POLLOUT | POLLWRNORM) & (poll->events));
                }
                if (sock->sk->poll_events & POLLOUT) {
                        poll->revents |= sock->sk->poll_events;
                }
            }
            DecSocketRef(sock);
            SocketRelease(sock);

            if (poll->revents > 0) {
                polled++;
            }
        }

        if (!polled && !timeout) {
            if (timeout_ms > 0) {
                if (TimerGetTick() - start_time >= timeout_ms * NSEC_PER_MS)
                    timeout = 1;
            } else if (timeout_ms < 0)
                timeout = 0;

            if (!timeout) {
                printf ("Poll registering syscall-wait !\n");
                fflush(stdout);
                RegisterSysCallWait();
            }
        }

    } while (polled == 0 && timeout == 0);
    
    if (timeout && !polled)
        return -1;

    return polled;
}

//! Control flags associated with a socket
int _fcntl(int sockfd, int cmd, ...) {
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Fcntl: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }

    SocketWrAcquire(sock);
    IncSocketRef(sock);
    va_list ap;
    int rc = 0;

    switch (cmd) {
    case F_GETFL:
        rc = sock->flags;
        goto out;
    case F_SETFL:
        va_start(ap, cmd);
        sock->flags = va_arg(ap, int);
        va_end(ap);
        rc = 0;
        goto out;
    default:
        rc = -1;
        goto out;
    }

    rc = -1;

out:
    DecSocketRef(sock);
    SocketRelease(sock);

    return rc;
}

//! Returns a previously set socket option value
int _getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Getsockopt: could not find socket (fd %u)\n", fd);
        return -EBADF;
    }

    if (!optval)
        return -EINVAL;

    if (level != SOL_SOCKET)
        return -EPROTONOSUPPORT;

    printf ("getsockopt through vt-tcp !\n");
    fflush(stdout);

    int rc = 0;

    SocketRdAcquire(sock);
    IncSocketRef(sock);
    
    switch (optname) {
        case SO_ERROR:
            *optlen = 4;
            *(int *)optval = sock->sk->err;
            rc = 0;
            break;
        case SO_SNDBUF:
            *(int *)optval = sock->send_buf_size;
            break;
        case SO_RCVBUF:
            *(int *)optval = sock->rcv_buf_size;
            break;
        default:
            print_err("Getsockopt unsupported optname %d\n", optname);
            rc =  -ENOPROTOOPT;
            break;
    }
        
    DecSocketRef(sock);
    SocketRelease(sock);

    return rc;
}

int _setsockopt(int sockfd, int level, int option_name,
                const void *option_value, socklen_t option_len) {
    struct vsocket *sock;
    if (level != SOL_SOCKET)
        return -EPROTONOSUPPORT;
    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Setsockopt: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }

    if (!option_value)
        return -EINVAL;

    printf ("setsockopt through vt-tcp !\n");
    fflush(stdout);

    int rc = 0;

    SocketRdAcquire(sock);
    IncSocketRef(sock);
    switch (option_name) {
        case SO_SNDBUF: sock->send_buf_size = *(int *)option_value;
            print_debug ("Sock: %d, SO_SNDBUF set to: %d\n", sockfd, sock->send_buf_size);
            break;
        case SO_RCVBUF: sock->rcv_buf_size = *(int *)option_value;
            print_debug ("Sock: %d, SO_RCVBUF set to: %d\n", sockfd, sock->rcv_buf_size);
            break;
        default:
            print_err("Setsockopt: Unsupported option %d\n", option_name);
            rc = -ENOPROTOOPT;
            break;
    }
    DecSocketRef(sock);
    SocketRelease(sock);

    return rc;
    

}

//! Returns the remote client's ip address and port number
int _getpeername(int sockfd, struct sockaddr *restrict address,
                 socklen_t *restrict address_len) {
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Getpeername: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }

    SocketRdAcquire(sock);
    IncSocketRef(sock);
    int rc = sock->ops->getpeername(sock, address, address_len);
    DecSocketRef(sock);
    SocketRelease(sock);


    return rc;
}

//! Returns this socket's bind ip address and port number
int _getsockname(int sockfd, struct sockaddr *restrict address,
                 socklen_t *restrict address_len){
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Getsockname: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }

    SocketRdAcquire(sock);
    IncSocketRef(sock);
    int rc = sock->ops->getsockname(sock, address, address_len);
    DecSocketRef(sock);
    SocketRelease(sock);

    return rc;
}

//! If the socket is of type TCP, then it sets its state to TCP_LISTEN
int _listen(int sockfd, int backlog) {
	int err = -1;
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Getsockname: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }

	if (!sock || backlog < 0)
		goto out;

    printf ("listen through vt-tcp !\n");
    fflush(stdout);
	SocketWrAcquire(sock);
    IncSocketRef(sock);
    print_debug ("Invoking InetListen !\n");
	if (sock->ops)
		err = sock->ops->listen(sock, backlog);
    print_debug ("Finished InetListen !\n");
    DecSocketRef(sock);
	SocketRelease(sock);

out:
	return err;
}

//! Associates the socket with a src-ip and source port
int _bind(int sockfd, const struct sockaddr *skaddr) {
	int err = -1;
    struct vsocket *sock;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Getsockname: could not find socket (fd %u)\n", sockfd);
        return -EBADF;
    }

	if (!sock || !skaddr)
		goto out;
	
    printf ("bind through vt-tcp !\n");
    fflush(stdout);
    SocketWrAcquire(sock);
    IncSocketRef(sock);
	if (sock->ops)
		err = sock->ops->bind(sock, skaddr);
    DecSocketRef(sock);
	SocketRelease(sock);

out:
	return err;
}

//! For a socket in TCP_LISTEN state, it blocks until a successfull connection
//  has been established from a remote client. It returns a new vsocket describing
//  the connection. If the socket is closed before this function returns, then
//  a null pointer is returned.
int _accept(int sockfd, struct sockaddr *skaddr, int * did_block) {
	struct vsocket *newsock = NULL;
    struct vsocket *sock;
    int err = 0;

    if ((sock = GetSocket(sockfd)) == NULL) {
        print_err("Getsockname: could not find socket (fd %u)\n", sockfd);
        return -1;
    }
	if (!sock)
		return -1;

    printf ("Accept through vt-tcp !\n");
    fflush(stdout);
	/* real accepting process */
    SocketWrAcquire(sock);
    IncSocketRef(sock);
    print_debug ("Invoking InetAccept !\n");
	if (sock->ops)
		newsock = sock->ops->accept(sock, &err, skaddr, did_block);
    DecSocketRef(sock);
	SocketRelease(sock);

    if (!newsock)
        return err;
    else
	    return newsock->fd;
}
