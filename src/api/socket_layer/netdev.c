

#include "netdev.h"
#include "ip.h"
#include "basic.h"
#include "syshead.h"
#include "utils.h"
#include "timer.h"
#include "skbuff.h"
#include "../VT_functions.h"

static struct netdev netdev;

void NetDevInit(uint32_t src_ip_addr) {
    netdev.in_callback = 1;
    print_debug ("Initializing Net-dev !\n");
    srand(time(0)); 
    netdev.src_ip_addr = ntohl(src_ip_addr);
    netdev.mtu = 1500;

    netdev.raw_sock_fd = socket (AF_INET, SOCK_RAW | SOCK_NONBLOCK, IPPROTO_TCP);
    

    int one = 1;
    const int *val = &one;
    if(netdev.raw_sock_fd == -1) {
        //socket creation failed, may be because of non-root privileges
        perror("Netdev: Failed to create raw socket !");
        exit(1);
    }
    if(setsockopt(netdev.raw_sock_fd, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) {
        perror("Netdev: raw socket setsockopt() error !");
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

void SetNetDevStackID(int tracerID, int stackID) {
    netdev.tracerID = tracerID;
    netdev.stackID = stackID;
    netdev.in_callback = 0;
}

int IsNetDevInCallback() {
    return netdev.in_callback;
}

// called when first socket is created inside socket syscall.
void MarkNetworkStackActive(uint32_t src_ip_addr) {
    NetDevInit(src_ip_addr);
    MarkStackActive(netdev.tracerID, netdev.stackID);
}

void MarkNetworkStackInactive() {
    if (!netdev.initialized)
        return;

    netdev.initialized = 0;
    close(netdev.raw_sock_fd);
    MarkStackInActive(netdev.tracerID, netdev.stackID);
}


int NetDevTransmit(struct sk_buff *skb, struct sockaddr_in * skaddr) {
    int ret = 0;
    assert(netdev.initialized == 1);
    netdev.in_callback = 1;
    if ((ret = sendto (netdev.raw_sock_fd, (char *)skb->data, skb->len, 0, 
                skaddr, sizeof (struct sockaddr_in)) < 0)) {
        perror ("Send-to Failed !\n");
	}
    netdev.in_callback = 0;
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
    netdev.in_callback = 0;
    MarkStackRxLoopComplete(netdev.tracerID, netdev.stackID);
    TimersProcessTick();
    // Update stack retransmit lookahead here as well
    UpdateStackSendRtxTime(netdev.tracerID, netdev.stackID,
        GetEarliestRtxSendTime());
    

    if (!NumActiveSockets()) {
        MarkNetworkStackInactive();
        TimersCleanup();
    }
}


void * StackThread(void *arg) {
    while (1) {
        if (RegisterStackThreadWait(netdev.tracerID, netdev.stackID) <= 0)
            break;
        NetDevRxLoop();
    }
    return NULL;
}
