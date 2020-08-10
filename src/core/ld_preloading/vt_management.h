#ifndef __VT_MANAGEMENT_H
#define __VT_MANAGEMENT_H

#include "VT_functions.h"

//! Creates pointers to all virtual time related functions which need to be
//  accessible from the intercept library
void LoadAllVtlFunctions(void * lib_vt_lib_handle);

//! Initializes virtual time related logic for the intercept library 
void InitializeVtlLogic();

#endif