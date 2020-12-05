#include "syshead.h"
#include "inet.h"
#include "socket.h"
#include "sock.h"
#include "tcp.h"
#include "wait.h"

extern struct net_ops tcp_ops;

static int inet_stream_connect(struct vsocket *sock, const struct sockaddr *addr,
                               int addr_len, int flags);

static int INET_OPS = 1;

struct net_family inet = {
    .create = InetCreate,
};

static struct vsock_ops inet_stream_ops = {
    .connect = &inet_stream_connect,
    .write = &InetWrite,
    .read = &InetRead,
    .close = &InetClose,
    .free = &InetFree,
    .abort = &InetAbort,
    .getpeername = &InetGetPeerName,
    .getsockname = &InetGetSockName,
    .accept = &InetAccept,
    .listen = &InetListen,
    .bind = &InetBind,
};

static struct vsock_type inet_ops[] = {
    {
        .sock_ops = &inet_stream_ops,
        .net_ops = &tcp_ops,
        .type = SOCK_STREAM,
        .protocol = IPPROTO_TCP,
    }
};

int InetCreate(struct vsocket *sock, int protocol) {
    struct vsock *sk;
    struct vsock_type *skt = NULL;

    for (int i = 0; i < INET_OPS; i++) {
        if (inet_ops[i].type & sock->type) {
            skt = &inet_ops[i];
            break;
        }
    }

    if (!skt) {
        print_err("Could not find socktype for socket\n");
        return 1;
    }

    sock->ops = skt->sock_ops;
    sk = SkAlloc(skt->net_ops, protocol);
    sk->protocol = protocol;
    SockInitData(sock, sk);
    return 0;
}

static int inet_stream_connect(struct vsocket *sock, const struct sockaddr *addr,
                        int addr_len, int flags) {
    struct vsock *sk = sock->sk;
    int rc = 0;
    
    if (addr_len < sizeof(addr->sa_family)) {
        return -EINVAL;
    }

    if (addr->sa_family == AF_UNSPEC) {
        sk->ops->disconnect(sk, flags);
        return -EAFNOSUPPORT;
    }

    switch (sock->state) {
    default:
        sk->err = -EINVAL;
        goto out;
    case SS_CONNECTED:
        sk->err = -EISCONN;
        goto out;
    case SS_CONNECTING:
        sk->err = -EALREADY;
        goto out;
    case SS_UNCONNECTED:
        sk->err = -EISCONN;
        if (sk->state != TCP_CLOSE) {
            goto out;
        }

        sk->ops->connect(sk, addr, addr_len, flags);
        sock->state = SS_CONNECTING;
        sk->err = -EINPROGRESS;

        if (sock->flags & O_NONBLOCK) {
            goto out;
        }

        
        do {
            SocketRelease(sock);
            RegisterSysCallWait();
            SocketWrAcquire(sock);

            if (sk->sock->sleep_wait_wakeup) {
                sk->sock->sleep_wait_wakeup = 0;
                break;
            }

        } while (1);
        
        
        switch (sk->err) {
        case -ETIMEDOUT:
        case -ECONNREFUSED:
            goto sock_error;
        }

        if (sk->err != 0) {
            goto out;
        }

        sock->state = SS_CONNECTED;
        break;
    }
    
out:
    return sk->err;
sock_error:
    rc = sk->err;
    return rc;
}

int InetWrite(struct vsocket *sock, const void *buf, int len, int * did_block) {
    struct vsock *sk = sock->sk;
    return sk->ops->write(sk, buf, len, did_block);
}

int InetRead(struct vsocket *sock, void *buf, int len, int * did_block) {
    struct vsock *sk = sock->sk;
    return sk->ops->read(sk, buf, len, did_block);
}


struct vsock *InetLookup(struct sk_buff *skb,
                          uint32_t saddr, uint32_t daddr,
                          uint16_t sport, uint16_t dport) {
    struct vsocket *sock = SocketLookup(saddr, daddr, sport, dport);
    if (sock == NULL) {
        printf ("No valid socket found !\n");
        return NULL;
    }
    return sock->sk;
}

int InetClose(struct vsocket *sock) {
    if (!sock) {
        return 0;
    }
    struct vsock *sk = sock->sk;
    return sock->sk->ops->close(sk);
}

int InetFree(struct vsocket *sock) {
    struct vsock *sk = sock->sk;
    SockFree(sk);
    free(sock->sk);
    
    return 0;
}

int InetAbort(struct vsocket *sock) {
    struct vsock *sk = sock->sk;
    
    if (sk) {
        sk->ops->abort(sk);
    }

    return 0;
}

int InetGetPeerName(struct vsocket *sock, struct sockaddr *restrict address,
                     socklen_t *address_len) {
    struct vsock *sk = sock->sk;

    if (sk == NULL) {
        return -1;
    }

    struct sockaddr_in *res = (struct sockaddr_in *) address;
    res->sin_family = AF_INET;
    res->sin_port = htons(sk->dport);
    res->sin_addr.s_addr = htonl(sk->daddr);
    *address_len = sizeof(struct sockaddr_in);

    inet_dbg(sock, "geetpeername sin_family %d sin_port %d sin_addr %d addrlen %d",
             res->sin_family, ntohs(res->sin_port), ntohl(res->sin_addr.s_addr), *address_len);
    
    return 0;
}
int InetGetSockName(struct vsocket *sock, struct sockaddr *restrict address,
                     socklen_t *address_len) {
    struct vsock *sk = sock->sk;

    if (sk == NULL) {
        return -1;
    }
    
    struct sockaddr_in *res = (struct sockaddr_in *) address;
    res->sin_family = AF_INET;
    res->sin_port = htons(sk->sport);
    res->sin_addr.s_addr = htonl(sk->saddr);
    *address_len = sizeof(struct sockaddr_in);

    inet_dbg(sock, "getsockname sin_family %d sin_port %d sin_addr %d addrlen %d",
             res->sin_family, ntohs(res->sin_port), ntohl(res->sin_addr.s_addr), *address_len);
    
    return 0;
}

struct vsocket * InetAccept(struct vsocket *sock, int * err, struct sockaddr *skaddr) {
	struct vsock *sk = sock->sk;
	struct vsock *newsk;
    struct vsocket * newvsock = NULL;
	*err = -1;
	if (!sk)
		goto out;
    print_debug ("Invoking tcp accept !\n");
	newsk = sk->ops->accept(sk);
    print_debug ("Finished invoking tcp accept !\n");
	if (newsk) {
		newvsock = newsk->sock;
		if (skaddr) {
            struct sockaddr_in *res = (struct sockaddr_in *) skaddr;
            res->sin_family = AF_INET;
            res->sin_port = htons(newsk->sport);
            res->sin_addr.s_addr = htonl(newsk->saddr);
		}
		err = 0;
	}
out:
	return newvsock;
}

int InetListen(struct vsocket *sock, int backlog) {
	struct vsock *sk = sock->sk;
	int err = -1;

	if (sock->type != SOCK_STREAM)
		return -1;
    print_debug ("Invoking TcpListen !\n");
	if (sk)
		err = sk->ops->listen(sk, backlog);
    print_debug ("Finished TcpListen !\n");
	return err;
}

int InetBind(struct vsocket *sock, const struct sockaddr *skaddr) {
	struct vsock *sk = sock->sk;
	int err = -1;
    struct sockaddr_in *res = (struct sockaddr_in *) skaddr;

	/* duplicate bind is error */
	if (sk->sport)
		goto err_out;

	/* bind address */
	sk->saddr = ntohl(res->sin_addr.s_addr);
    sk->sport = ntohs(res->sin_port);

    print_debug ("Setting bind src_port: %d\n", ntohs(res->sin_port));

	/* bind success */
	err = 0;
	/* clear connection */
	sk->daddr = 0;
	sk->dport = 0;
err_out:
	return err;
}
