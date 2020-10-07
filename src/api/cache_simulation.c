#include <stdio.h>
#include <stdlib.h>
#include "cache_simulation.h"

#ifndef DISABLE_INSN_CACHE_SIM

struct insnCacheParams insnCache;

void loadInsnCacheParams() {
    char * cacheType = getenv(VT_INS_CACHE_TYPE_ENV_VARIABLE);
    if (!cacheType) insnCache.type = DEFAULT_INS_CACHE_TYPE;

    if (!GetIntEnvVariable(VT_INS_CACHE_LINES_ENV_VARIABLE,
        &(insnCache.num_lines)))
        insnCache.num_lines = DEFAULT_INS_CACHE_LINES;
    
    if (!GetIntEnvVariable(VT_INS_CACHE_SIZE_ENV_VARIABLE,
        &(insnCache.size_kb)))
        insnCache.size_kb = DEFAULT_INS_CACHE_SIZE_KB;
    
    if (!GetIntEnvVariable(VT_INS_CACHE_MISS_CYCLES_ENV_VARIABLE,
        &(insnCache.miss_cycles)))
        insnCache.miss_cycles = DEFAULT_INS_CACHE_MISS_CYCLES;
    
}


//! Instruction Cache Callback function
void insCacheCallback() {
    // use the return address to find out if this block is in the cache
    //printf("Return address: %p\n", __builtin_return_address(0));
}
#endif 

#ifndef DISABLE_DATA_CACHE_SIM

struct dataCacheParams dataCache;


void loadDataCacheParams() {
    char * cacheType = getenv(VT_INS_CACHE_TYPE_ENV_VARIABLE);
    if (!cacheType) dataCache.type = DEFAULT_DATA_CACHE_TYPE;

    if (!GetIntEnvVariable(VT_DATA_CACHE_LINES_ENV_VARIABLE,
        &(dataCache.num_lines)))
        dataCache.num_lines = DEFAULT_DATA_CACHE_LINES;
    
    if (!GetIntEnvVariable(VT_DATA_CACHE_SIZE_ENV_VARIABLE,
        &(dataCache.size_kb)))
        dataCache.size_kb = DEFAULT_DATA_CACHE_SIZE_KB;
    
    if (!GetIntEnvVariable(VT_DATA_CACHE_MISS_CYCLES_ENV_VARIABLE,
        &(dataCache.miss_cycles)))
        dataCache.miss_cycles = DEFAULT_DATA_CACHE_MISS_CYCLES;
}

//! Invoked before read accesses to memory. The read memory address is passed
// as argument. The size of read memory in bytes is also provided. This
// function is invoked before instructions which read bytes, words, or
// double words from memory
void dataReadCacheCallback(u64_t address, int size_bytes) {
    //printf("DataReadCacheCallback: address: %p, size: %d\n", (void *)address,
    //       size_bytes);
}

//! Invoked before write accesses to memory. The write memory address is passed
// as argument. The size of written memory in bytes is also provided. This
// function is invoked before instructions which write to bytes, words, or
// double words to memory
void dataWriteCacheCallback(u64_t address, int size_bytes) {
    //printf("DataWriteCacheCallback: address: %p, size: %d\n", (void *)address,
    //    size_bytes);
}
#endif