#ifndef _TCP_DATA_H
#define _TCP_DATA_H

#include "tcp.h"

int TcpDataDequeue(struct tcp_sock *tsk, void *user_buf, int len);
int TcpDataQueue(struct tcp_sock *tsk, struct tcphdr *th, struct sk_buff *skb);
int TcpDataClose(struct tcp_sock *tsk, struct tcphdr *th, struct sk_buff *skb);

#endif
