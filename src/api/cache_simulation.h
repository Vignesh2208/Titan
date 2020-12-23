#ifndef __CACHE_SIMULATION_H
#define __CACHE_SIMULATION_H
#include "utility_functions.h"

#define VT_TIMING_MODEL_ENV_VARIABLE "VT_TIMING_MODEL"

//! Cache replacement policies
typedef enum {
    RANDOM = 1,
    LRU = 2,
    UNKNOWN = 3
} CACHE_REPLACEMENT_POLICY;

struct cache_line_metadata {
    int valid;
    int dirty;
    s64 tag;
    s64 access_time;
};

struct cache_set_metadata {
    int lru_idx;
};

#ifndef DISABLE_INSN_CACHE_SIM


//! Instruction cache params
struct insnCacheParams {
    int size_kb;
    int associativity;
    int lines_size_bytes;
    int miss_cycles;
    int num_lines;
    int num_sets;
    int offset_start_bit;
    int set_start_bit;
    int tag_start_bit;
    int initialization_status;
    int is_timing_model_analytic;
    s64 num_hits;
    s64 num_misses;
    s64 num_accesses;
    s64 set_mask;
    s64 offset_mask;
    s64 tag_mask;
    CACHE_REPLACEMENT_POLICY policy;
};



#define VT_L1_INS_CACHE_ASSOCIATIVITY "VT_L1_INS_CACHE_ASSOCIATIVITY"
#define VT_L1_INS_CACHE_SIZE_ENV_VARIABLE "VT_L1_INS_CACHE_SIZE_KB"
#define VT_L1_INS_CACHE_LINES_ENV_VARIABLE "VT_L1_INS_CACHE_LINES"
#define VT_L1_INS_CACHE_REPLACEMENT_POLICY_ENV_VARIABLE "VT_L1_INS_CACHE_REPLACEMENT_POLICY"
#define VT_L1_INS_CACHE_MISS_CYCLES_ENV_VARIABLE "VT_L1_INS_CACHE_MISS_CYCLES"
#define VT_L1_INS_CACHE_ASSOC_ENV_VARIABLE "VT_L1_INS_CACHE_ASSOC"
#define DEFAULT_L1_INS_CACHE_SIZE_KB 32
#define DEFAULT_L1_INS_CACHE_LINE_SIZE_BYTES 64
#define DEFAULT_L1_INS_CACHE_REPLACEMENT_POLICY LRU
#define DEFAULT_L1_INS_CACHE_MISS_CYCLES 100
#define DEFAULT_L1_INS_CACHE_ASSOCIATIVITY 2

/*** Read environment variables and populate cache params ***/
void loadInsnCacheParams(void);

/*** InsnCache callback ***/
void insCacheCallback(void);

/*** Invoked on context-switch ***/
void flushInsCache(void);

/*** Frees dynamically allocated memory ***/
void cleanupInsCache(void);


/*** Prints some stats ***/
void printInsnCacheStats(void);


/*** Returns the current cache-hit rate ***/
float getInsnCacheHitRate(void);

#endif

#ifndef DISABLE_DATA_CACHE_SIM



//! Data cache params
struct dataCacheParams {
    int size_kb;
    int associativity;
    int lines_size_bytes;
    int miss_cycles;
    int num_lines;
    int num_sets;
    int offset_start_bit;
    int set_start_bit;
    int tag_start_bit;
    int initialization_status;
    int is_timing_model_analytic;
    s64 num_hits;
    s64 num_misses;
    s64 num_accesses;
    s64 set_mask;
    s64 offset_mask;
    s64 tag_mask;
    CACHE_REPLACEMENT_POLICY policy;
};



#define VT_L1_DATA_CACHE_ASSOCIATIVITY "VT_L1_DATA_CACHE_ASSOCIATIVITY"
#define VT_L1_DATA_CACHE_SIZE_ENV_VARIABLE "VT_L1_DATA_CACHE_SIZE_KB"
#define VT_L1_DATA_CACHE_LINES_ENV_VARIABLE "VT_L1_DATA_CACHE_LINES"
#define VT_L1_DATA_CACHE_REPLACEMENT_POLICY_ENV_VARIABLE "VT_L1_DATA_CACHE_REPLACEMENT_POLICY"
#define VT_L1_DATA_CACHE_MISS_CYCLES_ENV_VARIABLE "VT_L1_DATA_CACHE_MISS_CYCLES"
#define VT_L1_DATA_CACHE_ASSOC_ENV_VARIABLE "VT_L1_DATA_CACHE_ASSOC"
#define DEFAULT_L1_DATA_CACHE_SIZE_KB 32
#define DEFAULT_L1_DATA_CACHE_LINE_SIZE_BYTES 64
#define DEFAULT_L1_DATA_CACHE_REPLACEMENT_POLICY LRU
#define DEFAULT_L1_DATA_CACHE_MISS_CYCLES 100
#define DEFAULT_L1_DATA_CACHE_ASSOCIATIVITY 2

/*** Read environment variables and populate cache params ***/
void loadDataCacheParams(void);

/*** Invoked before read accesses to memory ***/
void dataReadCacheCallback(u64_t address, int size_bytes);

/*** Invoked before write accesses to memory ***/
void dataWriteCacheCallback(u64_t address, int size_bytes);

/*** Invoked on context-switch ***/
void flushDataCache(void);

/*** Frees dynamically allocated memory ***/
void cleanupDataCache(void);

/*** Prints some stats ***/
void printDataCacheStats(void);


/*** Returns the current cache-hit rate ***/
float getDataCacheHitRate(void);


#endif

#endif