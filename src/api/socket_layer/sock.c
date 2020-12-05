#include "syshead.h"
#include "sock.h"
#include "socket.h"

//! Allocates a protocol specific vsock and returns it. This would eventually
//  be associated with a vsocket in the function call below.
struct vsock *SkAlloc(struct net_ops *ops, int protocol) {
    struct vsock *sk;
    sk = ops->alloc_sock(protocol);
    sk->ops = ops;
    return sk;
}

//! Initializes vsocket by coupling a protocol specific vsock to it.
//  Initializes receive and write queues of the protocol specific vsock
void SockInitData(struct vsocket *sock, struct vsock *sk) {
    sock->sk = sk;
    sk->sock = sock;
    sk->accept_wait_wakeup = 0;
    sk->recv_wait_wakeup = 0;
    sk->send_wait_wakeup = 0;
    SkbQueueInit(&sk->receive_queue);
    SkbQueueInit(&sk->write_queue);
    sk->poll_events = 0;
    if (sk->ops->init)
        sk->ops->init(sk);
}

//! Dequeues and frees every skb in receive and write queues
void SockFree(struct vsock *sk) {
    SkbQueueFree(&sk->receive_queue);
    SkbQueueFree(&sk->write_queue);
}

//! This function is called when the three-way handshake to establish a connection
//  is successfull at the client. It wakes up any process waiting on the connect
//  systcall and returns success.
void SockConnected(struct vsock *sk) {
    struct vsocket *sock = sk->sock;
    sock->state = SS_CONNECTED;
    sk->err = 0;
    sk->poll_events = (POLLOUT | POLLWRNORM | POLLWRBAND);
    sock->sleep_wait_wakeup = 1;
}
