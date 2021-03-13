#include "syshead.h"
#include "utils.h"
#include "netdev.h"
#include "socket.h"
#include "../vtl_logic.h"
#include "../VT_functions.h"
#include <unistd.h>
#include <sys/syscall.h> 

#define LOOKAHEAD_ANCHOR_NONE 0
#define LOOKAHEAD_ANCHOR_CURR_TIME 1
#define LOOKAHEAD_ANCHOR_EAT 2

void RegisterSysCallWait() {
    int ThreadId = syscall(SYS_gettid);
    s64 ret;
    #ifndef DISABLE_LOOKAHEAD
    /*** For setting lookaheads ***/
    SetLookahead(0, LOOKAHEAD_ANCHOR_EAT);
    #endif

    ret = TriggerSyscallWait(ThreadId, 0);
    long currQuantaUs = (long)(ret / NSEC_PER_US);
    if (!currQuantaUs)
        currQuantaUs = 1;
    SetNetDevCurrTsliceQuantaUs(currQuantaUs);
    NetDevRxLoop();
}


s64 RegisterStackThreadWait(int tracerID, int stackID) {
    return TriggerStackThreadWait(tracerID, stackID);
}

int RunCmd(char *cmd, ...) {
    va_list ap;
    char buf[CMDBUFLEN];
    va_start(ap, cmd);
    vsnprintf(buf, CMDBUFLEN, cmd, ap);

    va_end(ap);
    printf("EXEC: %s\n", buf);
    return system(buf);
}

uint32_t SumEvery16Bits(void *addr, int count) {
    register uint32_t sum = 0;
    uint16_t * ptr = addr;
    
    while( count > 1 )  {
        /*  This is the inner loop */
        sum += * ptr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if( count > 0 )
        sum += * (uint8_t *) ptr;

    return sum;
}

uint16_t checksum(void *addr, int count, int start_sum) {
    /* Compute Internet Checksum for "count" bytes
     *         beginning at location "addr".
     * Taken from https://tools.ietf.org/html/rfc1071
     */
    uint32_t sum = start_sum;

    sum += SumEvery16Bits(addr, count);
    
    /*  Fold 32-bit sum to 16 bits */
    while (sum>>16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}

int GetAddress(char *host, char *port, struct sockaddr *addr) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    s = getaddrinfo(host, port, &hints, &result);

    if (s != 0) {
        print_err("getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        *addr = *rp->ai_addr;
        freeaddrinfo(result);
        return 0;
    }
    
    return 1;
}

uint32_t ParseIPV4String(char* addr) {
    uint8_t addr_bytes[4];
    sscanf(addr, "%hhu.%hhu.%hhu.%hhu", &addr_bytes[3], &addr_bytes[2], &addr_bytes[1], &addr_bytes[0]);
    return addr_bytes[0] | addr_bytes[1] << 8 | addr_bytes[2] << 16 | addr_bytes[3] << 24;
}

uint32_t min(uint32_t x, uint32_t y) {
    return x > y ? y : x;
}
