

#include "netdev.h"
#include "ip.h"
#include "basic.h"
#include "syshead.h"
#include "utils.h"
#include "timer.h"
#include "skbuff.h"
#include "../VT_functions.h"


static struct netdev netdev;

static LIST_HEAD(netdev_pkt_queue);

#define NETDEV_STATUS_PROCESS_EXITING -2
#define NETDEV_STATUS_EXP_EXITING -1
#define NETDEV_STATUS_NORMAL 0

void NetDevInit(uint32_t src_ip_addr) {
    netdev.in_callback = 1;
    print_debug ("Initializing Net-dev !\n");
    srand((unsigned) netdev.uniqueID); 
    netdev.src_ip_addr = ntohl(src_ip_addr);
    netdev.mtu = 1500;

    netdev.raw_sock_fd = socket (AF_INET, SOCK_RAW | SOCK_NONBLOCK, IPPROTO_TCP);
    

    int one = 1;
    const int *val = &one;
    if(netdev.raw_sock_fd == -1) {
        //socket creation failed, may be because of non-root privileges
        perror("Netdev: Failed to create raw socket !");
	    netdev.in_callback = 0;
        exit(1);
    }
    if(setsockopt(netdev.raw_sock_fd, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) {
        perror("Netdev: raw socket setsockopt() error !");
	    netdev.in_callback = 0;
        exit(1);
    }

    int fwmark;
    fwmark = 40;
    if(-1 == setsockopt(netdev.raw_sock_fd, SOL_SOCKET, SO_MARK, &fwmark, sizeof (fwmark))) {
        perror("failed setting mark for raw socket packets");
    }
    IpInit(ntohl(src_ip_addr));
    
    netdev.initialized = 1;
    print_debug ("Net-dev initialized !\n");
    netdev.in_callback = 0;
}

void MarkNetDevExiting() {
    netdev.exiting = 1;
}

int GetStackTracerID() {
    return netdev.tracerID;
}

void SetNetDevStackID(int tracerID, int stackID, float nicSpeedMps) {
    char uniqueIDStr[100];
    netdev.tracerID = tracerID;
    netdev.stackID = stackID;
    netdev.in_callback = 1;
    netdev.nicSpeedMbps = nicSpeedMps;
    netdev.currTsliceQuantaUs = 1;
    netdev.netDevPktQueueSize = 0;
    memset(uniqueIDStr, 0, 100);
    sprintf(uniqueIDStr, "%d%d", netdev.tracerID, netdev.stackID);
    netdev.uniqueID = atoi(uniqueIDStr);
    printf ("TCP stack unique-id: %d\n", netdev.uniqueID);
    MarkStackInActive(netdev.tracerID, netdev.stackID);
    netdev.exiting = 0;
    netdev.in_callback = 0;
}

int IsNetDevInCallback() {
    return netdev.in_callback;
}


void SetNetDevCurrTsliceQuantaUs(long quantaUs) {
    //printf ("Setting current tslice US to %ld\n", quantaUs);
    //printf ("Netdev QLen = %d\n", netdev.netDevPktQueueSize);

    netdev.currTsliceQuantaUs += quantaUs;
}

// called when first socket is created inside socket syscall.
void MarkNetworkStackActive(uint32_t src_ip_addr) {
    NetDevInit(src_ip_addr);
    netdev.in_callback = 1;
    netdev.netDevPktQueueSize = 0;
    MarkStackActive(netdev.tracerID, netdev.stackID);
    netdev.in_callback = 0;
}

void MarkNetworkStackInactive() {
    if (!netdev.initialized)
        return;
    netdev.in_callback = 1;
    netdev.initialized = 0;
    close(netdev.raw_sock_fd);
    MarkStackInActive(netdev.tracerID, netdev.stackID);
    netdev.in_callback = 0;
}


int NetDevTransmit(struct sk_buff *skb, struct sockaddr_in * skaddr,
    int payload_hash, int payload_len) {
    int ret = 0;
    int old_state = netdev.in_callback;
    netdev.in_callback = 1;
    assert(netdev.initialized == 1);
    /*if ((ret = sendto (netdev.raw_sock_fd, (char *)skb->data, skb->len, 0, 
                skaddr, sizeof (struct sockaddr_in)) < 0)) {
        perror ("Send-to Failed !\n");
	}*/

    struct netdev_pkt * new_pkt = malloc(sizeof(struct netdev_pkt));
    assert(new_pkt != NULL);
    memset(new_pkt->data, 0, BUFLEN);
    assert(skb->len <= BUFLEN);
    memcpy(new_pkt->data, (char *)skb->data, skb->len);
    memcpy(&new_pkt->sin, skaddr, sizeof(struct sockaddr_in));
    ListInit(&new_pkt->list);
    ListAddTail(&new_pkt->list, &netdev_pkt_queue);
    new_pkt->len = skb->len;
    new_pkt->payload_hash = payload_hash;
    new_pkt->payload_len = payload_len;
    netdev.netDevPktQueueSize ++;

    //printf ("Queuing new pkt of length: %d\n", new_pkt->len);
    //fflush(stdout);

    netdev.in_callback = old_state;
    return skb->len;
}

void NetDevSendQueuedPackets() {

    struct netdev_pkt * pkt;
    struct list_head *item, *tmp;
    float availQuantaUs = netdev.currTsliceQuantaUs;
    float currPktUsedQuantaUs = 0;
    float usedPktQuantaUs = 0;
    int numQueuedPkts = 0;
    int ret = 0;
    int old_state = netdev.in_callback;
    netdev.in_callback = 1;
    assert(netdev.initialized == 1);
    s64 currTime = GetCurrentTimeTracer(GetStackTracerID());

    
    list_for_each_safe(item, tmp, &netdev_pkt_queue) {
        pkt = list_entry(item, struct netdev_pkt, list);

        if (pkt) {
            currPktUsedQuantaUs = (float)(pkt->len * 8.0) / (netdev.nicSpeedMbps);
        } else {
            break;
        }

        //printf ("Attempting to process any queued pkts: availQuantaUs: %f, currPktQuanta: %f, Pkt-len = %d !\n",
        //    availQuantaUs, currPktUsedQuantaUs, pkt->len);
        //fflush(stdout);
        numQueuedPkts ++;

        if (pkt && availQuantaUs - currPktUsedQuantaUs > 0) {
            availQuantaUs -= currPktUsedQuantaUs;
            usedPktQuantaUs += currPktUsedQuantaUs;

            //printf ("Sending new pkt !\n");
            //fflush(stdout);
            SetPktSendTimeAPI(netdev.tracerID, pkt->payload_hash, pkt->payload_len, 
                currTime + ((int)usedPktQuantaUs)* NSEC_PER_US);
            if ((ret = sendto (netdev.raw_sock_fd, (char *)pkt->data, pkt->len, 0, 
                &pkt->sin, sizeof (struct sockaddr_in)) < 0)) {
                perror ("Send-to Failed !\n");
	        }
            netdev.netDevPktQueueSize --;
            ListDel(&pkt->list);
            free(pkt);
        } else {
            break;
        }

        
    }

    if (!numQueuedPkts) {
        //reset
        netdev.netDevPktQueueSize = 0;
        netdev.currTsliceQuantaUs = 0.0;
    } else {
        netdev.currTsliceQuantaUs = availQuantaUs;
    }

    if (netdev.netDevPktQueueSize) {
        UpdateStackSendRtxTime(netdev.tracerID, netdev.stackID, TimerGetTick());
    } else {
        UpdateStackSendRtxTime(netdev.tracerID, netdev.stackID, 0);
    }

    netdev.in_callback = old_state;

}

static int NetDevReceive(struct sk_buff *skb) {
    IpRcv(skb);
    return 0;
}

void NetDevRxLoop() {
    
    int packet_size = -1;
    struct sk_buff *skb;
    if (!netdev.initialized)
        return;
    netdev.in_callback = 1;
    while (1) {

        if (packet_size <= 0)
            skb = AllocSkb(BUFLEN);
        
        packet_size = recvfrom(
            netdev.raw_sock_fd , (char *)skb->data , BUFLEN , 0 , NULL, NULL);
        

        if (packet_size < 0) {
            FreeSkb(skb);
            break;
        } else {
            //printf ("Received pkt !\n");
            //fflush(stdout);
            NetDevReceive(skb);
            packet_size = 0;
        }
    }
    
    TimersProcessTick();
    GarbageCollectSockets();
    NetDevSendQueuedPackets();

    MarkStackRxLoopComplete(netdev.tracerID, netdev.stackID);
    // Update stack retransmit lookahead here as well
    //UpdateStackSendRtxTime(netdev.tracerID, netdev.stackID,
    //    GetEarliestRtxSendTime());
    

    if (!NumActiveSockets()) {
        MarkNetworkStackInactive();
        TimersCleanup();
    }
    netdev.in_callback = 0;
}

void MarkInNetDevCallback() {
    netdev.in_callback = 1;
}

void ClearInNetDevCallback() {
    netdev.in_callback = 0;
}


void * StackThread(void *arg) {
    s64 ret;
    int exiting = 0;
    int num_rounds = 0;
    while (1) {
        //printf ("Entering register stack thread wait !\n");
        //fflush(stdout);
        ret = RegisterStackThreadWait(netdev.tracerID, netdev.stackID);
        //printf ("Resuming from register stack thread wait: %d!\n", ret);
        //fflush(stdout);
        if (exiting || netdev.exiting || ret == NETDEV_STATUS_PROCESS_EXITING) {
            if (!exiting) {
                printf ("Detected process exit for stack thread with id: %d!\n", netdev.stackID);
                printf ("Continuing until no active sockets are left. Num-active-sockets: %d\n",
                        NumActiveSockets());
            }
	        fflush(stdout);
            exiting = 1;
            num_rounds ++;
            
            if (!NumActiveSockets()) {
                printf ("No active sockets left. Num rounds: %d. Triggering stack thread finish !\n",
                        num_rounds);
                fflush(stdout);
                TriggerStackThreadFinish(netdev.tracerID, netdev.stackID);
                break;
            } else {
                //TODO: Update this to get the proper tslice quanta-us
                SetNetDevCurrTsliceQuantaUs(1);
                NetDevRxLoop();
            }
        } else if (ret == NETDEV_STATUS_EXP_EXITING) {
            printf ("Exp finishing. Stopping stack thread with id: %d!\n", netdev.stackID);
            fflush(stdout);
            break;
        } else {
            long currQuantaUs = (long)(ret / NSEC_PER_US);
            if (!currQuantaUs)
                currQuantaUs = 1;
            SetNetDevCurrTsliceQuantaUs(currQuantaUs);
            NetDevRxLoop();
        }
    }
    return NULL;
}
