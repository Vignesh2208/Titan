#ifndef __CACHE_SIMULATION_H
#define __CACHE_SIMULATION_H
#include "utility_functions.h"

#ifndef DISABLE_INSN_CACHE_SIM

//! Instruction cache types
typedef enum {
    INSN_CACHE_TYPE_DMA = 1,
    INSN_CACHE_TYPE_LRU = 2,
    INSN_CACHE_TYPE_UNKNOWN = 3
} INSN_CACHE_TYPE;

//! Instruction cache params
struct insnCacheParams {
    int size_kb;
    int num_lines;
    int miss_cycles;
    INSN_CACHE_TYPE type;
};


#define VT_INS_CACHE_SIZE_ENV_VARIABLE "VT_INS_CACHE_SIZE_KB"
#define VT_INS_CACHE_LINES_ENV_VARIABLE "VT_INS_CACHE_LINES"
#define VT_INS_CACHE_TYPE_ENV_VARIABLE "VT_INS_CACHE_TYPE"
#define VT_INS_CACHE_MISS_CYCLES_ENV_VARIABLE "VT_INS_CACHE_MISS_CYCLES"
#define DEFAULT_INS_CACHE_SIZE_KB 32
#define DEFAULT_INS_CACHE_LINES 32
#define DEFAULT_INS_CACHE_TYPE INSN_CACHE_TYPE_DMA
#define DEFAULT_INS_CACHE_MISS_CYCLES 100

/*** Read environment variables and populate cache params ***/
void loadInsnCacheParams(void);

/*** InsnCache callback ***/
void insCacheCallback(void);
#endif

#ifndef DISABLE_DATA_CACHE_SIM

//! Data cache types
typedef enum {
    DATA_CACHE_TYPE_DMA = 1,
    DATA_CACHE_TYPE_LRU = 2,
    DATA_CACHE_TYPE_UNKNOWN = 3
} DATA_CACHE_TYPE;

//! Data cache params
struct dataCacheParams {
    int size_kb;
    int num_lines;
    int miss_cycles;
    DATA_CACHE_TYPE type;
};

#define VT_DATA_CACHE_SIZE_ENV_VARIABLE "VT_DATA_CACHE_SIZE_KB"
#define VT_DATA_CACHE_LINES_ENV_VARIABLE "VT_DATA_CACHE_LINES"
#define VT_DATA_CACHE_TYPE_ENV_VARIABLE "VT_DATA_CACHE_TYPE"
#define VT_DATA_CACHE_MISS_CYCLES_ENV_VARIABLE "VT_DATA_CACHE_MISS_CYCLES"
#define DEFAULT_DATA_CACHE_SIZE_KB 32
#define DEFAULT_DATA_CACHE_LINES 32
#define DEFAULT_DATA_CACHE_TYPE DATA_CACHE_TYPE_DMA
#define DEFAULT_DATA_CACHE_MISS_CYCLES 100

/*** Read environment variables and populate cache params ***/
void loadDataCacheParams(void);

/*** Invoked before read accesses to memory ***/
void dataReadCacheCallback(u64_t address, int size_bytes);

/*** Invoked before write accesses to memory ***/
void dataWriteCacheCallback(u64_t address, int size_bytes);


#endif

#endif