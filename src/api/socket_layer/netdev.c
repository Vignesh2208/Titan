

#include "netdev.h"
#include "ip.h"
#include "basic.h"
#include "syshead.h"
#include "utils.h"
#include "timer.h"
#include "skbuff.h"
#include "../VT_functions.h"


static struct netdev netdev;

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

void SetNetDevStackID(int tracerID, int stackID) {
    char uniqueIDStr[100];
    netdev.tracerID = tracerID;
    netdev.stackID = stackID;
    netdev.in_callback = 1;
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

// called when first socket is created inside socket syscall.
void MarkNetworkStackActive(uint32_t src_ip_addr) {
    NetDevInit(src_ip_addr);
    netdev.in_callback = 1;
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


int NetDevTransmit(struct sk_buff *skb, struct sockaddr_in * skaddr) {
    int ret = 0;
    int old_state = netdev.in_callback;
    netdev.in_callback = 1;
    assert(netdev.initialized == 1);
    if ((ret = sendto (netdev.raw_sock_fd, (char *)skb->data, skb->len, 0, 
                skaddr, sizeof (struct sockaddr_in)) < 0)) {
        perror ("Send-to Failed !\n");
	}
    netdev.in_callback = old_state;
    return ret;
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
            NetDevReceive(skb);
            packet_size = 0;
        }
    }
    
    MarkStackRxLoopComplete(netdev.tracerID, netdev.stackID);
    TimersProcessTick();
    GarbageCollectSockets();
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
    int ret;
    int exiting = 0;
    int num_rounds = 0;
    while (1) {
        ret = RegisterStackThreadWait(netdev.tracerID, netdev.stackID);
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
                NetDevRxLoop();
            }
        } else if (ret == NETDEV_STATUS_EXP_EXITING) {
            printf ("Exp finishing. Stopping stack thread with id: %d!\n", netdev.stackID);
            fflush(stdout);
            break;
        } else {
            NetDevRxLoop();
        }
    }
    return NULL;
}
