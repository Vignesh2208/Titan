#ifndef __VT_UTILS_H
#define __VT_UTILS_H

#include <linux/string.h>
#include "vt_module.h"


//! Returns the first int value present in the string buffer (until a non)
// numeric character is reached
int GetNextValue (char *write_buffer);

//! Converts a string to int
int atoi(char *s);

//! Returns task_struct for a task with the specified pid
struct task_struct* FindTaskByPid(unsigned int pid);

//! Initializes a newly created tracer
/*!
    \param new_tracer An uninitialized tracer 
    \param tracer_id Id to assign to the tracer
*/
void InitializeTracerEntry(tracer * new_tracer, uint32_t tracer_id);

//! Allots and initializes memory for a new tracer with the specified ID
tracer * AllocTracerEntry(uint32_t tracer_id);

//! Frees memory associated with the specified tracer
void FreeTracerEntry(tracer * tracer_entry);

//! Constraints the task and all its children to the specified CPU
void SetChildrenCpu(struct task_struct *aTask, int cpu);

//! Sets the virtual time for all tracees of a tracer
/*!
    \param tracer_entry Represents the relevant tracer
    \param time Virtual time to set
    \param increment If set, then "time" will be added to the existing
        virtual clock of each tracee
*/
void SetChildrenTime(tracer * tracer_entry, s64 time, int increment);

//! Prints the set of tracees in a tracer's schedule queue 
void PrintScheduleList(tracer* tracer_entry);

//! Sends a signal to the specific process
/*!
    \params killTask Represents the task to send the signal to
    \params sig Represents the signal number
*/
int SendSignalToProcess(struct task_struct *killTask, int sig);

//! Acquires a thread-safe read lock which allows reading details about the relevant tracer
void GetTracerStructRead(tracer* tracer_entry);

//! Releases a thread-safe read lock which prevents reading details about the relevant tracer
void PutTracerStructRead(tracer* tracer_entry);

//! Acquires a thread-safe write lock which allows changing details of the relevant tracer
void GetTracerStructWrite(tracer* tracer_entry);

//! Releases a thread-safe write lock which prevents changing details of the relevant tracer
void PutTracerStructWrite(tracer* tracer_entry);

//! Takes in a string containing comma separated numbers and returns an array of numbers
/*!
    \param str The string to process
    \param arr An integer array of a specific size which has already been allocated
    \param arr_size Max size of the integer array
*/
int ConvertStringToArray(char * str, int * arr, int arr_size);

//! Binds the specified task to a cpu
/*!
    \param task Task to bind
    \param cpu Cpu to bind the task is bound to
*/
void BindToCpu(struct task_struct * task, int target_cpu);

#endif
