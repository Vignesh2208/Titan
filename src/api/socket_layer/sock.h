#ifndef _SOCK_H
#define _SOCK_H

#include "socket.h"
#include "wait.h"
#include "skbuff.h"

struct vsock;

// protocol specific socket ops
struct net_ops {
    struct vsock* (*alloc_sock) (int protocol);
    int (*init) (struct vsock *sk);
    int (*connect) (struct vsock *sk, const struct sockaddr *addr, int addr_len, int flags);
    int (*disconnect) (struct vsock *sk, int flags);
    int (*write) (struct vsock *sk, const void *buf, int len, int * did_block);
    int (*read) (struct vsock *sk, void *buf, int len, int * did_block);
    int (*recv_notify) (struct vsock *sk);
    int (*close) (struct vsock *sk);
    int (*abort) (struct vsock *sk);
    
	int (*set_port)(struct vsock *sk, unsigned short port);
	int (*listen)(struct vsock *sk, int backlog);
	struct vsock *(*accept)(struct vsock * sk, int * did_block);
};

struct vsock {
    struct vsocket *sock;
    struct net_ops *ops;
    void * proto_sock;
    
    int send_wait_wakeup;
    int recv_wait_wakeup;
    int accept_wait_wakeup;

    struct sk_buff_head receive_queue;
    struct sk_buff_head write_queue;
    int protocol;
    int state;
    int err;
    short int poll_events;
    uint16_t sport;
    uint16_t dport;
    uint32_t saddr;
    uint32_t daddr;
};

static inline struct sk_buff *WriteQueueHead(struct vsock *sk) {
    return SkbPeek(&sk->write_queue);
}


struct vsock *SkAlloc(struct net_ops *ops, int protocol);
void SockFree(struct vsock *sk);
void SockInitData(struct vsocket *sock, struct vsock *sk);
void SockConnected(struct vsock *sk);

#endif

