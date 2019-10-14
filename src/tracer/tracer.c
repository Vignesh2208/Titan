#include "tracer.h"
#include <sys/sysinfo.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <sys/file.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <sched.h>
#include <getopt.h>
#include <string.h>
#include <sched.h>
#include <signal.h>

#define TK_IOC_MAGIC  'k'
#define TK_IO_GET_STATS _IOW(TK_IOC_MAGIC,  1, int)
#define TK_IO_WRITE_RESULTS _IOW(TK_IOC_MAGIC,  2, int)
#define TK_IO_RESUME_BLOCKED_SYSCALLS _IOW(TK_IOC_MAGIC,  3, int)
#define PTRACE_SET_DELTA_BUFFER_WINDOW 0x42f4
#define TRACER_RESULTS 'J'

#define BUFFER_WINDOW_SIZE 50


#ifdef TEST
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;

#endif


struct sched_param param;
char logFile[MAX_FNAME_SIZ];

int llist_elem_comparer(tracee_entry * elem1, tracee_entry * elem2) {

	if (elem1->pid == elem2->pid)
		return 0;
	return 1;

}


void printLog(const char *fmt, ...) {
#ifdef DEBUG
	FILE* pFile = fopen(logFile, "a");
	if (pFile != NULL) {
		va_list args;
		va_start(args, fmt);
		vfprintf(pFile, fmt, args);
		va_end(args);
		fclose(pFile);
	}
#else

	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	fflush(stdout);
	va_end(args);


#endif
}

tracee_entry * alloc_new_tracee_entry(pid_t pid) {

	tracee_entry * tracee;


	tracee = (tracee_entry *) malloc(sizeof(tracee_entry));
	if (!tracee) {
		LOG("MALLOC ERROR !. Exiting for now\n");
		exit(FAIL);
	}

	tracee->pid = pid;
	tracee->vfork_stop = 0;
	tracee->syscall_blocked = 0;
	tracee->vfork_parent = NULL;
	tracee->n_insns_share = 0;
	tracee->n_insns_left = 0;
	tracee->vrun_time = 0;
	tracee->n_preempts = 0;
	tracee->n_sleeps = 0;
	tracee->pd = NULL;

	return tracee;

}

tracee_entry * get_min_vrun_time_tracee(llist * run_queue) {
	llist_elem * head = run_queue->head;
	tracee_entry * tracee;
	tracee_entry * min_vrun_time_tracee = NULL;
	u32 min_vrun_time = 100000000;
	while (head != NULL) {
		tracee = (tracee_entry *) head->item;
		if (tracee && tracee->vrun_time < min_vrun_time) {
			min_vrun_time = tracee->vrun_time;
			min_vrun_time_tracee = tracee;
		}
		head = head->next;
	}
	return min_vrun_time_tracee;
}

void setup_all_traces(hashmap * tracees, llist * tracee_list, llist * run_queue) {

	llist_elem * head = tracee_list->head;
	tracee_entry * tracee;
	int ret = 0;
	unsigned long status = 0;

	while (head != NULL) {
		tracee = (tracee_entry *) head->item;
		do {
			ret = waitpid(tracee->pid, &status, 0);
		} while ((ret == (pid_t) - 1 && errno == EINTR ));

		if (errno == ESRCH) {
			LOG("Child %d is dead. Removing from active tids ...\n", tracee->pid);
			hmap_remove_abs(tracees, tracee->pid);
			llist_remove(tracee_list, tracee);

		} else {
			if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
				LOG("Child %d stopped. Setting trace options \n", tracee->pid);
				ptrace(PTRACE_SETOPTIONS, tracee->pid, NULL, PTRACE_O_EXITKILL |
				       PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT |
				       PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
				       PTRACE_O_TRACEEXEC);

				//ptrace(PTRACE_DETACH, tracee->pid, 0, SIGSTOP);
				if (errno == ESRCH)
					LOG("Error setting ptrace options\n");
			}
		}

		head = head->next;

	}

}


int wait_for_ptrace_events(hashmap * tracees, llist * tracee_list,
                           llist * run_queue, pid_t pid,
                           struct libperf_data * pd, int cpu_assigned) {

	llist_elem * head = tracee_list->head;
	tracee_entry * new_tracee;
	tracee_entry * curr_tracee;

#ifdef PROCESS_CFS_SCHED_MODE
	tracee_entry * min_vrun_time_tracee;
#endif

	struct user_regs_struct regs;

	int ret;
	u32 n_ints;
	u32 status = 0;
	uint64_t counter;

	char buffer[100];
	pid_t new_child_pid;
	struct libperf_data * pd_tmp;
	u32 n_insns = 1000;


	curr_tracee = hmap_get_abs(tracees, pid);
	if (curr_tracee->vfork_stop)	//currently inside a vfork stop
		return TID_FORK_EVT;

retry:
	status = 0;
	flush_buffer(buffer, 100);
	errno = 0;
	do {
		ret = waitpid(pid, &status, WTRACE_DESCENDENTS | __WALL);
	} while (ret == (pid_t) - 1 && errno == EINTR);

	if ((pid_t)ret != pid) {
		if (errno == EBREAK_SYSCALL) {
			LOG("Waitpid: Breaking out. Process entered blocking syscall\n");
			curr_tracee->syscall_blocked = 1;
			return TID_SYSCALL_ENTER;
		}
		return TID_IGNORE_PROCESS;
	}



	if (errno == ESRCH) {
		LOG("Process does not exist\n");
		return TID_IGNORE_PROCESS;
	}

	LOG("Waitpid finished for Process : %d. Status = %lX\n ", pid, status);

	if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {


		// check for different types of events and determine whether
		// this is a system call stop detected fork or clone event. get new tid.
		if (status >> 8 == (SIGTRAP | PTRACE_EVENT_CLONE << 8)
		        || status >> 8 == (SIGTRAP | PTRACE_EVENT_FORK << 8)) {
			LOG("Detected PTRACE EVENT CLONE or FORK for : %d\n", pid);
			ret = ptrace(PTRACE_GETEVENTMSG, pid, NULL, (u32*)&new_child_pid);
			status = 0;
			do {
				ret = waitpid(new_child_pid, &status, __WALL);
			} while (ret == (pid_t) - 1 && errno == EINTR);

			if (errno == ESRCH) {
				LOG("Process does not exist\n");
				return TID_IGNORE_PROCESS;
			}
			if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) {
				new_tracee = alloc_new_tracee_entry(new_child_pid);
				llist_append(tracee_list, new_tracee);
#ifdef PROCESS_RR_SCHED_MODE
				llist_append(run_queue, new_tracee);
#endif


#ifdef PROCESS_CFS_SCHED_MODE
				min_vrun_time_tracee = get_min_vrun_time_tracee(run_queue);
				assert(min_vrun_time_tracee != NULL);
				new_tracee->vrun_time = min_vrun_time_tracee->vrun_time;
				llist_append(run_queue, new_tracee);
#endif

				hmap_put_abs(tracees, new_child_pid, new_tracee);
				LOG("Detected new cloned child with tid: %d. status = %lX, "
				    "Ret = %d, errno = %d, Set trace options.\n", new_child_pid,
				    status, ret, errno);
				new_tracee->pd = libperf_initialize((int)new_child_pid, cpu_assigned);
				return TID_FORK_EVT;
			} else {
				LOG("ERROR ATTACHING TO New Thread "
				    "status = %lX, ret = %d, errno = %d\n", status, ret, errno);
				return TID_IGNORE_PROCESS;
			}
		} else if (status >> 8 == (SIGTRAP | PTRACE_EVENT_EXIT << 8)) {
			LOG("Detected process exit for : %d\n", pid);
			if (curr_tracee->vfork_parent != NULL) {
				curr_tracee->vfork_parent->vfork_stop = 0;
				curr_tracee->vfork_parent = NULL;
			}
			return TID_EXITED;
		} else if (status >> 8 == (SIGTRAP | PTRACE_EVENT_VFORK << 8)) {
			LOG("Detected PTRACE EVENT VFORK for : %d\n", pid);
			ret = ptrace(PTRACE_GETEVENTMSG, pid, NULL, (u32*)&new_child_pid);
			status = 0;
			do {
				ret = waitpid(new_child_pid, &status, __WALL);
			} while (ret == (pid_t) - 1 && errno == EINTR);

			if (errno == ESRCH) {
				return TID_IGNORE_PROCESS;
			}
			if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) {
				new_tracee = alloc_new_tracee_entry(new_child_pid);
				llist_append(tracee_list, new_tracee);
#ifdef PROCESS_RR_SCHED_MODE
				llist_append(run_queue, new_tracee);
#endif

#ifdef PROCESS_CFS_SCHED_MODE
				min_vrun_time_tracee = get_min_vrun_time_tracee(run_queue);
				assert(min_vrun_time_tracee != NULL);
				new_tracee->vrun_time = min_vrun_time_tracee->vrun_time;
				llist_append(run_queue, new_tracee);
#endif

				hmap_put_abs(tracees, new_child_pid, new_tracee);
				LOG("Detected new vforked process with pid: %d. "
				    "status = %lX, Ret = %d, errno = %d, Set trace options.\n",
				    new_child_pid, status, ret, errno);
				curr_tracee->vfork_stop = 1;
				new_tracee->vfork_parent = curr_tracee;
				new_tracee->pd = libperf_initialize((int)new_child_pid, cpu_assigned);
				return TID_FORK_EVT;
			} else {
				LOG("ERROR ATTACHING TO New Thread "
				    "status = %lX, ret = %d, errno = %d\n", status, ret, errno);
				return TID_IGNORE_PROCESS;
			}

		} else if (status >> 8 == (SIGTRAP | PTRACE_EVENT_EXEC << 8)) {
			LOG("Detected PTRACE EVENT EXEC for : %d\n", pid);
			if (curr_tracee->vfork_parent != NULL) {
				curr_tracee->vfork_parent->vfork_stop = 0;
				curr_tracee->vfork_parent = NULL;
			}

			return TID_OTHER;
		} else {


			ret = ptrace(PTRACE_GET_REM_MULTISTEP, pid, 0, (u32*)&n_ints);
//#ifdef DEBUG_VERBOSE
			ret = ptrace(PTRACE_GETREGS, pid, NULL, &regs);
			LOG("Single step completed for Process : %d. "
			    "Status = %lX. Rip: %lX\n\n ", pid, status, regs.rip);
//#endif

			if (ret == -1) {
				LOG("ERROR in GETREGS.\n");
			}
			return TID_SINGLESTEP_COMPLETE;

		}


	} else {
		if (WIFSTOPPED(status) && WSTOPSIG(status) >= SIGUSR1
		        && WSTOPSIG(status) <= SIGALRM ) {
			LOG("Received Signal: %d\n", WSTOPSIG(status));
			errno = 0;
			n_insns = 1000;
			// init lib
			libperf_ioctlrefresh(pd, LIBPERF_COUNT_HW_INSTRUCTIONS,
			                   (uint64_t )n_insns);

			// enable hardware counter
			libperf_enablecounter(pd, LIBPERF_COUNT_HW_INSTRUCTIONS);
			ret = ptrace(PTRACE_SET_REM_MULTISTEP, pid, 0, (u32*)&n_insns);

			LOG("PTRACE RESUMING process After signal. "
			    "ret = %d, error_code = %d. pid = %d\n", ret, errno, pid);
			fflush(stdout);
			ret = ptrace(PTRACE_CONT, pid, 0, WSTOPSIG(status));
			goto retry;


		} else if (WIFSTOPPED(status) && WSTOPSIG(status) != SIGCHLD) {
			LOG("Received Exit Signal: %d\n", WSTOPSIG(status));
			if (curr_tracee->vfork_parent != NULL) {
				curr_tracee->vfork_parent->vfork_stop = 0;
				curr_tracee->vfork_parent = NULL;
			}
			ret = ptrace(PTRACE_CONT, pid, 0, WSTOPSIG(status));
			return TID_EXITED;
		} else {
			LOG("Received Unknown Signal: %d\n", WSTOPSIG(status));
		}
	}


	if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGCHLD) {
		LOG("Received SIGCHLD. Process exiting...\n");
		if (curr_tracee->vfork_parent != NULL) {
			curr_tracee->vfork_parent->vfork_stop = 0;
			curr_tracee->vfork_parent = NULL;
		}
		return TID_EXITED;
	} else {
		LOG("Received Unknown Status: %d\n", WSTOPSIG(status));
	}
	return TID_OTHER;
}

int is_tracee_blocked(tracee_entry * curr_tracee) {
	u32 flags = 0;
	u32 status = 0;
	int ret;
	pid_t pid;

	if (!curr_tracee)
		return TID_NOT_MONITORED;

	pid = curr_tracee->pid;
	if (curr_tracee->syscall_blocked) {
		errno = 0;
		ret = ptrace(PTRACE_GET_MSTEP_FLAGS,
		             pid, 0, (u32*)&flags);

		if (flags == 0) {
			LOG("Waiting for unblocked tracee: %d to reach stop state\n", curr_tracee->pid); 
			errno = 0;
			do {
				ret = waitpid(pid, &status,
				              WTRACE_DESCENDENTS | __WALL);
			} while (ret == (pid_t) - 1 && errno == EINTR);

			if ((pid_t)ret != pid) {
				LOG("Error during wait\n");
				return TID_PROCESS_BLOCKED;
			}
			LOG("Tracee: %d unblocked\n", curr_tracee->pid);
			
			curr_tracee->syscall_blocked = 0;
			return FAIL;
		} else{
			LOG("Tracee : %d Still Blocked Flags: %d\n", pid, flags);
			return TID_PROCESS_BLOCKED;
		}
	}
	if (curr_tracee->vfork_stop) {
		return TID_PROCESS_BLOCKED;
	}
	return FAIL;
}


/*
 * Runs a tracee specified by its pid for a specific number of instructions
 * and returns the result of the operation.
 * Input:
 *		tracees: Hashmap of <pid, tracee_entry>
 *		tracees_list: Linked list of <tracee_entry>
 *		pid:	PID of the tracee which is to be run
 *		n_insns: Number of instructions by which the specified tracee is advanced
 *		cpu_assigned: Cpu on which the tracer is running
 *		rel_cpu_speed: Kronos specific relative cpu speed assigned to the
 		tracer. Equivalent to TDF.
 * Output:
 *		SUCCESS or FAIL
 */
int run_commanded_process(hashmap * tracees, llist * tracee_list,
                          llist * run_queue, pid_t pid,
                          u32 n_insns_orig, int cpu_assigned, float rel_cpu_speed) {


	int i = 0;
	int ret;
	int cont = 1;
	char buffer[100];
	int singlestepmode = 1;
	tracee_entry * curr_tracee;
	unsigned long burst_target = BUFFER_WINDOW_SIZE;
	u32 status = 0;
        u32 n_insns;

	u32 flags = 0;


	curr_tracee = hmap_get_abs(tracees, pid);
	if (!curr_tracee)
		return FAIL;

	LOG("Running Child: %d Quanta: %d instructions\n", pid, n_insns);

	n_insns = n_insns_orig * (int) rel_cpu_speed;

	if (n_insns <= burst_target)
		singlestepmode = 1;
	else
		singlestepmode = 0;


	flush_buffer(buffer, 100);
	while (cont == 1) {

		if (is_tracee_blocked(curr_tracee) == TID_PROCESS_BLOCKED) {

			int sleep_duration = (int)n_insns * ((float)
			                                     rel_cpu_speed / 1000.0);
			if (sleep_duration >= 1) {
				usleep(sleep_duration);
			}

			LOG("Unblocked tracee %d blocked again\n", curr_tracee->pid);
			return SUCCESS;
		}

		if (singlestepmode) {
			errno = 0;
			ret = ptrace(PTRACE_SET_REM_MULTISTEP, pid, 0, (u32*)&n_insns);

			LOG("PTRACE RESUMING MULTI-STEPPING OF process. "
			    "ret = %d, error_code = %d, pid = %d, n_insns = %d\n", ret,
			    errno, pid, n_insns);

			ret = ptrace(PTRACE_MULTISTEP, pid, 0, (u32*)&n_insns);

		} else {
			errno = 0;

			n_insns = n_insns - burst_target;
			ptrace(PTRACE_SET_DELTA_BUFFER_WINDOW, pid, 0,
			       (unsigned long *)&burst_target);
			// init lib
			if (curr_tracee->pd == NULL) 
				curr_tracee->pd = libperf_initialize((int)pid, cpu_assigned);

			libperf_ioctlrefresh(curr_tracee->pd, LIBPERF_COUNT_HW_INSTRUCTIONS,
			                     (uint64_t )n_insns);
			// enable HW counter
			libperf_enablecounter(curr_tracee->pd, LIBPERF_COUNT_HW_INSTRUCTIONS);
			ret = ptrace(PTRACE_SET_REM_MULTISTEP, pid, 0, (u32*)&n_insns);

			
			ret = ptrace(PTRACE_CONT, pid, 0, 0);

			LOG("PTRACE RESUMING process. "
			    "ret = %d, error_code = %d. pid = %d, n_insns = %d\n", ret, errno, pid, n_insns);



		}

		if (ret < 0 || errno == ESRCH) {
			usleep(100000);
			if (kill(pid, 0) == -1 && errno == ESRCH) {
				LOG("PTRACE_RESUME ERROR. Child process is Dead. Breaking\n");
				if (curr_tracee->vfork_parent != NULL) {
					curr_tracee->vfork_parent->vfork_stop = 0;
					curr_tracee->vfork_parent = NULL;
				}
				if (curr_tracee->pd != NULL) {
					libperf_disablecounter(curr_tracee->pd,
							LIBPERF_COUNT_HW_INSTRUCTIONS);
					libperf_finalize(curr_tracee->pd, 0);
				}
				llist_remove(tracee_list, curr_tracee);
#ifndef PROCESS_MULTI_CORE_SCHED_MODE
				llist_remove(run_queue, curr_tracee);
#endif

				hmap_remove_abs(tracees, pid);
				print_tracee_list(tracee_list);
				free(curr_tracee);
				cont = 0;
				errno = 0;
				return FAIL;
			}
			LOG("PTRACE CONT Error: %d\n", errno);
			errno = 0;
			n_insns = n_insns_orig * (int) rel_cpu_speed;
			//assert(0);
			continue;
		}


		ret = wait_for_ptrace_events(tracees, tracee_list, run_queue, pid, curr_tracee->pd,
		                             cpu_assigned);
		switch (ret) {

		case TID_SYSCALL_ENTER:
			curr_tracee->syscall_blocked = 1;
			curr_tracee->n_preempts ++;
			return SUCCESS;

		case TID_IGNORE_PROCESS:
			llist_remove(tracee_list, curr_tracee);
#ifndef PROCESS_MULTI_CORE_SCHED_MODE
			llist_remove(run_queue, curr_tracee);
#endif

			hmap_remove_abs(tracees, pid);
			print_tracee_list(tracee_list);
			// For now, we handle this case like this.
			ptrace(PTRACE_CONT, pid, 0, 0);
			usleep(10);
			LOG("Process: %d, IGNORED\n", pid);
			if (curr_tracee->pd != NULL)
				libperf_finalize(curr_tracee->pd, 0);
			free(curr_tracee);
			return TID_IGNORE_PROCESS;


		case TID_EXITED:
			llist_remove(tracee_list, curr_tracee);
#ifndef PROCESS_MULTI_CORE_SCHED_MODE
			llist_remove(run_queue, curr_tracee);
#endif

			hmap_remove_abs(tracees, pid);
			print_tracee_list(tracee_list);
			// Exit is still not fully complete. need to do this to complete it.

			
			errno = 0;
			ret = ptrace(PTRACE_DETACH, pid, 0, SIGCHLD);
			
			LOG("Ptrace Detach: ret: %d, errno: %d\n", ret, errno);
			LOG("Process: %d, EXITED\n", pid);
			if (curr_tracee->pd != NULL)
				libperf_finalize(curr_tracee->pd, 0);

			free(curr_tracee);
			return TID_IGNORE_PROCESS;

		default:		return SUCCESS;

		}
		break;
	}
	return FAIL;


}


void run_processes_in_multi_core_mode(hashmap * tracees,
                                      llist * tracee_list, llist * run_queue,
                                      u32 n_round_insns,
                                      int * processes_pids,
                                      int cpu_assigned, float rel_cpu_speed, int fp) {

	int status;
	int per_process_advance = SMALLEST_PROCESS_QUANTA_INSNS;
	int process_advance_in_round = 0;
	int atleast_one_ran = 0;
	int atleast_one_runnable = 0;
#ifdef RETRY_BLOCKED_PROCESSES_MODE
	int blocked_processes_n_insns[MAX_IGNORE_PIDS];
	int blocked_processes_pids[MAX_IGNORE_PIDS];
	int n_blocked, n_blocked_insns;
#endif
	int n_ran, is_last;
	int processes_n_insns[MAX_IGNORE_PIDS];
	unsigned long arg;
	int i, j;

	do {

		atleast_one_runnable = 0;
		atleast_one_ran = 0;
		n_ran = 0;
		is_last = 0;


		for (i = 0; i < MAX_IGNORE_PIDS; i++) {

#ifdef RETRY_BLOCKED_PROCESSES_MODE
			blocked_processes_n_insns[i] = 0;
			blocked_processes_pids[i] = 0;
			n_blocked = 0;
			n_blocked_insns = 0;
#endif

			if (processes_pids[i])
				processes_n_insns[i] = n_round_insns;
		}




		for (i = 0; i < MAX_IGNORE_PIDS; i++) {
			if (processes_n_insns[i] > 0
			        && processes_n_insns[i] < per_process_advance) {
				process_advance_in_round = processes_n_insns[i];
				processes_n_insns[i] = 0;
			} else if (processes_n_insns[i] > 0) {
				process_advance_in_round = per_process_advance;
				processes_n_insns[i] -= per_process_advance;
				if (processes_n_insns[i] > 0)
					atleast_one_runnable = 1;
			} else {
				process_advance_in_round = 0;
			}

			if (process_advance_in_round) {

				is_last = 1;
				if (processes_n_insns[i] > 0 )
					is_last = 0;
				else {
					for (j = i + 1; j < MAX_IGNORE_PIDS; j++) {
						if (processes_n_insns[j]) {
							is_last = 0;
							break;
						}
					}
				}
				LOG("Running Process %d for %d instructions\n",
				    processes_pids[i], process_advance_in_round);

				status =
				    run_commanded_process(tracees, tracee_list, run_queue,
				                          processes_pids[i], process_advance_in_round,
				                          cpu_assigned, rel_cpu_speed);
				if (status == TID_PROCESS_BLOCKED) {
#ifdef RETRY_BLOCKED_PROCESSES_MODE
					blocked_processes_pids[n_blocked] = processes_pids[i];
					blocked_processes_n_insns[n_blocked] = process_advance_in_round;
					n_blocked ++;
					n_blocked_insns += process_advance_in_round;
#else
					usleep(process_advance_in_round / 1000);
#endif

				} else {
					n_ran ++;
				}


			}
		}

#ifdef RETRY_BLOCKED_PROCESSES_MODE
		if (n_blocked > 0) {

			usleep(per_process_advance / 1000);
			do {
				atleast_one_ran = 0;
				for (j = 0; blocked_processes_pids[j] != 0; j++) {

					if (blocked_processes_pids[j] != -1) {
						status = run_commanded_process(
						             tracees, tracee_list, run_queue,
						             blocked_processes_pids[j],
						             blocked_processes_n_insns[j],
						             cpu_assigned, rel_cpu_speed);
						if (status != TID_PROCESS_BLOCKED) {
							n_blocked_insns -= blocked_processes_n_insns[j];
							blocked_processes_pids[j] = -1;
							blocked_processes_n_insns[j] = -1;
							n_blocked --;
							atleast_one_ran = 1;
						}

					}
				}
			} while (atleast_one_ran == 1 && n_blocked > 0);
			sched_yield();
		}
#endif

		if (atleast_one_runnable)
			ioctl(fp, TK_IO_RESUME_BLOCKED_SYSCALLS, (unsigned long)process_advance_in_round);



	} while (atleast_one_runnable == 1);



}


void run_processes_in_round(hashmap * tracees,
                            llist * tracee_list,
                            u32 n_round_insns,
                            llist * run_queue,
                            llist * blocked_tracees,
                            int * processes_quanta_n_insns,
                            int * processes_pids,
                            int cpu_assigned, float rel_cpu_speed, int fp
                           ) {

	int base_insn_share = SMALLEST_PROCESS_QUANTA_INSNS;
	int status;
	u32 n_insns_run = 0;
	unsigned long arg;
	int n_checked_processes;
	int n_blocked_processes;
	tracee_entry * tracee;
	u32 n_insns_to_run;


#ifdef PROCESS_MULTI_CORE_SCHED_MODE
	return run_processes_in_multi_core_mode(tracees, tracee_list, run_queue,
	                                        n_round_insns, processes_pids,
	                                        cpu_assigned, rel_cpu_speed, fp);
#endif


#ifdef PROCESS_RR_SCHED_MODE
	for (int i = 0; i < MAX_IGNORE_PIDS; i++) {
		if (processes_pids[i]) {
			tracee_entry * curr_tracee = hmap_get_abs(tracees,
			                             processes_pids[i]);
			if (curr_tracee) {
				curr_tracee->n_insns_share = processes_quanta_n_insns[i];
			}

		}
	}
#endif

	do {

		n_insns_to_run = 0;
		n_checked_processes = 0;
		n_blocked_processes = llist_size(blocked_tracees);

		while (n_checked_processes < n_blocked_processes) {
			tracee_entry * curr_tracee = llist_get(blocked_tracees, 0);
			if (!curr_tracee)
				break;

			if (is_tracee_blocked(curr_tracee) == FAIL) {
				llist_pop(blocked_tracees);
				llist_append(run_queue, curr_tracee);

			} else {
				curr_tracee->n_sleeps ++;
				llist_requeue(blocked_tracees);
			}
			n_checked_processes ++;
		}


#ifdef PROCESS_RR_SCHED_MODE
		tracee = llist_get(run_queue, 0);
#else
		tracee = get_min_vrun_time_tracee(run_queue);
#endif

		if (!tracee) {

			if ((n_round_insns - n_insns_run) > base_insn_share) {
				n_insns_run += base_insn_share;
				n_insns_to_run = base_insn_share;
			} else {
				n_insns_to_run = n_round_insns - n_insns_run;
				n_insns_run = n_round_insns;

			}
			usleep((n_insns_to_run * rel_cpu_speed) / 1000);
			LOG("No Runnable tracees. Called resume blocked syscalls !");
			ioctl(fp, TK_IO_RESUME_BLOCKED_SYSCALLS, (unsigned long)(n_insns_to_run));
			if (n_insns_run < n_round_insns)
				continue;
			else
				return;
		}

		if (tracee->n_insns_share == 0)
			tracee->n_insns_share = base_insn_share;

#ifdef PROCESS_RR_SCHED_MODE
		if (tracee->n_insns_left) {

			if (n_insns_run + tracee->n_insns_left > n_round_insns) {
				n_insns_to_run = n_round_insns - n_insns_run;
				n_insns_run = n_round_insns;
				tracee->n_insns_left -= n_insns_to_run;
			} else {
				n_insns_run += tracee->n_insns_left;
				n_insns_to_run = tracee->n_insns_left;
				tracee->n_insns_left = 0;
			}
		} else {
			if (n_insns_run + tracee->n_insns_share > n_round_insns) {
				n_insns_to_run = n_round_insns - n_insns_run;
				n_insns_run = n_round_insns;
				tracee->n_insns_left = tracee->n_insns_share - n_insns_to_run;
			} else {
				n_insns_run += tracee->n_insns_share;
				n_insns_to_run = tracee->n_insns_share;
				tracee->n_insns_left = 0;
			}

		}
#else
		if (n_insns_run + tracee->n_insns_share > n_round_insns) {
			n_insns_to_run = n_round_insns - n_insns_run;
			n_insns_run = n_round_insns;
		}  else {
			n_insns_run += tracee->n_insns_share;
			n_insns_to_run = tracee->n_insns_share;
		}
#endif

		assert(n_insns_to_run > 0);

		LOG("Running %d for %d\n instructions\n", tracee->pid, n_insns_to_run);
		status =
		    run_commanded_process(tracees, tracee_list, run_queue,
		                          tracee->pid, n_insns_to_run,
		                          cpu_assigned, rel_cpu_speed);
		LOG("Ran %d for %d\n instructions\n", tracee->pid, n_insns_to_run);


		if (n_insns_run <= n_round_insns)
			ioctl(fp, TK_IO_RESUME_BLOCKED_SYSCALLS, (unsigned long)n_insns_to_run);

		tracee->vrun_time += (n_insns_to_run / 1000);

		if (status == SUCCESS) {

			if (is_tracee_blocked(tracee) == TID_PROCESS_BLOCKED ) {
				// tracee is blocked after running

#ifdef PROCESS_RR_SCHED_MODE
				tracee->n_insns_left = 0;
				llist_pop(run_queue);
#else
				llist_remove(run_queue, tracee);
#endif

				llist_append(blocked_tracees, tracee);
			} else {

#ifdef PROCESS_RR_SCHED_MODE
				if (tracee->n_insns_left == 0) {
					llist_requeue(run_queue);
				} else {
					// process not blocked and n_insns_left > 0. we reached end.
					LOG("N-insns left %d Pid: %d\n", tracee->n_insns_left, tracee->pid);
					break;
				}
#endif


			}
		}



	} while (n_insns_run < n_round_insns);






}

#ifdef TEST
/*
 * Initializes Message Buffer for use in test mode.
 */
void init_msg_buffer(struct nlmsghdr *nlh, struct sockaddr_nl * dst_addr) {

	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)dst_addr;
	msg.msg_namelen = sizeof(*dst_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

}
#endif

/*
 * Writes results of the last executed command back to Kronos.
 */
void write_results(int fp, char * command) {


	char * ptr;
	int ret = 0;
	ptr = command;
	ret = ioctl(fp, TK_IO_WRITE_RESULTS, command);

	LOG("Wrote results back. ioctl ret = %d \n", ret);
#ifdef DEBUG_VERBOSE
	LOG("Wrote results back. ioctl ret = %d \n", ret);
#endif
}

void print_usage_normal_mode(int argc, char* argv[]) {
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
	fprintf(stderr,	"		%s -i TRACER_ID "
	        "[-f CMDS_FILE_PATH or -c \"CMD with args\"] -r RELATIVE_CPU_SPEED "
	        "-n N_ROUND_INSTRUCTIONS -s CREATE_SPINNER\n", argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr, "This program executes all COMMANDs specified in the "
	        "CMD file path or the specified CMD with arguments in trace mode "
	        "under the control of Kronos\n");
	fprintf(stderr, "\n");
}

void print_usage_test_mode(int argc, char* argv[]) {
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
	fprintf(stderr,	"		%s [-f CMDS_FILE_PATH or -c \"CMD with args\"]\n",
	        argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr, "This program executes all COMMANDs specified in the "
	        "CMD file path in trace mode\n");
	fprintf(stderr, "\n");
}


int main(int argc, char * argv[]) {

	char * cmd_file_path = NULL;
	int fp;
	char * line;
	size_t line_read;
	size_t len = 0;

	u32 n_insns;
	u32 n_recv_commands = 0;

	llist tracee_list;
	llist run_queue;
	llist blocked_tracees;
	hashmap tracees;
	tracee_entry * new_entry;

	pid_t new_cmd_pid;
	pid_t new_pid;
	char nxt_cmd[MAX_PAYLOAD];
	int tail_ptr;
	int tracer_id = 0;
	int count = 0;
	float rel_cpu_speed;
	u32 n_round_insns;
	int cpu_assigned;
	char command[MAX_BUF_SIZ];
	int ignored_pids[MAX_IGNORE_PIDS];
	int n_cpus = get_nprocs();
	int read_ret = -1;
	int create_spinner = 0;
	pid_t spinned_pid;
	int cmd_no = 0;
	FILE* fp1;
	int option = 0;
	int read_from_file = 1;
	int i, n_tracees;
	int status;
	int n_total_insns_in_round;

	int processes_quanta_n_insns[MAX_IGNORE_PIDS];
	int processes_pids[MAX_IGNORE_PIDS];


	hmap_init(&tracees, 1000);
	llist_init(&tracee_list);
	llist_init(&run_queue);
	llist_init(&blocked_tracees);

	llist_set_equality_checker(&tracee_list, llist_elem_comparer);
	llist_set_equality_checker(&run_queue, llist_elem_comparer);
	llist_set_equality_checker(&blocked_tracees, llist_elem_comparer);


#ifndef TEST

	if (argc < 6 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		print_usage_normal_mode(argc, argv);
		exit(FAIL);
	}


	while ((option = getopt(argc, argv, "i:f:r:n:sc:h")) != -1) {
		switch (option) {
		case 'i' : tracer_id = atoi(optarg);
			break;
		case 'r' : rel_cpu_speed = atof(optarg);
			break;
		case 'f' : cmd_file_path = optarg;
			break;
		case 'n' : n_round_insns = (u32) atoi(optarg);
			break;
		case 's' : create_spinner = 1;
			break;
		case 'c' : flush_buffer(command, MAX_BUF_SIZ);
			sprintf(command, "%s\n", optarg);
			read_from_file = 0;
			break;
		case 'h' :
		default: print_usage_normal_mode(argc, argv);
			exit(FAIL);
		}
	}



#else
	if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		print_usage_test_mode(argc, argv);
		exit(FAIL);
	}


	while ((option = getopt(argc, argv, "f:c:h")) != -1) {
		switch (option) {
		case 'f' : cmd_file_path = optarg;
			break;
		case 'c' : flush_buffer(command, MAX_BUF_SIZ);
			sprintf(command, "%s\n", optarg);
			read_from_file = 0;
			break;
		case 'h' :
		default: print_usage_test_mode(argc, argv);
			exit(FAIL);
		}
	}
#endif



#ifdef DEBUG
	flush_buffer(logFile, MAX_FNAME_SIZ);
	sprintf(logFile, "/log/tracer%d.log", tracer_id);

	FILE* pFile = fopen(logFile, "w+");
	fclose(pFile);

#endif

	LOG("Tracer PID: %d\n", (pid_t)getpid());
#ifndef TEST
	LOG("TracerID: %d\n", tracer_id);
	LOG("REL_CPU_SPEED: %f\n", rel_cpu_speed);
	LOG("N_ROUND_INSNS: %lu\n", n_round_insns);
	LOG("N_EXP_CPUS: %d\n", n_cpus);
#endif


	if (read_from_file)
		LOG("CMDS_FILE_PATH: %s\n", cmd_file_path);
	else
		LOG("CMD TO RUN: %s\n", command);



	if (read_from_file) {
		fp1 = fopen(cmd_file_path, "r");
		if (fp1 == NULL) {
			LOG("ERROR opening cmds file\n");
			exit(FAIL);
		}
		while ((line_read = getline(&line, &len, fp1)) != -1) {
			count ++;
			LOG("TracerID: %d, Starting Command: %s", tracer_id, line);
			run_command(line, &new_cmd_pid);
			new_entry = alloc_new_tracee_entry(new_cmd_pid);
			llist_append(&tracee_list, new_entry);
#ifndef PROCESS_MULTI_CORE_SCHED_MODE
			llist_append(&run_queue, new_entry);
#endif

			hmap_put_abs(&tracees, new_cmd_pid, new_entry);
		}
		fclose(fp1);
	} else {
		LOG("TracerID: %d, Starting Command: %s", tracer_id, command);
		run_command(command, &new_cmd_pid);
		new_entry = alloc_new_tracee_entry(new_cmd_pid);
		llist_append(&tracee_list, new_entry);
#ifndef PROCESS_MULTI_CORE_SCHED_MODE
		llist_append(&run_queue, new_entry);
#endif

		hmap_put_abs(&tracees, new_cmd_pid, new_entry);
	}

	setup_all_traces(&tracees, &tracee_list, &run_queue);


#ifdef TEST
	sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
	if (sock_fd < 0) {
		LOG("TracerID: %d, SOCKET Error\n", tracer_id);
		exit(FAIL);
	}


	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); /* self pid */

	bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 1234; /* For Linux Kernel */
	dest_addr.nl_groups = 0; /* unicast */

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	if (!nlh) {
		LOG("TracerID: %d, Message space allocation failure\n",
		    tracer_id);
		exit(FAIL);
	}

	while (1) {
		get_next_command(sock_fd, &dest_addr, &msg,  nlh, &new_cmd_pid,
		                 &n_insns);
		if (new_cmd_pid == -1 )
			break;

		run_commanded_process(&tracees, &tracee_list, &run_queue, new_cmd_pid,
		                      n_insns, 0, 1.0);
	}
	close(sock_fd);

#else
	if (create_spinner) {
		create_spinner_task(&spinned_pid);
		cpu_assigned = addToExp_sp(1.0, n_round_insns, spinned_pid);
	} else
		cpu_assigned = addToExp(1.0, n_round_insns);

	if (cpu_assigned <= 0) {

		if ((255 - errno) > 0 && (255 - errno) < n_cpus) {
			cpu_assigned = 255 - errno;
			LOG("TracerID: %d, Assigned CPU: %d\n", tracer_id, cpu_assigned);
			errno = 0;
		} else {
			LOG("TracerID: %d, Registration Error. Errno: %d\n", tracer_id,
			    errno);
			exit(FAIL);
		}
	} else {
		LOG("TracerID: %d, Assigned CPU: %d\n", tracer_id, cpu_assigned);
	}


	fp = open("/proc/status", O_RDWR);
	if (fp == -1) {
		LOG("TracerID: %d, PROC File open error\n", tracer_id);
		exit(FAIL);
	}
	flush_buffer(nxt_cmd, MAX_PAYLOAD);
	sprintf(nxt_cmd, "%c,0,", TRACER_RESULTS);
	while (1) {

		tail_ptr = 0;
		new_cmd_pid = 0;
		n_insns = 0;
		read_ret = -1;
		cmd_no ++;
		n_total_insns_in_round = 0;

		for (i = 0; i < MAX_IGNORE_PIDS; i++) {
			ignored_pids[i] = 0;
			processes_pids[i] = 0;
			processes_quanta_n_insns[i] = 0;

		}

		llist_elem * head = tracee_list.head;
		tracee_entry * tracee;
		i = 0;
		while (head != NULL) {
			tracee = (tracee_entry *) head->item;
			processes_pids[i] = tracee->pid;
			processes_quanta_n_insns[i] = SMALLEST_PROCESS_QUANTA_INSNS;
			head = head->next;
			i++;
		}

		i = 0;
		write_results(fp, nxt_cmd);
		print_tracee_list(&tracee_list);
		LOG("TracerID: %d, ##########################################\n",
		    tracer_id);
		while (tail_ptr != -1) {
			tail_ptr = get_next_command_tuple(nxt_cmd, tail_ptr, &new_cmd_pid,
			                                  &n_insns);


			if (tail_ptr == -1 && new_cmd_pid == -1) {
				printf("TracerID: %d, STOP Command received. Stopping tracer ...\n",
				    tracer_id);
				fflush(stdout);
				goto end;
			}

			if (new_cmd_pid == 0)
				break;


			// By default, Kronos schedules in RR mode. Since we explicitly implment
			// Fair RR in the tracer itself, we ignore suggestions from Kronos.
			//processes_pids[i] = new_cmd_pid;
			//processes_quanta_n_insns[i] = (int)n_insns;
			//i++;

		}

		run_processes_in_round(&tracees, &tracee_list,
		                       n_round_insns, &run_queue, &blocked_tracees,
		                       processes_quanta_n_insns, processes_pids,
		                       cpu_assigned, rel_cpu_speed, fp);


		int j = 0;
		for (i = 0; processes_pids[i] != 0; i++) {
			if (!hmap_get_abs(&tracees,
			                  processes_pids[i])) {
				ignored_pids[j] = processes_pids[i];
				j++;
			}
		}

		flush_buffer(nxt_cmd, MAX_PAYLOAD);
		sprintf(nxt_cmd, "%c,", TRACER_RESULTS);
		for (i = 0; ignored_pids[i] != 0; i++) {
			sprintf(nxt_cmd + strlen(nxt_cmd), "%d,", ignored_pids[i]);
		}
		strcat(nxt_cmd, "0,");


	}
end:
	close(fp);


#endif
	if (create_spinner)
		kill(spinned_pid, SIGKILL);

	n_tracees = llist_size(&tracee_list);
	i = 0;
	while (i < n_tracees) {
		tracee_entry * curr_tracee = llist_get(&tracee_list, i);
		if (!curr_tracee)
			break;
		if (curr_tracee->pd != NULL) {
			libperf_disablecounter(curr_tracee->pd,
							LIBPERF_COUNT_HW_INSTRUCTIONS);
			libperf_finalize(curr_tracee->pd, 0);
		}
		i++;
	}
	llist_destroy(&tracee_list);
	hmap_destroy(&tracees);
	return 0;
}
