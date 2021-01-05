#include "syshead.h"
#include "utils.h"
#include "tcp.h"
#include "ip.h"
#include "skbuff.h"
#include "timer.h"

//static int num_sleeps = 0;

static void *TcpRetransmissionTimeout(void *arg);

static struct sk_buff *TcpAllockSkb(int optlen, int size) {
    int reserved = IP_HDR_LEN + TCP_HDR_LEN + optlen + size;
    struct sk_buff *skb = AllocSkb(reserved);
    SkbReserve(skb, reserved);
    skb->protocol = IP_TCP;
    skb->dlen = size;
    skb->seq = 0;
    skb->txmitted = 0;
    return skb;
}

static int TcpWriteSynOptions(struct tcphdr *th, struct tcp_options *opts, int optlen) {
    struct tcp_opt_mss *opt_mss = (struct tcp_opt_mss *) th->data;

    opt_mss->kind = TCP_OPT_MSS;
    opt_mss->len = TCP_OPTLEN_MSS;
    opt_mss->mss = htons(opts->mss);
    th->hl = TCP_DOFFSET + (optlen / 4);
    return 0;
}

static int TcpSynOptions(struct vsock *sk, struct tcp_options *opts) {
    struct tcp_sock *tsk = tcp_sk(sk);
    int optlen = 0;
    opts->mss = tsk->rmss;
    optlen += TCP_OPTLEN_MSS;
    return optlen;
}

// Reads sport, dport, saddr and daddr from socket book-keeping. In other words
// this function may only be used by socket which is aslready connected and only
// if the skb is intended for the destination to which the socket is connected to.
static int TcpTransmitSkb(struct vsock *sk, struct sk_buff *skb, uint32_t seq) {
    struct tcp_sock *tsk = tcp_sk(sk);
    struct tcb *tcb = &tsk->tcb;
    struct tcphdr *thdr = tcp_hdr(skb);

    /* No options were previously set */
    if (thdr->hl == 0) thdr->hl = TCP_DOFFSET;

    SkbPush(skb, thdr->hl * 4);

    thdr->sport = sk->sport;
    thdr->dport = sk->dport;

    if (!skb->seqset) {
        thdr->seq = seq;
    }

    thdr->ack_seq = tcb->rcv_nxt;
    thdr->win = min(tcb->rcv_wnd, sk->sock->rcv_buf_size - tsk->rcv_queue_size);
    thdr->rsvd = 0;
    thdr->csum = 0;
    thdr->urp = 0;


    tcp_out_dbg(thdr, sk, skb);
    tcp_print_hdr(thdr);
    tcpsock_dbg("\nTCP (skb) transmit: ", sk);

    // We are forced to do this ugly hack because iptable rules are somehow
    // blocking any actual rst packet from being sent out
    if (thdr->rst) {
        thdr->urg = 1;
        thdr->rst = 0;
    }

    skb->txmitted = 1;
    thdr->sport = htons(thdr->sport);
    thdr->dport = htons(thdr->dport);
    thdr->seq = htonl(thdr->seq);
    thdr->ack_seq = htonl(thdr->ack_seq);
    thdr->win = htons(thdr->win);
    thdr->csum = htons(thdr->csum);
    thdr->urp = htons(thdr->urp);
    thdr->csum = TcpV4Checksum(skb, htonl(sk->saddr), htonl(sk->daddr));
    
    return IpOutput(sk, skb);
}

static int TcpQueueTransmitSkb(struct vsock *sk, struct sk_buff *skb) {
    struct tcp_sock *tsk = tcp_sk(sk);
    struct tcb *tcb = &tsk->tcb;
    struct tcphdr * th = tcp_hdr(skb);
    int rc = 0;
    
    if (SkbQueueEmpty(&sk->write_queue)) {
        TcpRearmRtoTimer(tsk);
    }

    
    if (tsk->inflight == 0) {
        /* Store sequence information into the socket buffer */
        rc = TcpTransmitSkb(sk, skb, tcb->snd_nxt);
        tsk->inflight++;
        skb->seq = tcb->snd_nxt;
        tcb->snd_nxt += skb->dlen;
        skb->end_seq = tcb->snd_nxt;

        if (th->fin) tcb->snd_nxt++;
    }
    tsk->send_queue_size += skb->dlen;
    SkbQueueTail(&sk->write_queue, skb);
    return rc;
}


/* Routine for timer-invoked delayed acknowledgment */
void *TcpSendDelack(void *arg) {

    
    struct vsock *sk = (struct vsock *) arg;
    SocketWrAcquire(sk->sock);
    struct tcp_sock *tsk = tcp_sk(sk);
    tsk->delacks = 0;
    TcpReleaseDelackTimer(tsk);
    TcpSendAck(sk);
    printf ("tcp-send delack. ack_seq:  %u\n", tsk->tcb.rcv_nxt);
    SocketRelease(sk->sock);
    return NULL;
}

int TcpSendNext(struct vsock *sk) {
    struct tcp_sock *tsk = tcp_sk(sk);
    struct tcb *tcb = &tsk->tcb;
    struct tcphdr *th;
    struct sk_buff *next;
    struct list_head *item, *tmp;
    int i = 0;

    list_for_each_safe(item, tmp, &sk->write_queue.head) {
        
        next = list_entry(item, struct sk_buff, list);
        if (next == NULL) return -1;

        if (next->txmitted)
            continue;

        if (tcb->snd_nxt + next->dlen >= tcb->snd_una + tcb->snd_wnd)
            break;

        SkbResetHeader(next);
        TcpTransmitSkb(sk, next, tcb->snd_nxt);

        next->seq = tcb->snd_nxt;
        tcb->snd_nxt += next->dlen;
        next->end_seq = tcb->snd_nxt;

        i ++;

        th = tcp_hdr(next);
        if (th->fin) {
            printf ("Transmitting fin packet \n");
            fflush(stdout);
            tcb->snd_nxt++;
        }
    }
    
    return i;
}

//! Sends a syn with seq=iss, ack_seq=rcv.nxt (ack_seq will be 0 because rcv.nxt is 0)
//  snd.nxt will be set to iss + 1 after this call.
static int TcpSendSyn(struct vsock *sk) {
    if (sk->state != TCP_SYN_SENT && sk->state != TCP_CLOSE && sk->state != TCP_LISTEN) {
        print_err("Socket was not in correct state (closed or listen)\n");
        return 1;
    }

    struct sk_buff *skb;
    struct tcphdr *th;
    struct tcp_options opts = { 0 };
    int tcp_options_len = 0;

    tcp_options_len = TcpSynOptions(sk, &opts);
    skb = TcpAllockSkb(tcp_options_len, 0);
    th = tcp_hdr(skb);

    skb->seqset = 0;

    TcpWriteSynOptions(th, &opts, tcp_options_len);

    sk->state = TCP_SYN_SENT;
    th->syn = 1;


    return TcpQueueTransmitSkb(sk, skb);
}

// This is intended for transmitting to any destination regardless of whether
// the socket is connected to the destination or not.
static int TcpTransmitSkbTo(struct vsock *sk, struct sk_buff *src_skb,
                               struct sk_buff *dst_skb) {
    struct tcp_sock *tsk = tcp_sk(sk);
    struct tcb *tcb = &tsk->tcb;
    struct tcphdr *thdr = tcp_hdr(dst_skb);
    struct tcphdr *sthdr = tcp_hdr(src_skb);

    /* No options were previously set */
    if (thdr->hl == 0) thdr->hl = TCP_DOFFSET;

    assert(dst_skb->seqset == 1);

    SkbPush(dst_skb, thdr->hl * 4);

    thdr->sport = sthdr->dport;
    thdr->dport = sthdr->sport;
    thdr->win = min(tcb->rcv_wnd, sk->sock->rcv_buf_size - tsk->rcv_queue_size);
    thdr->csum = 0;
    thdr->urp = 0;
    thdr->rsvd = 0;

    tcp_out_dbg(thdr, sk, dst_skb);
    tcp_print_hdr(thdr);
    tcpsock_dbg("\nTCP (skb) transmit: ", sk);

    if (thdr->rst) {
        thdr->urg = 1;
        thdr->rst = 0;
    }

    dst_skb->txmitted = 1;

    thdr->sport = htons(thdr->sport);
    thdr->dport = htons(thdr->dport);
    thdr->seq = htonl(thdr->seq);
    thdr->ack_seq = htonl(thdr->ack_seq);
    thdr->win = htons(thdr->win);
    thdr->csum = htons(thdr->csum);
    thdr->urp = htons(thdr->urp);
    thdr->csum = TcpV4Checksum(dst_skb, htonl(IpHdr(src_skb)->daddr),
        htonl(IpHdr(src_skb)->saddr));
    
    return IpOutputDaddr(dst_skb, IpHdr(src_skb)->saddr);
}


int TcpSendResetTo(struct tcp_sock *tsk, struct sk_buff *src_skb) {
    struct tcphdr *sthdr = tcp_hdr(src_skb);
    struct sk_buff *skb;
    struct tcphdr *th;
    int rc = 0;

    if (sthdr->rst)
        return 0;

    skb = TcpAllockSkb(0, 0);
    th = tcp_hdr(skb);
    th->rst = 1;
    th->hl = TCP_DOFFSET;

    if (sthdr->ack) {
        th->seq = sthdr->ack_seq;
        th->ack = 0;
    } else {
        th->ack = 1;
        th->ack_seq = src_skb->seq + src_skb->len;
        th->seq = 0;
        
    }

    skb->seqset = 1;

    rc = TcpTransmitSkbTo(&tsk->sk, src_skb, skb);
    FreeSkb(skb);
    return rc;
}

// Send reset with seq=snd.nxt, ack_seq=rcv.nxt
int TcpSendReset(struct tcp_sock *tsk) {
    struct sk_buff *skb;
    struct tcphdr *th;
    struct tcb *tcb;
    int rc = 0;

    skb = TcpAllockSkb(0, 0);
    th = tcp_hdr(skb);
    tcb = &tsk->tcb;

    th->rst = 1;
    tcb->snd_una = tcb->snd_nxt;
    skb->seqset = 0;

    rc = TcpTransmitSkb(&tsk->sk, skb, tcb->snd_nxt);
    FreeSkb(skb);

    return rc;
}


// Send ack with seq=snd.nxt, ack_seq=rcv.nxt
int TcpSendAck(struct vsock *sk) {
    if (sk->state == TCP_CLOSE) return 0;
    
    struct sk_buff *skb;
    struct tcphdr *th;
    struct tcb *tcb = &tcp_sk(sk)->tcb;
    int rc = 0;

    skb = TcpAllockSkb(0, 0);
    th = tcp_hdr(skb);
    th->ack = 1;
    th->hl = TCP_DOFFSET;
    skb->seqset = 0;

    rc = TcpTransmitSkb(sk, skb, tcb->snd_nxt);
    FreeSkb(skb);
    return rc;
}

//! Send syn-ack with seq=iss, ack_seq=rcv.nxt. Read dport and sport from
//  input src_skbuff
int TcpSendSynackTo(struct vsock *sk, struct sk_buff *src_skb) {
	struct tcphdr *sthdr = tcp_hdr(src_skb);
    struct sk_buff *skb;
    struct tcphdr *th;
    struct tcp_sock * tcpsk = tcp_sk(sk);
    int rc = 0;

	if (sthdr->rst)
		return 0;
    skb = TcpAllockSkb(0, 0);
    th = tcp_hdr(skb);
    th->syn = 1;
    th->ack = 1;
    th->hl = TCP_DOFFSET;
    th->sport = sthdr->dport;
    th->dport = sthdr->sport;
    th->seq = tcpsk->tcb.iss;
    th->ack_seq = tcpsk->tcb.rcv_nxt;
    th->ack = 1;
    th->win = min(tcpsk->tcb.rcv_wnd, sk->sock->rcv_buf_size - tcpsk->rcv_queue_size);
    

    skb->seqset = 1;

    rc = TcpTransmitSkbTo(sk, src_skb, skb);
    FreeSkb(skb);
    return rc;
}

//! Send syn-ack with seq=iss, ack_seq=rcv.nxt
int TcpSendSynack(struct vsock *sk) {
    if (sk->state != TCP_SYN_SENT && sk->state != TCP_SYN_RECEIVED) {
        print_err("TCP synack: Socket was not in correct state (SYN_SENT or SYN_RECEIVED)\n");
        return 1;
    }

    struct sk_buff *skb;
    struct tcphdr *th;
    struct tcb * tcb = &tcp_sk(sk)->tcb;
    int rc = 0;

    skb = TcpAllockSkb(0, 0);
    th = tcp_hdr(skb);

    th->syn = 1;
    th->ack = 1;
    th->seq = tcb->iss;
    th->ack_seq = tcb->rcv_nxt;
    th->win = min(tcb->rcv_wnd, sk->sock->rcv_buf_size - tcp_sk(sk)->rcv_queue_size);

    skb->seqset = 1;

    rc = TcpTransmitSkb(sk, skb, tcb->snd_nxt);
    FreeSkb(skb);

    return rc;
}


void TcpSelectInitialWindow(uint32_t *rcv_wnd) {
    // default in linux (= 10 x mss)
    *rcv_wnd = 14600;
}

static void TcpNotifyUser(struct vsock *sk) {
    switch (sk->state) {
    case TCP_CLOSE_WAIT:
        sk->sock->sleep_wait_wakeup = 1;
        break;
    }
}

static void *TcpConnectRto(void *arg) {
    struct tcp_sock *tsk = (struct tcp_sock *) arg;
    struct tcb *tcb = &tsk->tcb;
    struct vsock *sk = &tsk->sk;

    printf ("tcp-connect rto\n");

    SocketWrAcquire(sk->sock);
    TcpReleaseRtoTimer(tsk);

    if (sk->state == TCP_SYN_SENT) {
        if (tsk->backoff > TCP_CONN_RETRIES) {
            sk->poll_events |= (POLLOUT | POLLERR | POLLHUP);
            TcpDone(sk, -ETIMEDOUT);
        } else {
            struct sk_buff *skb = WriteQueueHead(sk);

            if (skb) {
                SkbResetHeader(skb);
                TcpTransmitSkb(sk, skb, tcb->snd_una);
            
                tsk->backoff++;
                TcpRearmRtoTimer(tsk);
            }
         }
    } else {
        print_err("TCP connect RTO triggered even when not in SYNSENT\n");
    }

    SocketRelease(sk->sock);

    return NULL;
}

static void *TcpRetransmissionTimeout(void *arg) {

    

    struct tcp_sock *tsk = (struct tcp_sock *) arg;
    
    struct vsock *sk = &tsk->sk;

    SocketWrAcquire(sk->sock);

    TcpReleaseRtoTimer(tsk);

    struct sk_buff *skb = WriteQueueHead(sk);

    if (!skb) {
        tsk->backoff = 0;
        tcpsock_dbg("TCP RTO queue empty, notifying user", sk);
        TcpNotifyUser(sk);
        goto unlock;
    }

    struct tcphdr *th = tcp_hdr(skb);
    SkbResetHeader(skb);
    
    struct tcb *tcb = &tsk->tcb;
    TcpTransmitSkb(sk, skb, tcb->snd_una);

    
    /* RFC 6298: 2.5 Maximum value MAY be placed on RTO, provided it is at least
       60 seconds */
    if (tsk->rto > 60000) {
        sk->poll_events |= (POLLOUT | POLLERR | POLLHUP);
        TcpDone(sk, -ETIMEDOUT);
        SocketRelease(sk->sock);
        return NULL;
    } else {
        /* RFC 6298: Section 5.5 double RTO time */
        tsk->rto = tsk->rto * 2;
        tsk->backoff++;

        printf ("tcp-rto: snd_una: %u, rto: %u, backoff: %u, sock-fd = %d\n",
            tcb->snd_una, tsk->rto, tsk->backoff, sk->sock->fd);
        fflush(stdout);

        tsk->retransmit = TimerAdd(tsk->rto, &TcpRetransmissionTimeout, tsk);

        if (th->fin) {
            TcpHandleFinState(sk);
        }
    }

unlock:
    SocketRelease(sk->sock);

    return NULL;
}

void TcpRearmRtoTimer(struct tcp_sock *tsk) {
    struct vsock *sk = &tsk->sk;
    TcpReleaseRtoTimer(tsk);

    if (sk->state == TCP_SYN_SENT) {
        tsk->retransmit = TimerAdd(TCP_SYN_BACKOFF << tsk->backoff, &TcpConnectRto, tsk);
    } else {
        tsk->retransmit = TimerAdd(tsk->rto, &TcpRetransmissionTimeout, tsk);
    }
}

int TcpConnect(struct vsock *sk) {
    struct tcp_sock *tsk = tcp_sk(sk);
    struct tcb *tcb = &tsk->tcb;
    int rc = 0;
    
    tsk->tcp_header_len = sizeof(struct tcphdr);
    tcb->iss = GenerateISS();
    tcb->snd_una = tcb->iss;
    tcb->snd_nxt = tcb->iss;
    tcb->rcv_nxt = tcb->iss;
    tcb->snd_wnd = 0;
    tcb->snd_wl1 = 0;

    TcpSelectInitialWindow(&tsk->tcb.rcv_wnd);

    rc = TcpSendSyn(sk);
    tcb->snd_nxt++;
    
    return rc;
}

int TcpSend(struct tcp_sock *tsk, const void *buf, int len) {
    struct sk_buff *skb;
    struct tcphdr *th;    
    int slen;
    int mss = tsk->smss;
    int dlen = 0;
    struct vsock *sk = &tsk->sk;
    struct vsocket *sock = sk->sock;

    int curr_space_left;
    int queued_bytes = 0;

    tsk->blocked = 0;

    while (queued_bytes < len) {
        if (tsk->sk.sock->send_buf_size < tsk->send_queue_size)
            curr_space_left = 0;
        else
            curr_space_left = tsk->sk.sock->send_buf_size - tsk->send_queue_size;

        slen = min(len - queued_bytes, curr_space_left);
        while (slen > 0) {
            dlen = slen > mss ? mss : slen;
            slen -= dlen;

            skb = TcpAllockSkb(0, dlen);
            SkbPush(skb, dlen);
            memcpy(skb->data, buf, dlen);
            
            buf += dlen;
            queued_bytes += dlen;
            th = tcp_hdr(skb);
            th->ack = 1;

            if (queued_bytes == len) {
                th->psh = 1;
            }

            // we will make only send syscall set this.
            if (SkbQueueEmpty(&sk->write_queue)) {
                assert(tsk->send_queue_size == 0);
                tsk->inflight = 0;
            } else {

                if (tsk->inflight) {
                    tsk->chk_pt_send_queue_size += dlen;
                    if (!tsk->chk_pt_send_skb)
                        tsk->chk_pt_send_skb = skb;
                }
            }

            if (TcpQueueTransmitSkb(&tsk->sk, skb) == -1) {
                perror("Error on TCP skb queueing");
            }

        }


        if (queued_bytes < len && curr_space_left == 0) {
            
            if (sock->flags & O_NONBLOCK) {
                if (queued_bytes == 0) {
                    len = -EAGAIN;
                    break;
                } else {
                    len = queued_bytes;
                    break;
                }
            } else {
                // need to wait
                print_debug ("SEND: blocked because snd-buffer is full !\n");
                tsk->blocked = 1;
                do {
                    SocketRelease(sock);
                    RegisterSysCallWait();
                    SocketWrAcquire(sock);
                    if (sk->send_wait_wakeup) {
                        sk->send_wait_wakeup = 0;
                        break;
                    }
                } while (1);
                print_debug ("SEND: resumed because snd-buffer has some empty space!\n");
            }
        }

        if (tsk->sk.err < 0) {
            tcpsock_dbg("Breaking out of tcp-send due to socket error: ", (&tsk->sk));
            break;
        }
    }

    if (tsk->sk.err >= 0) {
        TcpRearmUserTimeout(&tsk->sk); 
        return len;
    } 
    return tsk->sk.err;
}


//! Enqueues a fin. When fin is transmitted seq=snd.nxt, ack_seq=rcv.nxt
//  snd.nxt is incremented after fin is sent.
int TcpQueueFin(struct vsock *sk) {
    struct sk_buff *skb;
    struct tcphdr *th;
    int rc = 0;

    skb = TcpAllockSkb(0, 0);
    th = tcp_hdr(skb);

    th->fin = 1;
    th->ack = 1;

    //printf ("Queuing fin \n");
    //fflush(stdout);

    tcpsock_dbg("Queueing fin", sk);
    
    rc = TcpQueueTransmitSkb(sk, skb);

    return rc;
}
