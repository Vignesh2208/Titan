#ifndef NETDEV_H
#define NETDEV_H
#include "syshead.h"
#include "skbuff.h"
#include "utils.h"

#define BUFLEN 1600
#define MAX_ADDR_LEN 32

#define netdev_dbg(fmt, args...)                \
    do {                                        \
        print_debug("NETDEV: "fmt, ##args);     \
    } while (0)

struct netdev {
    uint32_t src_ip_addr;
    uint32_t mtu;
    int netDevPktQueueSize;
    float nicSpeedMbps;
    float currTsliceQuantaUs;
    int stackID;
    int tracerID;
    int uniqueID;
    int raw_sock_fd;
    int initialized;
    int in_callback;
    int exiting;
};

struct netdev_pkt {
    struct list_head list;
    char data[BUFLEN];
    int len;
    int payload_hash;
    int payload_len;
    struct sockaddr_in sin;
};


void MarkInNetDevCallback();
void ClearInNetDevCallback();
int GetStackTracerID();
void MarkNetDevExiting();
int IsNetDevInCallback();
void SetNetDevCurrTsliceQuantaUs(long quantaUs);
void SetNetDevStackID(int tracerID, int stackID, float modelledNicSpeedMbps);
void NetDevInit(uint32_t src_ip_addr);
int NetDevTransmit(struct sk_buff *skb, struct sockaddr_in * skaddr,
    int payload_hash, int payload_len);
void NetDevRxLoop();
void MarkNetworkStackActive(uint32_t src_ip_addr);
void MarkNetworkStackInactive();

void * StackThread(void *arg);

#endif
