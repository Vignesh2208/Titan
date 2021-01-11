#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "cache_simulation.h"

u64_t U64MAX = 0xFFFFFFFFFFFFFFFF;

extern s64 currBurstCyclesLeft;

#define BIT_MASK(end, start)  ~(U64MAX << (end - start + 1));

#define NUM_BITS(x) log2(x)

#ifndef DISABLE_INSN_CACHE_SIM

struct insnCacheParams insnCache;

void loadInsnCacheParams() {
}


//! Instruction Cache Callback function
void insCacheCallback() {
    // use the return address to find out if this block is in the cache
    //printf("Return address: %p\n", __builtin_return_address(0));
}
#endif 

#ifndef DISABLE_DATA_CACHE_SIM

struct dataCacheParams dataCache;

struct cache_line_metadata * cacheLineMeta;

struct cache_set_metadata * cacheSetMeta;


void loadDataCacheParams() {
    char * cacheType = getenv(VT_L1_DATA_CACHE_REPLACEMENT_POLICY_ENV_VARIABLE);
    if (!cacheType) dataCache.policy = DEFAULT_L1_DATA_CACHE_REPLACEMENT_POLICY;

    char * timingModel = getenv(VT_TIMING_MODEL_ENV_VARIABLE);

    assert(timingModel);

    if (strcmp(timingModel, "EMPIRICAL") == 0)
        dataCache.is_timing_model_analytic = 0;
    else
        dataCache.is_timing_model_analytic = 1;

    if (!GetIntEnvVariable(VT_L1_DATA_CACHE_LINES_ENV_VARIABLE,
        &(dataCache.lines_size_bytes)))
        dataCache.lines_size_bytes = DEFAULT_L1_DATA_CACHE_LINE_SIZE_BYTES;
    
    if (!GetIntEnvVariable(VT_L1_DATA_CACHE_SIZE_ENV_VARIABLE,
        &(dataCache.size_kb)))
        dataCache.size_kb = DEFAULT_L1_DATA_CACHE_SIZE_KB;
    
    if (!GetIntEnvVariable(VT_L1_DATA_CACHE_MISS_CYCLES_ENV_VARIABLE,
        &(dataCache.miss_cycles)))
        dataCache.miss_cycles = DEFAULT_L1_DATA_CACHE_MISS_CYCLES;

    if (!GetIntEnvVariable(VT_L1_DATA_CACHE_ASSOC_ENV_VARIABLE,
        &(dataCache.associativity)))
        dataCache.associativity = DEFAULT_L1_DATA_CACHE_ASSOCIATIVITY;

    dataCache.num_lines = (dataCache.size_kb * 1024) / dataCache.lines_size_bytes;
    dataCache.num_sets = dataCache.num_lines / dataCache.associativity;

    cacheLineMeta = (struct cache_line_metadata *)malloc(
        sizeof(struct cache_line_metadata) * dataCache.num_lines);

    cacheSetMeta = (struct cache_set_metadata *)malloc(
        sizeof(struct cache_set_metadata) * dataCache.num_sets);

    int num_offset_bits = NUM_BITS(dataCache.lines_size_bytes);
    int num_set_bits = NUM_BITS(dataCache.num_sets);

    dataCache.offset_start_bit = 0;
    dataCache.set_start_bit = num_offset_bits;
    dataCache.tag_start_bit = num_offset_bits + num_set_bits;

    dataCache.offset_mask = BIT_MASK(num_offset_bits - 1, 0);
    dataCache.set_mask = BIT_MASK(num_offset_bits + num_set_bits - 1, num_offset_bits);
    dataCache.tag_mask = BIT_MASK(63, dataCache.tag_start_bit);

    dataCache.offset_mask = dataCache.offset_mask << dataCache.offset_start_bit;
    dataCache.set_mask = dataCache.set_mask << dataCache.set_start_bit;
    dataCache.tag_mask = dataCache.tag_mask << dataCache.tag_start_bit;

    dataCache.num_hits = 0;
    dataCache.num_accesses = 0;
    dataCache.num_misses = 0;

    dataCache.initialization_status = 1;

    flushDataCache();
    printDataCacheStats();

}



//! Invoked before read accesses to memory. The read memory address is passed
// as argument. The size of read memory in bytes is also provided. This
// function is invoked before instructions which read bytes, words, or
// double words from memory
void dataReadCacheCallback(u64_t address, int size_bytes) {
    //printf("DataReadCacheCallback: address: %p, size: %d\n", (void *)address,
    //       size_bytes);

    if (dataCache.initialization_status != 1)
        return;

    int set_idx = (address & dataCache.set_mask) >> dataCache.set_start_bit;
    int line_idx = set_idx * dataCache.associativity;
    s64 tag = (address & dataCache.tag_mask);
    int lru_idx = line_idx;
    dataCache.num_accesses ++;

    if (dataCache.associativity == 1) {
        if (cacheLineMeta[line_idx].tag == tag &&
            cacheLineMeta[line_idx].valid) {
            return;
        }
        cacheLineMeta[line_idx].tag = tag;
        cacheLineMeta[line_idx].valid = 1;
        dataCache.num_misses ++;
        if (dataCache.is_timing_model_analytic) {
            currBurstCyclesLeft -= dataCache.miss_cycles;
        }
        return;
    }

    if (dataCache.associativity == 2) {
        if (cacheLineMeta[line_idx].tag == tag &&
            cacheLineMeta[line_idx].valid) {
            cacheLineMeta[line_idx].access_time = dataCache.num_accesses;
            return;
        }

        if (cacheLineMeta[line_idx + 1].tag == tag &&
            cacheLineMeta[line_idx + 1].valid) {
            cacheLineMeta[line_idx + 1].access_time = dataCache.num_accesses;
            return;
        }

        // its a miss
        

        if (cacheLineMeta[line_idx + 1].access_time <
            cacheLineMeta[lru_idx].access_time)
            lru_idx = line_idx + 1;

        cacheLineMeta[lru_idx].tag = tag;
        cacheLineMeta[lru_idx].valid = 1;
        cacheLineMeta[lru_idx].access_time = dataCache.num_accesses;
        dataCache.num_misses ++;
        if (dataCache.is_timing_model_analytic) {
            currBurstCyclesLeft -= dataCache.miss_cycles;
        }
        return;
    }

    if (dataCache.associativity % 4 == 0) {
        int i = line_idx;

        // loop unrolling makes it a bit faster
        do {
            if (cacheLineMeta[i].tag == tag &&
                cacheLineMeta[i].valid) {
                cacheLineMeta[i].access_time = dataCache.num_accesses;
                return;
            }
            if (cacheLineMeta[i + 1].tag == tag &&
                cacheLineMeta[i + 1].valid) {
                cacheLineMeta[i + 1].access_time = dataCache.num_accesses;
                return;
            }
            if (cacheLineMeta[i + 2].tag == tag &&
                cacheLineMeta[i + 2].valid) {
                cacheLineMeta[i + 2].access_time = dataCache.num_accesses;
                return;
            }
            if (cacheLineMeta[i + 3].tag == tag &&
                cacheLineMeta[i + 3].valid) {
                cacheLineMeta[i + 3].access_time = dataCache.num_accesses;
                return;
            }
            i += 4;

        } while (i < dataCache.associativity + line_idx);

        // its a miss
        goto miss;
    }

    // handle other associativities here
    for (int i = line_idx; i < dataCache.associativity + line_idx; i++) {
        if (cacheLineMeta[i].tag == tag &&
            cacheLineMeta[i].valid) {
            cacheLineMeta[i].access_time = dataCache.num_accesses;
            return;
        }
    }

    miss:
    for (int i = line_idx + 1; i < dataCache.associativity + line_idx; i++) {
        if (cacheLineMeta[i].access_time < cacheLineMeta[lru_idx].access_time)
            lru_idx = i;
    }

    cacheLineMeta[lru_idx].tag = tag;
    cacheLineMeta[lru_idx].valid = 1;
    cacheLineMeta[lru_idx].access_time = dataCache.num_accesses;
    dataCache.num_misses ++;

    if (dataCache.is_timing_model_analytic) {
        currBurstCyclesLeft -= dataCache.miss_cycles;
    }


}

//! Invoked before write accesses to memory. The write memory address is passed
// as argument. The size of written memory in bytes is also provided. This
// function is invoked before instructions which write to bytes, words, or
// double words to memory
void dataWriteCacheCallback(u64_t address, int size_bytes) {
    //printf("DataWriteCacheCallback: address: %p, size: %d\n", (void *)address,
    //    size_bytes);

    // TODO: Handle dirty writes. For now, identical to dataReadCacheCallback.
    if (dataCache.initialization_status != 1)
        return;

    int set_idx = (address & dataCache.set_mask) >> dataCache.set_start_bit;
    int line_idx = set_idx * dataCache.associativity;
    int lru_idx = line_idx;
    s64 tag = (address & dataCache.tag_mask);
    dataCache.num_accesses ++;

    if (dataCache.associativity == 1) {
        if (cacheLineMeta[line_idx].tag == tag &&
            cacheLineMeta[line_idx].valid) {
            return;
        }
        cacheLineMeta[line_idx].tag = tag;
        cacheLineMeta[line_idx].valid = 1;
        dataCache.num_misses ++;
        if (dataCache.is_timing_model_analytic) {
            currBurstCyclesLeft -= dataCache.miss_cycles;
        }
        return;
    }

    if (dataCache.associativity == 2) {
        if (cacheLineMeta[line_idx].tag == tag &&
            cacheLineMeta[line_idx].valid) {
            cacheLineMeta[line_idx].access_time = dataCache.num_accesses;
            return;
        }

        if (cacheLineMeta[line_idx + 1].tag == tag &&
            cacheLineMeta[line_idx + 1].valid) {
            cacheLineMeta[line_idx + 1].access_time = dataCache.num_accesses;
            return;
        }

        // its a miss

        if (cacheLineMeta[line_idx + 1].access_time <
            cacheLineMeta[lru_idx].access_time)
            lru_idx = line_idx + 1;

        cacheLineMeta[lru_idx].tag = tag;
        cacheLineMeta[lru_idx].valid = 1;
        cacheLineMeta[lru_idx].access_time = dataCache.num_accesses;
        dataCache.num_misses ++;
        if (dataCache.is_timing_model_analytic) {
            currBurstCyclesLeft -= dataCache.miss_cycles;
        }
        return;
    }

    if (dataCache.associativity % 4 == 0) {
        int i = line_idx;

        // loop unrolling makes it a bit faster
        do {
            if (cacheLineMeta[i].tag == tag &&
                cacheLineMeta[i].valid) {
                cacheLineMeta[i].access_time = dataCache.num_accesses;
                return;
            }
            if (cacheLineMeta[i + 1].tag == tag &&
                cacheLineMeta[i + 1].valid) {
                cacheLineMeta[i + 1].access_time = dataCache.num_accesses;
                return;
            }
            if (cacheLineMeta[i + 2].tag == tag &&
                cacheLineMeta[i + 2].valid) {
                cacheLineMeta[i + 2].access_time = dataCache.num_accesses;
                return;
            }
            if (cacheLineMeta[i + 3].tag == tag &&
                cacheLineMeta[i + 3].valid) {
                cacheLineMeta[i + 3].access_time = dataCache.num_accesses;
                return;
            }
            i += 4;

        } while (i < dataCache.associativity + line_idx);

        // its a miss
        goto miss;
    }

    // handle other associativities here
    for (int i = line_idx; i < dataCache.associativity + line_idx; i++) {
        if (cacheLineMeta[i].tag == tag &&
            cacheLineMeta[i].valid) {
            cacheLineMeta[i].access_time = dataCache.num_accesses;
            return;
        }
    }

    miss:
    for (int i = line_idx + 1; i < dataCache.associativity + line_idx; i++) {
        if (cacheLineMeta[i].access_time < cacheLineMeta[lru_idx].access_time)
            lru_idx = i;
    }

    cacheLineMeta[lru_idx].tag = tag;
    cacheLineMeta[lru_idx].valid = 1;
    cacheLineMeta[lru_idx].access_time = dataCache.num_accesses;
    dataCache.num_misses ++;

    if (dataCache.is_timing_model_analytic) {
        currBurstCyclesLeft -= dataCache.miss_cycles;
    }

}

/*** Invoked on context-switch ***/
void flushDataCache() {
    int i;
    if (dataCache.initialization_status != 1)
        return;

    printf ("Flushed Data Cache !\n");
    fflush(stdout);

    for (i = 0; i < dataCache.num_lines; i++ ) {
        cacheLineMeta[i].access_time = 0;
        cacheLineMeta[i].tag = 0;
        cacheLineMeta[i].valid = 0;
        cacheLineMeta[i].dirty = 0;
    }

    for(i = 0; i < dataCache.num_sets; i++) {
        cacheSetMeta[i].lru_idx = 0;
    }
}

/*** Frees dynamically allocated memory ***/
void cleanupDataCache(void) {
    if (dataCache.initialization_status != 1)
        return;

    free(cacheSetMeta);
    free(cacheLineMeta);
    dataCache.initialization_status = 0;    
}

/*** Prints some stats ***/
void printDataCacheStats(void) {

    if (dataCache.initialization_status != 1)
        return;
    dataCache.num_hits = dataCache.num_accesses - dataCache.num_misses;

    printf ("\n******************* Data Cache Stats *******************\n");
    printf ("Size (kb)                   :       %d\n", dataCache.size_kb);
    printf ("Lines size (bytes)          :       %d\n", dataCache.lines_size_bytes);
    printf ("Associativity               :       %d\n", dataCache.associativity);
    printf ("Miss cycles                 :       %d\n", dataCache.miss_cycles);
    printf ("Num lines                   :       %d\n", dataCache.num_lines);
    printf ("Num sets                    :       %d\n", dataCache.num_sets);
    printf ("Offset start                :       %d\n", dataCache.offset_start_bit);
    printf ("Set start                   :       %d\n", dataCache.set_start_bit);
    printf ("Tag start                   :       %d\n", dataCache.tag_start_bit);
    printf ("Num hits                    :       %lld\n", dataCache.num_hits);
    printf ("Num misses                  :       %lld\n", dataCache.num_misses);
    printf ("Num accesses                :       %lld\n", dataCache.num_accesses);

    if (dataCache.num_accesses) {
        printf ("Miss rate                   :       %f\n", (float)dataCache.num_misses/(float)dataCache.num_accesses);
        printf ("Hit rate                    :       %f\n", (float)dataCache.num_hits/(float)dataCache.num_accesses);
    }

    if (!dataCache.is_timing_model_analytic) {
        printf ("Timing Model                :       EMPIRICAL\n");
    } else {
        printf ("Timing Model                :       ANALYTICAL\n");
    }
}


/*** Returns the current cache-hit rate ***/
float getDataCacheHitRate(void) {

    if (dataCache.initialization_status != 1)
        return 0.0;

    dataCache.num_hits = dataCache.num_accesses - dataCache.num_misses;
    if (dataCache.num_accesses) {
        return (float)dataCache.num_hits / (float)dataCache.num_accesses;
    }
    return 0.0;
}


#endif
