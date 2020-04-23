#include <stdio.h>
#include <sys/time.h>


int get_payload_hash(const void * pkt, int size);
int is_raw_packet(const void *buf, size_t len);
