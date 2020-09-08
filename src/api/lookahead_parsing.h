#ifndef __LOOKAHEAD_PARSING_H
#define __LOOKAHEAD_PARSING_H

#ifndef DISABLE_LOOKAHEAD
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h> 
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "utility_functions.h"

//! Parses a Lookahead file at the specified path and populates a lookahead map
int LoadLookahead(const char * file_path, struct lookahead_info * linfo);

//! Clean's up a loaded lookahead map. Called before a process exits.
void CleanLookaheadInfo(struct lookahead_info * linfo);

#endif
#endif