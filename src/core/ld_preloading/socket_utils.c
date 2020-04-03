
#include "socket_utils.h"


int get_packet_hash(const char * pkt, int size) {

    int hash = 0;
    const char * ptr = pkt;
    int i = 0;
    
    for(i = 0; i < size; i++) {
    	hash += *ptr;
    	hash += (hash << 10);
    	hash ^= (hash >> 6);

        ++ptr;
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);


    if (hash < 0)
	return -1 * hash;

    return hash;
}


/*
int is_raw_packet(const void *buf, size_t len) {

	if (len <= sizeof (struct iphdr))
		return FAIL;

	struct iphdr* ip_header = (struct iphdr *)buf;

	if (ip_header->version == 0x4)
		return SUCCESS;

	return FAIL;

}*/
