
#include "socket_utils.h"
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>


#define SUCCESS 1
#define FAIL 0

#define ETHER_TYPE_IP    (0x0800)
#define ETHER_TYPE_ARP   (0x0806)
#define ETHER_TYPE_8021Q (0x8100)
#define ETHER_TYPE_IPV6  (0x86DD)


int get_payload_hash(const void * buf, int total_len) {

    int hash = 0;
    char * payload;
    int i = 0;
    int size = total_len;
    struct iphdr* ip_header;
    struct tcphdr * tcp_header;
    int ether_offset = 14;
    int tcphdrlen, iphdrlen;

    
    if (is_raw_packet(buf, total_len)) {
	    ip_header = (struct iphdr*)(buf + ether_offset);
	    if (ip_header->protocol == 0x11) {
		// UDP
		iphdrlen = ip_header->ihl*sizeof (uint32_t);
		payload = (char *)(buf + ether_offset + iphdrlen + sizeof(struct udphdr));
		size = total_len - (ether_offset + iphdrlen + sizeof(struct udphdr));

	    } else if (ip_header->protocol == 0x6) {
		// TCP
		iphdrlen = ip_header->ihl*sizeof (uint32_t);
		tcp_header = (struct tcphdr *)(buf + ether_offset + iphdrlen);
		tcphdrlen = sizeof (uint32_t) * tcp_header->doff;
		payload = (char *)(buf + ether_offset + iphdrlen + tcphdrlen);
		size = total_len - (ether_offset + iphdrlen + tcphdrlen);

	    } else {
		return 0;
	    }
    } else {
	payload = (char *)buf;
    }
    
    for(i = 0; i < size; i++) {
    	hash += *payload;
    	hash += (hash << 10);
    	hash ^= (hash >> 6);
        ++payload;
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    if (hash < 0)
	return -1 * hash;

    return hash;
}




int is_raw_packet(const void *buf, size_t len) {

	int ether_offset = 14;
	if (len <= ether_offset + sizeof (struct iphdr))
		return FAIL;

	struct ether_header* eth= (struct ether_header *) buf;
	struct iphdr* ip_header = (struct iphdr *)(buf + ether_offset);

	if (eth->ether_type == ETHER_TYPE_IP
	    && ip_header->version == 0x4)
		return SUCCESS;

	return FAIL;

}
