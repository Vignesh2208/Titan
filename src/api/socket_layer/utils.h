#ifndef UTILS_H
#define UTILS_H

#include "../vtl_logic.h"


#define CMDBUFLEN 100

#ifdef DEBUG
#define print_debug(str, ...)                       \
    printf(str" - %s:%u\n", ##__VA_ARGS__, __FILE__, __LINE__);
#else
#define print_debug(str, ...) ;
#endif

#define print_err(str, ...)                     \
    fprintf(stderr, str, ##__VA_ARGS__);

int RunCmd(char *cmd, ...);
uint32_t SumEvery16Bits(void *addr, int count);
uint16_t checksum(void *addr, int count, int start_sum);
int GetAddress(char *host, char *port, struct sockaddr *addr);
uint32_t ParseIPV4String(char *addr);
uint32_t min(uint32_t x, uint32_t y);


void RegisterSysCallWait();
int RegisterStackThreadWait(int tracerID, int stackID);

#endif
