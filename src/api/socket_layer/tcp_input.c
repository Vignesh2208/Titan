#include "syshead.h"
#include "tcp.h"
#include "tcp_data.h"
#include "skbuff.h"
#include "sock.h"

extern struct net_ops tcp_ops;

static int TcpParseOpts(struct tcp_sock *tsk, struct tcphdr *th) {
    uint8_t *ptr = th->data;
    uint8_t optlen = tcp_hlen(th) - 20;
    struct tcp_opt_mss *opt_mss = NULL;
    
    
    while (optlen > 0 && optlen < 20) {
        switch (*ptr) {
        case TCP_OPT_MSS:
            opt_mss = (struct tcp_opt_mss *)ptr;
            uint16_t mss = ntohs(opt_mss->mss);

            if (mss > 536 && mss <= 1460) {
                tsk->smss = mss;
            }

            ptr += sizeof(struct tcp_opt_mss);
            optlen -= 4;
            break;
        case TCP_OPT_NOOP:
            ptr += 1;
            optlen--;
            break;
        default:
            print_err("Unrecognized TCPOPT\n");
            optlen--;
            break;
        }
    }
    return 0;
}

uint8_t * AllockNewDataBlock(int size) {

    if (size)
        return (uint8_t *)malloc(size);
    return NULL;
}

int MergeSkbs(struct vsock *sk, struct sk_buff * prev_skb, struct sk_buff * skb) {
    uint8_t * data_block;

    if (!skb || !prev_skb)
        return -1;

    int hdr_space = (prev_skb->end - prev_skb->head) - prev_skb->dlen;

    data_block = AllockNewDataBlock(hdr_space + skb->dlen + prev_skb->dlen);
    if (!data_block)
        return -1;
    
    assert(hdr_space >= IP_HDR_LEN + TCP_HDR_LEN);
    
    memcpy(data_block, prev_skb->head, hdr_space + prev_skb->dlen);
    free(prev_skb->head);
    prev_skb->head = data_block;
    prev_skb->end = data_block + prev_skb->dlen + skb->dlen + hdr_space;

    SkbResetHeader(skb);
    memcpy(prev_skb->head + hdr_space + prev_skb->dlen, skb->data, skb->dlen);
    prev_skb->dlen += skb->dlen;

    //delete skb from write queue because it has been merged with prev_skb
    ListDel(&skb->list);
    sk->write_queue.qlen -= 1;
    FreeSkb(skb);

    SkbReserve(prev_skb, hdr_space + prev_skb->dlen);
    prev_skb->protocol = IP_TCP;
    prev_skb->seq = 0;
    prev_skb->txmitted = 0;

    return 0;
}

/*
 * Acks all segments from retransmissionn queue that are "older"
 * than current unacknowledged sequence
 */ 
static int TcpCleanRtoQueue(struct vsock *sk, uint32_t una) {
    struct tcp_sock *tsk = tcp_sk(sk);
    //struct vsocket * sock = sk->sock;
    struct sk_buff *skb;
    struct sk_buff *skb_first;
    int rc = 0;
    int first_seq;
    struct list_head *item, *tmp = NULL;
    struct sk_buff * chk_pt_resume_skb;
    struct sk_buff * prev_skb = NULL;
    struct tcphdr *th;
    struct tcphdr *tmpth; 
    
    //int prev_full;
    //int handle_wrap_around = 0;
    //struct list_head *item, *tmp;

    skb_first = SkbPeek(&sk->write_queue);

    if (!skb_first || !skb_first->txmitted) {

        if (skb_first && !skb_first->txmitted) {
            assert(tsk->inflight == 0);
        }

        return rc;
    }

    first_seq = skb_first->seq;

    //prev_full = (sock->send_buf_size == tsk->send_queue_size);
    while ((skb = SkbPeek(&sk->write_queue)) != NULL) {
        if (!skb->txmitted)
            break;
        // una < first_seq accounts for wrap-around segments
        if ((una < first_seq && skb->seq > una) || 
            (skb->seq >= 0 && skb->end_seq <= una)) { 
            SkbDequeue(&sk->write_queue);
            skb->refcnt--;
            assert(tsk->send_queue_size >= skb->dlen);
            tsk->send_queue_size -= skb->dlen;
            FreeSkb(skb);

            if (tsk->inflight > 0) {
                tsk->inflight--;
            }
        } else {
            break;
        }
    }


    print_debug ("inflight after = %d, queue_size = %d\n", tsk->inflight, tsk->send_queue_size);

    if (tsk->inflight == 0) {
        TcpStopRtoTimer(tsk);
    }

    if (skb == NULL || tsk->send_queue_size == 0) {
        /* No unacknowledged skbs, wake-up any sleeping send process */
         TcpSendNotify(sk);
    } 


    if (tsk->chk_pt_send_queue_size > 0) {
        // some new data has been added to write queue
        // coalesce any small untransmitted skbs into bigger ones before they
        // get transmitted.

        print_debug ("Write-queue size before coalescing: %d\n", sk->write_queue.qlen);
        //printf("Write-queue size before coalescing: %d\n", sk->write_queue.qlen);


        assert(tsk->chk_pt_send_skb != NULL);
        chk_pt_resume_skb = tsk->chk_pt_send_skb;
        list_for_each_safe_from(item, tmp, &chk_pt_resume_skb->list, &sk->write_queue.head) {
            if (!item) continue;
            skb = list_entry(item, struct sk_buff, list);

            if (!skb || skb->txmitted)
                break;
            
            th = tcp_hdr(skb);

            if (th->fin || th->syn || skb->dlen == 0) {
                // we don't consider skbs with zero length or with these flags for merging with prev_skb
                prev_skb = NULL;
                continue;
            }

            if (prev_skb)
                assert(prev_skb->dlen <= tsk->smss);

            if (th->psh) {
                if (prev_skb && 
                    !tcp_hdr(prev_skb)->psh &&
                    skb->dlen + prev_skb->dlen <= tsk->smss) {
                    assert(prev_skb->dlen > 0);
                    
                    rc = MergeSkbs(sk, prev_skb, skb);
                    if (rc < 0)
                        break;

                    //set psh flag in prev_skb
                    tmpth = tcp_hdr(prev_skb);
                    tmpth->ack = 1;
                    tmpth->psh = 1;
                    
                } 

                // set prev_skb as NULL because this cant me used for merging
                // further since there is a push flag
                prev_skb = NULL;

            } else if (prev_skb) {
                // no psh, syn or fin flags set
                if (skb->dlen + prev_skb->dlen > tsk->smss) {
                    // set prev_skb = skb
                    prev_skb = skb;
                    continue; 
                }

                // merge skb with prev_skb
                rc = MergeSkbs(sk, prev_skb, skb);
                if (rc < 0)
                    break;

                tmpth = tcp_hdr(prev_skb);
                tmpth->ack = 1;
                tmpth->psh = 0;

            } else {
                // no psh, syn, fin flags set and prev_skb is null and skb->len > 0
                // set this skb as prev_skb
                prev_skb = skb;
            }

        }

        tsk->chk_pt_send_queue_size = 0;
        tsk->chk_pt_send_skb = NULL;

        print_debug ("Write-queue size after coalescing: %d\n", sk->write_queue.qlen);
        //printf ("Write-queue size after coalescing: %d\n", sk->write_queue.qlen);

    }

    if (prev_skb && rc == 0) {
        // this is last skb in queue. make sure ack flag is set for this.
        tmpth = tcp_hdr(prev_skb);
        tmpth->ack = 1;
        //tmpth->psh = 1;

    }

    return rc;
}

static inline int __tcp_drop(struct vsock *sk, struct sk_buff *skb) {
    FreeSkb(skb);
    return 0;
}

static int TcpVerifySegment(struct tcp_sock *tsk, struct tcphdr *th, struct sk_buff *skb) {
    struct tcb *tcb = &tsk->tcb;

    if (skb->dlen > 0 && tcb->rcv_wnd == 0) return 0;

    // received segment's sequence must be in [rcv_nxt, rcv_nxt + rcv_wind)
    // beware the receive window may have wrapped around 0 (because of integer
    // overflow)

    if (th->seq < tcb->rcv_nxt ||
        th->seq >= (tcb->rcv_nxt + tcb->rcv_wnd)) {
        // its not in the window. Now check if the window itself has wrapped
        // around 0

        if ((tcb->rcv_nxt + tcb->rcv_wnd) < tcb->rcv_nxt) { 
            // overflow/wrap around
            if (th->seq >= tcb->rcv_nxt || th->seq < tcb->rcv_wnd - (UINT32_MAX - th->seq))
                return 1;
            printf ("Ovf: Outside rcv-wnd: seq: %u, rcv_nxt: %u, rcv_nxt + rcv_wnd: %u\n",
                    th->seq, tcb->rcv_nxt, tcb->rcv_nxt + tcb->rcv_wnd);
            
        } else {
            if (th->seq < tcb->rcv_nxt) {
                printf ("Before rcv-wnd: seq: %u, rcv_nxt: %u\n", th->seq, tcb->rcv_nxt);
            } else {
                printf ("Outside rcv-wnd: seq: %u, rcv_nxt: %u, rcv_nxt + rcv_wnd: %u\n",
                            th->seq, tcb->rcv_nxt, tcb->rcv_nxt + tcb->rcv_wnd);
            }
        }
        tcpsock_dbg("Received invalid segment", (&tsk->sk));
        return 0;
    }

    return 1;
}

/* TCP RST received */
static void TcpReset(struct vsock *sk) {
    sk->poll_events = (POLLOUT | POLLWRNORM | POLLERR | POLLHUP);
    int err;
    switch (sk->state) {
    case TCP_SYN_SENT:
        err = -ECONNREFUSED;
        break;
    case TCP_CLOSE_WAIT:
        err = -EPIPE;
        break;
    case TCP_CLOSE:
        return;
    default:
        err = -ECONNRESET;
        break;
    }

    TcpDone(sk, err);
}

static inline int TcpDiscard(struct tcp_sock *tsk, struct sk_buff *skb, struct tcphdr *th) {
    FreeSkb(skb);
    return 0;
}
/**** FOR HANDLING LISTEN AND ACCEPT *****/

static struct tcp_sock *TcpListenChildSock(struct tcp_sock *tsk,
    struct sk_buff *skb) {

    // Allocate new vsock for tcp
	struct vsock *newsk = TcpAllocSock(tsk->sk.protocol);

    if (!newsk)
        return NULL;

    // Allocate new vsocket to hold the vsock
    struct vsocket * newvsocket = AllockSocket();
	if (!newvsocket) {
        free(newsk);
        return NULL;
    }
    // Associate vsock with newly allocated vsocket.
    newvsocket->type = tsk->sk.sock->type;
	newvsocket->ops = tsk->sk.sock->ops;
    newsk->ops = &tcp_ops;
    newsk->protocol = tsk->sk.protocol;
    SockInitData(newvsocket, newsk);

	struct tcp_sock *newtsk = tcpsk(newsk);
	struct iphdr *iph;
    struct tcphdr *th;

    iph = IpHdr(skb);
    th = (struct tcphdr*) iph->data;

	newsk->saddr = iph->daddr;
	newsk->daddr = iph->saddr;
	newsk->sport = th->dport;
	newsk->dport = th->sport;

    TcpInitSock(newtsk);
    newtsk->parent = tsk;

    // Inherit SO_SNDBUF and SO_RCVBUF values from parent
    newtsk->tcb.rcv_wnd = tsk->tcb.rcv_wnd;
    newvsocket->rcv_buf_size = tsk->sk.sock->rcv_buf_size;
    newvsocket->send_buf_size = tsk->sk.sock->send_buf_size;
	
	ListAdd(&newtsk->list, &tsk->listen_queue);
    AddVSocketToList(newvsocket);

	return newtsk;
}

static int TcpListenState(struct tcp_sock *tsk, struct sk_buff *skb, struct tcphdr *th) {
	struct tcp_sock *newtsk;

    #ifdef DEBUG
    struct vsock * sk = &tsk->sk;
    #endif

    tcpsock_dbg("LISTEN STATE", sk);

	/* first check for an RST */
	tcpsock_dbg("1. check rst", sk);
	if (th->rst)
		goto discarded;
	/* sencod check for an AKC */
	tcpsock_dbg("2. check ack", sk);
	if (th->ack) {
		TcpSendResetTo(tsk, skb);
		goto discarded;
	}
	/* third check for a SYN (security check is ignored) */
	tcpsock_dbg("3. check syn",sk);
	/* RFC 2873: ignore the security/compartment check */
	if (!th->syn)
		goto discarded;

    if (TcpAcceptQueueFull(tsk)) {
        TcpSendResetTo(tsk, skb);
		goto discarded;
    }

	/* set for first syn */
	newtsk = TcpListenChildSock(tsk, skb);
	if (!newtsk) {
		tcpsock_dbg("cannot alloc new sock", sk);
		goto discarded;
	}
    print_debug ("Configuring new accepted socket \n!");
    
    // read from the received packet. this is the iss of the tcp-client or
    // equivalently the irs of the tcp server
	newtsk->tcb.irs = th->seq;

    // create a new iss. this is the iss of the tcp-server 
	newtsk->tcb.iss = GenerateISS();

    // indicating that ack to this synack must have th->seq + 1 as its sequence number
	newtsk->tcb.rcv_nxt = th->seq + 1;

    // sequence number to be used for the next packet which will sent after
    // this syn-ack. 
    newtsk->tcb.snd_nxt = newtsk->tcb.iss + 1;

    // this syn-ack's sequence number will be iss. Since this has not yet
    // been acknowledged, mark una as equal to iss
	newtsk->tcb.snd_una = newtsk->tcb.iss;

    // relevant send window settings
    newtsk->tcb.snd_wnd = th->win;
	newtsk->tcb.snd_wl1 = th->seq;
	newtsk->tcb.snd_wl2 = th->ack_seq;
    TcpSelectInitialWindow(&newtsk->tcb.rcv_wnd);

    print_debug ("Newtsk: IRS: %u, ISS: %u\n", newtsk->tcb.irs, newtsk->tcb.iss);
    tcp_set_state((&newtsk->sk), TCP_SYN_RECEIVED);

	/* send seq=server.iss, ack=rcv.nxt (=client.iss+1), syn|ack */
	TcpSendSynackTo(&newtsk->sk, skb);
    
    // A timer would automatically be started after the syn-ack is sent.
    // the ack for this syn-ack will be processed in step-5 in TcpInputState function

    // At end of this state:
    // una = server.iss
    // snd.nxt = server.iss + 1
    // rcv.nxt = client.iss + 1
    // irs = client.iss
	
    
discarded:
	FreeSkb(skb);
    return 0;
}

/* handle sock acccept queue when receiving ack in SYN-RECV state */
static int TcpSynRecvAck(struct tcp_sock *tsk) {
	if (!tsk->parent || tsk->parent->sk.state != TCP_LISTEN) {
        return -1;
    }
	if (TcpAcceptQueueFull(tsk->parent))
		return -1;

	TcpAcceptEnqueue(tsk);
	tcpsock_dbg("Passive three-way handshake successes!", (&tsk->sk));
    tsk->parent->sk.accept_wait_wakeup = 1;
	return 0;
}

/**** END FOR HANDLING LISTEN AND ACCEPT ***/
//! This is tcp-client side code for handling any incoming packets after
//  a syn packet has been sent. 
//  After a syn has been sent (at this stage):
//  una = client.iss, snd.nxt = client.iss + 1, rcv.nxt = 0
//  The syn would have been sent with:
//      seq=client.iss, ack_seq=0
//  A proper syn-ack from server will have:
//      seq=server.iss, ack_seq=client.iss + 1 
static int TcpSynSent(struct tcp_sock *tsk, struct sk_buff *skb, struct tcphdr *th) {
    // should be a syn or syn-ack
    struct tcb *tcb = &tsk->tcb;
    struct vsock *sk = &tsk->sk;
    int is_ack_acceptable = 1;

    tcpsock_dbg("state is synsent", sk);
    
    if (th->ack) {
        if (th->ack_seq <= tcb->iss || th->ack_seq > tcb->snd_nxt) {
            tcpsock_dbg("ACK is unacceptable", sk);
            if (!th->rst)
                TcpSendResetTo(tsk, skb);
            is_ack_acceptable = 0;
            goto discard;   
        }
        if (th->ack_seq < tcb->snd_una || th->ack_seq > tcb->snd_nxt) {
            tcpsock_dbg("ACK is unacceptable", sk);
            is_ack_acceptable = 0;
            goto discard;
        }
    } else {
        is_ack_acceptable = 0;
    }

    /* ACK is acceptable */
    if (th->rst) {
        if (is_ack_acceptable)
            TcpReset(&tsk->sk);
        goto discard;
    } 

    /* third check the security and precedence -> ignored */

    /* fourth check the SYN bit */
    if (!th->syn) {
        goto discard;
    }

    // th->seq would be iss of server socket
    tcb->rcv_nxt = th->seq + 1;
    tcb->irs = th->seq;

    if (th->ack) {
        /* Any packets in RTO queue that are acknowledged here should be removed */
        TcpCleanRtoQueue(sk, th->ack_seq);
    }

    if (th->ack_seq > tcb->iss) {
        // this will execute if there is an ack in the packet.

        // this code is executed by client side socket after receiving a syn-ack from
        // server.
        tcp_set_state(sk, TCP_ESTABLISHED);

        
        // We are setting snd_una to client.iss + 1 because everything has been
        // acked. So oldest una equals snd.nxt
        tcb->snd_una = tcb->snd_nxt;

        tsk->backoff = 0;
        tcb->snd_wnd = th->win;
		tcb->snd_wl1 = th->seq;
		tcb->snd_wl2 = th->ack_seq;

        /* RFC 6298: Sender SHOULD set RTO <- 1 second */

        tsk->rto = 200;
        // directly send ack because socket book-keeping is upto date.
        // Send ack with seq=snd.nxt (=client.iss + 1), ack_seq=rcv.nxt (=server.iss + 1)
        // this ack will be processed in step-5 of TcpInputState function
        TcpSendAck(&tsk->sk);

        TcpRearmUserTimeout(&tsk->sk);
        TcpParseOpts(tsk, th);
        SockConnected(sk);

    } else {
        // this will execute iff there is no ack in the packet. This is
        // a simultaneous open. This is just a syn packet.

        // this code is executed by client side socket after receiving a syn from
        // server as a part of simultaneous open. the socket book-keeping
        // is upto date here as well because a connect would already have been
        // called before it ever gets here.
        tcp_set_state(sk, TCP_SYN_RECEIVED);

        // this may be redundant. we are just setting una to client.iss just
        // for safety. Out previously sent syn has not been acked yet.

        tcb->snd_una = tcb->iss;

        // Send syn-ack with seq=client.iss, ack_seq=rcv.nxt (=server.iss+1)
        // this will be processed in step-5 of TcpInputState function on the remote end
        TcpSendSynack(&tsk->sk);
    }

    // At the end of this stage:
    // una = [client.iss + 1 (or client.iss if simultaneous open)]
    // snd.nxt = client.iss + 1
    // rcv.nxt = server.iss + 1
    // irs = server.iss
    
discard:
    tcp_drop(sk, skb);
    return 0;
}

static int TcpClosed(struct tcp_sock *tsk, struct sk_buff *skb, struct tcphdr *th) {

    int rc = -1;
    tcpsock_dbg("state is closed", (&tsk->sk));

    if (th->rst) {
        TcpDiscard(tsk, skb, th);
        rc = 0;
        goto out;
    }  
    rc = TcpSendResetTo(tsk, skb);
    FreeSkb(skb);
out:
    return rc;
}

static _inline void __TcpUpdateWindow(struct tcp_sock *tsk, struct tcphdr *th) {
    /* SND.WND is an offset from SND.UNA */

    
    tsk->tcb.snd_wnd = th->win;
    tsk->tcb.snd_wl1 = th->seq;
    tsk->tcb.snd_wl2 = th->ack_seq;

    print_debug ("Updated snd-wnd = %u\n", tsk->tcb.snd_wnd);
    
}

static _inline void TcpUpdateWindow(struct tcp_sock *tsk, struct tcphdr *th)
{
	if ((tsk->tcb.snd_una <= th->ack_seq && th->ack_seq <= tsk->tcb.snd_nxt) &&
		(tsk->tcb.snd_wl1 < th->seq ||
			(tsk->tcb.snd_wl1 == th->seq && tsk->tcb.snd_wl2 <= th->ack_seq)))
		__TcpUpdateWindow(tsk, th);
}

/*
 * Follows RFC793 "Segment Arrives" section closely
 */ 
int TcpInputState(struct vsock *sk, struct tcphdr *th, struct sk_buff *skb) {
    struct tcp_sock *tsk = tcp_sk(sk);
    struct tcb *tcb = &tsk->tcb;

    tcpsock_dbg("input state", sk);

    switch (sk->state) {
        case TCP_CLOSE:
            return TcpClosed(tsk, skb, th);
        case TCP_LISTEN:
            return TcpListenState(tsk, skb, th);
        case TCP_SYN_SENT:
            return TcpSynSent(tsk, skb, th);
    }

    /* "Otherwise" section in RFC793 */
    /* first check sequence number */
    if (!TcpVerifySegment(tsk, th, skb)) {
        /* RFC793: If an incoming segment is not acceptable, an acknowledgment
         * should be sent in reply (unless the RST bit is set, if so drop
         * the segment and return): */
        if (!th->rst) {
            TcpSendAck(sk);
        }
        return_tcp_drop(sk, skb);
    }
    
    /* second check the RST bit */
    if (th->rst) {
        switch(sk->state) {
            case TCP_LISTEN:
                break;
            case TCP_SYN_SENT:
            case TCP_SYN_RECEIVED:
            case TCP_ESTABLISHED:
            case TCP_FIN_WAIT_1:
            case TCP_FIN_WAIT_2:
            case TCP_CLOSE_WAIT:
                tsk->sk.err = -ECONNRESET;
                tsk->sk.ops->recv_notify(&tsk->sk);
            case TCP_CLOSING:
            case TCP_LAST_ACK:
            case TCP_TIME_WAIT:
                TcpDone(sk, -ECONNRESET);
        }
        return_tcp_drop(sk, skb);
        
    }
    
    /* third check security and precedence -skipped */

    /* fourth check the SYN bit */
    if (th->syn) {
        /* RFC 5961 Section 4.2 */

        if (sk->state != TCP_SYN_RECEIVED) {
            TcpSendResetTo(tsk, skb);
            tsk->sk.err = -ECONNRESET;
            tsk->sk.ops->recv_notify(&tsk->sk);
            TcpDone(sk, -ECONNRESET);
            return_tcp_drop(sk, skb);
        } else {
            // it we receive a syn-ack in state syn_received, it is a part of
            // simultaneous open. We process the ack in step-5.
            th->syn = 0;
        }
    }
    
    /* fifth check the ACK field */
    if (!th->ack) {
        return_tcp_drop(sk, skb);
    }

    // Step-5: ACK bit is on
    switch (sk->state) {
        case TCP_SYN_RECEIVED:
            // Handles ack for a syn-ack. This may be a simultaneous open 
            // in which case this case will be executed in both client and server
            // else, it will be executed only on server
            // If simultaneous open:
            //      Client: una: client.iss, snd.nxt = client.iss + 1, rcv.nxt = server.iss + 1, irs = server.iss
            //      Server: una: server.iss, snd.nxt = server.iss + 1, rcv.nxt = client.iss + 1, irs = client.iss
            //      Rx: ack: seq=remote.iss, ack_seq=my.iss + 1
            // Else:
            //      Client: una: client.iss + 1, snd.nxt = client.iss + 1, rcv.nxt = server.iss + 1, irs = server.iss
            //      Server: una: server.iss, snd.nxt = server.iss + 1, rcv.nxt = client.iss + 1, irs = client.iss
            //      Rx: ack: seq=remote.iss, ack_seq=my.iss + 1
            if (tcb->snd_una > tcb->snd_nxt) {
                // there has been an overflow - wrap around
                if (th->ack_seq > tcb->snd_una || th->ack_seq <= tcb->snd_nxt) {
                    if (TcpSynRecvAck(tsk) < 0) {
                        return_tcp_drop(sk, skb);
                    }
                    tcp_set_state(sk, TCP_ESTABLISHED);
                } else {
                    TcpSendResetTo(tsk, skb);
                    return_tcp_drop(sk, skb);   
                }
            } else if (tcb->snd_una < th->ack_seq && th->ack_seq <= tcb->snd_nxt) {
                if (TcpSynRecvAck(tsk) < 0) {
                    return_tcp_drop(sk, skb);
                }
                // It falls through below into TCP_ESTABLISHED cases for further processing
                tcp_set_state(sk, TCP_ESTABLISHED);
            } else {
                TcpSendResetTo(tsk, skb);
                return_tcp_drop(sk, skb);
            }
        case TCP_ESTABLISHED:
        case TCP_FIN_WAIT_1:
        case TCP_FIN_WAIT_2:
        case TCP_CLOSE_WAIT:
        case TCP_CLOSING:
        case TCP_LAST_ACK:
            if (tcb->snd_una > tcb->snd_nxt) { 
                // there has been an overflow - wrap around
                if (th->ack_seq >= tcb->snd_una || th->ack_seq <= tcb->snd_nxt) {
                    // ack_seq points to rcv.nxt of remote end i.e the sequence
                    // number the remote end expects next. This has not yet been
                    // acked. So we force set una to ack_seq.

                    tcb->snd_una = th->ack_seq;
                    /* Any segments on the retransmission queue which are thereby
                    entirely acknowledged are removed. */
                    TcpCleanRtoQueue(sk, tcb->snd_una);
                    TcpRtt(tsk);
                    TcpUpdateWindow(tsk, th);

                    // continue processing
                } else {
                    return_tcp_drop(sk, skb);
                }

            }  else if (tcb->snd_una <= th->ack_seq && th->ack_seq <= tcb->snd_nxt) {
                // ack_seq points to rcv.nxt of remote end i.e the sequence
                // number the remote end expects next. This has not yet been
                // acked. So we force set una to ack_seq.    
                tcb->snd_una = th->ack_seq;

                /* Any segments on the retransmission queue which are thereby
                entirely acknowledged are removed. */
                TcpCleanRtoQueue(sk, tcb->snd_una);
                TcpRtt(tsk);
                TcpUpdateWindow(tsk, th);
            } else {

                // If the ACK is a duplicate, it can be ignored
                return_tcp_drop(sk, skb);
            }
            break;
    }

    /* If the write queue is empty, it means our FIN was acked */
    if (SkbQueueEmpty(&sk->write_queue)) {
        switch (sk->state) {
            case TCP_FIN_WAIT_1:
                tcp_set_state(sk, TCP_FIN_WAIT_2);
            case TCP_FIN_WAIT_2:
                break;
            case TCP_CLOSING:
                /* In addition to the processing for the ESTABLISHED state, if
                * the ACK acknowledges our FIN then enter the TIME-WAIT state,
                otherwise ignore the segment. */
                tcp_set_state(sk, TCP_TIME_WAIT);
                break;
            case TCP_LAST_ACK:
                /* The only thing that can arrive in this state is an acknowledgment of our FIN.  
                * If our FIN is now acknowledged, delete the TCB, enter the CLOSED state, and return. */
                FreeSkb(skb);
                return TcpDone(sk, 0);
            case TCP_TIME_WAIT:
                /* The only valid thing that can arrive in this state is a
                retransmission of the remote FIN. It will be handled in step-8 */
                if (skb->seq == tcb->rcv_nxt) {
                    // if this is an ack and not a fin, we can immediately terminate
                    // connection because the remote has ack'ed our connection termination
                    // request.
                    printf ("Received ack in time_wait !\n");
                    FreeSkb(skb);
                    return TcpDone(sk, 0);
                }
                break;
        }
    }
    
    /* sixth, check the URG bit - skipped */

    int expected = skb->seq == tcb->rcv_nxt;
    
    /* seventh, process the segment txt */
    switch (sk->state) {
        case TCP_ESTABLISHED:
        case TCP_FIN_WAIT_1:
        case TCP_FIN_WAIT_2:
            if (th->psh || skb->dlen > 0) {
                if (TcpDataQueue(tsk, th, skb) < 0) {
                    goto drop_and_unlock;
                }
            }      
            break;
        case TCP_CLOSE_WAIT:
        case TCP_CLOSING:
        case TCP_LAST_ACK:
        case TCP_TIME_WAIT:
            /* This should not occur, since a FIN has been received from the
               remote side.  Ignore the segment text. */
        break;
    }


    /* eighth, check the FIN bit */
    // the expected check is included to not process fin right away if it arrives out of order
    // so we don't send an ack for it. It will eventually be re-transmitted.
    if (th->fin && expected) {

        //printf ("Received fin !\n");
        //fflush(stdout);
        
        tcpsock_dbg("Received FIN", sk);

        switch (sk->state) {
            case TCP_CLOSE:
            case TCP_LISTEN:
            case TCP_SYN_SENT:
                // RFC-793: Do not process, since SEG.SEQ cannot be validated
                goto drop_and_unlock;
        }

        tcb->rcv_nxt = th->seq + 1;
        tsk->flags |= TCP_FIN;
        sk->poll_events |= (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND);
        
        // seq = snd.nxt, ack_seq=rcv.nxt
        TcpSendAck(sk);
        tsk->sk.ops->recv_notify(&tsk->sk);

        switch (sk->state) {
            case TCP_SYN_RECEIVED:
            case TCP_ESTABLISHED:
                tcp_set_state(sk, TCP_CLOSE_WAIT);
                break;
            case TCP_FIN_WAIT_1:
                /* 
                If our FIN has been ACKed (perhaps in this segment), then
                enter TIME-WAIT, start the time-wait timer, turn off the other
                timers; otherwise enter the CLOSING state. 
                */
                if (SkbQueueEmpty(&sk->write_queue)) {
                    printf ("Entering time-wait from FIN_WAIT_1\n");
                    TcpEnterTimeWait(sk);
                } else {
                    tcp_set_state(sk, TCP_CLOSING);
                }

                break;
            case TCP_FIN_WAIT_2:
                /*
                Enter the TIME-WAIT state.  Start the time-wait timer, turn
                off the other timers.
                */
                printf ("Entering time-wait from FIN_WAIT_2\n");
                TcpEnterTimeWait(sk);
                break;
            case TCP_CLOSE_WAIT:
            case TCP_CLOSING:
            case TCP_LAST_ACK:
                /* 
                Remain in the state and do-nothing. We have already acked the fin.
                */
                break;
            case TCP_TIME_WAIT:
                /*
                TODO: Remain in the TIME-WAIT state.  Restart the 2 MSL time-wait
                timeout.
                */
                break;
        }
    }

    /* Congestion control and delacks */
    switch (sk->state) {
    case TCP_ESTABLISHED:
    case TCP_FIN_WAIT_1:
    case TCP_FIN_WAIT_2:
        if (expected) {
            // we got an expected-in sequence ack (presumably for previous segment send).
            // Now we start sending other pending segments which may be present
            // in the write queue 
            TcpStopDelackTimer(tsk);

            if (tcb->rcv_wnd > sk->sock->rcv_buf_size - tsk->rcv_queue_size) {
                // receiver is lagging behind. trigger an additional ack to be sent
                // when socket receive buffer is finally cleared. This additional
                // ack would have an updated rcv window which would be set at the sender
                tsk->trigger_rcv_ack = 1;
            }
            

            int pending = SkbQueueLen(&sk->write_queue) > 0;
            /* RFC1122:  A TCP SHOULD implement a delayed ACK, but an ACK should not
             * be excessively delayed; in particular, the delay MUST be less than
             * 0.5 seconds, and in a stream of full-sized segments there SHOULD 
             * be an ACK for at least every second segment. */
            if (tsk->inflight == 0 && pending > 0) {
                // we implictly acknowledge the received in sequence segment
                // since the ack flag in the send packet will be set as well.
                pending = TcpSendNext(sk);
                tsk->inflight += pending;


                if (pending > 0) {
                    print_debug ("Send-next: Batch size: %d\n", pending);
                    TcpRearmRtoTimer(tsk);
                }
                
            } else if (th->psh || (skb->dlen > 0)) {
                tsk->chk_pt_rcv_queue_size = 0;
                TcpSendAck(sk);
            }
        }
    }

    FreeSkb(skb);

unlock:
    return 0;
drop_and_unlock:
    tcp_drop(sk, skb);
    goto unlock;
}

int TcpReceive(struct tcp_sock *tsk, void *buf, int len) {
    int rlen = 0;
    int curlen = 0;
    struct vsock *sk = &tsk->sk;
    struct vsocket *sock = sk->sock;

    memset(buf, 0, len);
    tsk->blocked = 0;

    while (rlen < len) {
        curlen = TcpDataDequeue(tsk, buf + rlen, len - rlen);

        rlen += curlen;

        if (tsk->flags & TCP_PSH) {
            tsk->flags &= ~TCP_PSH;
            break;
        }

        // TCP_FIN indicates the remote socket doesn't have any more data to send
        if (tsk->flags & TCP_FIN || rlen == len) break;

        if (sock->flags & O_NONBLOCK) {
            if (rlen == 0) {
                rlen = -EAGAIN;
            } 
            break;
        } else {
            tsk->blocked = 1;
            
            do {
                SocketRelease(sock);
                RegisterSysCallWait();
                SocketWrAcquire(sock); 
                if (sk->recv_wait_wakeup) {
                    sk->recv_wait_wakeup = 0;
                    break;
                } 
            } while (1);
        }

        if (tsk->sk.err < 0) {
            tcpsock_dbg("Breaking out of tcp-receive due to socket error: ", (&tsk->sk));
            break;
        }
    }

    if (rlen >= 0 && tsk->sk.err >= 0) TcpRearmUserTimeout(sk);

    tsk->flags &= ~TCP_PSH;

    if (tsk->trigger_rcv_ack && tsk->rcv_queue_size == 0) {
        printf ("Sending tsk trigger rcv ack to update window at sender!\n");
        TcpSendAck(sk);
        tsk->trigger_rcv_ack = 0;
    }
    
    return rlen;
}
