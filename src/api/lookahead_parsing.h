#ifndef __LOOKAHEAD_PARSING_H
#define __LOOKAHEAD_PARSING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON/cJSON.h"
#include "utility_functions.h"

#define MAX_OFFSET_DIGITS 10
struct lookahead_map {
    s64 * lookaheads;
    long start_offset;
    long finish_offset;
    long num_entries;
};

//! Parses a Lookahead json file at the specified path and populates a lookahead map
int ParseLookaheadJsonFile(const char * file_path, struct lookahead_map ** lmap);

#endif