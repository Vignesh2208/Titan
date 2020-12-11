#include <stdio.h>
#include <sys/time.h>

//! Returns a integer hash associated with a packet represented as a string
/*!
    \param pkt A string of specific size representing a packet
    \param size Size of the packet in bytes
*/
int GetPacketHash(const void * pkt, int size);

//! Returns a positive value if the string is a raw packet (containing Ethernet + IP headers)
int IsRawPacket(const void *buf, size_t len);
