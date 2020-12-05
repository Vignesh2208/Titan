#ifndef _INET_H
#define _INET_H

#include "syshead.h"
#include "socket.h"
#include "skbuff.h"

#ifdef DEBUG
#define DEBUG_SOCKET
#endif

#ifdef DEBUG_SOCKET
#define inet_dbg(sock, msg, ...)                                            \
    do {                                                                \
        socket_dbg(sock, "INET "msg, ##__VA_ARGS__);                    \
    } while (0)
#else
#define inet_dbg(msg, th, ...)
#endif

int InetCreate(struct vsocket *sock, int protocol);
int InetWrite(struct vsocket *sock, const void *buf, int len, int * did_block);
int InetRead(struct vsocket *sock, void *buf, int len, int * did_block);
int InetClose(struct vsocket *sock);
int InetFree(struct vsocket *sock);
int InetAbort(struct vsocket *sock);
int InetGetPeerName(struct vsocket *sock, struct sockaddr *restrict address,
                    socklen_t *restrict address_len);
int InetGetSockName(struct vsocket *sock, struct sockaddr *restrict address,
                    socklen_t *restrict address_len);

int InetListen(struct vsocket * sock, int backlog);
int InetBind(struct vsocket * sock, const struct sockaddr * sockaddr);
struct vsocket * InetAccept(struct vsocket *sock, int * err, struct sockaddr *skaddr);

struct vsock *InetLookup(struct sk_buff *skb, uint32_t saddr, uint32_t daddr,
                         uint16_t sport, uint16_t dport);
#endif
