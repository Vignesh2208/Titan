#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <pthread.h> 
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* uintmax_t */
#include <string.h>
#include <sys/mman.h>
#include <unistd.h> /* sysconf */
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <signal.h>

#define MAX_COMMAND_LENGTH 1000
#define MAX_CONTROLLABLE_PROCESSES 100
#define MAX_BUF_SIZ 100
#define FAIL -1

#include "VT_functions.h"


/***
 Convert string to integer
***/
int str_to_i(char *s) {
  int i, n;
  n = 0;
  for (i = 0; *(s + i) >= '0' && *(s + i) <= '9'; i++)
    n = 10 * n + *(s + i) - '0';
  return n;
}

/***
Used when reading the input from Kronos module.
Will basically return the next number in a string
***/
int get_next_value(char *write_buffer) {
  int i;
  for (i = 1; *(write_buffer + i) >= '0' && *(write_buffer + i) <= '9'; i++) {
    continue;
  }

  if (write_buffer[i] == '\0') return -1;

  if (write_buffer[i + 1] == '\0') return -1;

  return (i + 1);
}


int create_spinner_task(pid_t *child_pid) {
  pid_t child;

  child = fork();
  if (child == (pid_t)-1) {
    printf("fork() failed in create_spinner_task: %s.\n", strerror(errno));
    exit(-1);
  }

  if (!child) {
    while (1) {
      usleep(1000000);
    }
    exit(2);
  }

  *child_pid = child;
  return 0;
}

void envelope_command_under_pin(char * enveloped_cmd_str, char * orig_command_str, int total_num_tracers, int tracer_id) {
	char * pin_binary_path = "/home/titan/pin/pin";
	char * pintool_path = "/home/titan/Titan/src/tracer/pintool/obj-intel64/inscount_tls.so";
	memset(enveloped_cmd_str, 0, MAX_COMMAND_LENGTH);
	sprintf(enveloped_cmd_str, "%s -t %s -n %d -i %d -- %s", pin_binary_path, pintool_path, total_num_tracers, tracer_id, orig_command_str);
	printf("Full enveloped command: %s", enveloped_cmd_str);
	
} 

int run_command_under_pin(char *orig_command_str, pid_t *child_pid, int total_num_tracers, int tracer_id) {
  char **args;
  char full_command_str[MAX_COMMAND_LENGTH];
  char *iter = full_command_str;

  int i = 0;
  int n_tokens = 0;
  int token_no = 0;
  int token_found = 0;
  int ret;
  pid_t child;

  envelope_command_under_pin(full_command_str, orig_command_str, total_num_tracers, tracer_id);

  
  while (full_command_str[i] != '\0') {
    if (i != 0 && full_command_str[i - 1] != ' ' &&
        full_command_str[i] == ' ') {
      n_tokens++;
    }
    i++;
  }

  args = malloc(sizeof(char *) * (n_tokens + 2));

  if (!args) {
    printf("Malloc error in run_command\n");
    exit(-1);
  }
  args[n_tokens + 1] = NULL;

  i = 0;

  while (full_command_str[i] != '\n' && full_command_str[i] != '\0') {
    if (i == 0) {
      args[0] = full_command_str;
      token_no++;
      token_found = 1;
    } else {
      if (full_command_str[i] == ' ') {
        full_command_str[i] = '\0';
        token_found = 0;
      } else if (token_found == 0) {
        args[token_no] = full_command_str + i;
        token_no++;
        token_found = 1;
      }
    }
    i++;
  }
  full_command_str[i] = NULL;

  child = fork();
  if (child == (pid_t)-1) {
    printf("fork() failed in run_command: %s.\n", strerror(errno));
    exit(-1);
  }

  if (!child) {
    i = 0;
    while (args[i] != NULL) {
      if (strcmp(args[i], ">>") == 0 || strcmp(args[i], ">") == 0) {
        args[i] = '\0';
        
        int fd;

        if (args[i + 1]) {
          if ((fd = open(args[i + 1], O_RDWR | O_CREAT)) == -1) {
            perror("open");
            exit(-1);
          }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
        break;
      }
      if (strcmp(args[i], "<") == 0) {
        args[i] = '\0';
        int fd;
        // printf("Stdin file : %s\n", args[i+1]);
        if (args[i + 1]) {
          if ((fd = open(args[i + 1], O_RDONLY)) < 0) {
            perror("open");
            exit(-1);
          }
          dup2(fd, 0);
          close(fd);
        }
      }
      i++;
    }
    fflush(stdout);
    fflush(stderr);
    execvp(args[0], &args[0]);
    free(args);
    exit(2);
  }

  *child_pid = child;
  return 0;
}

void print_usage(int argc, char* argv[]) {
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
	fprintf(stderr,	"		%s -i TRACER_ID -n TOTAL_NUM_TRACERS"
	        "[-f CMDS_FILE_PATH or -c \"CMD with args\"] "
	        "-s CREATE_SPINNER\n", argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr, "This program executes all COMMANDs specified in the "
	        "CMD file path or the specified CMD with arguments in trace mode "
	        "under the control of VT-Module\n");
	fprintf(stderr, "\n");
}


int main(int argc, char * argv[]) {

	char * cmd_file_path = NULL;
	char * line;
	size_t line_read;
	size_t len = 0;
	pid_t new_cmd_pid;
	int tracer_id = 0;
	char command[MAX_BUF_SIZ];
	int cpu_assigned;
	int n_cpus = get_nprocs();
	int create_spinner = 0;
	pid_t spinned_pid;
	FILE* fp;
	int option = 0;
	int read_from_file = 1;
	int i, status;
	int total_num_tracers, num_controlled_processes;
        pid_t controlled_pids[MAX_CONTROLLABLE_PROCESSES];
	num_controlled_processes = 0;

	if (argc < 4 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		print_usage(argc, argv);
		exit(FAIL);
	}


	while ((option = getopt(argc, argv, "i:f:n:sc:h")) != -1) {
		switch (option) {
		case 'i' : tracer_id = atoi(optarg);
			break;
		case 'n' : total_num_tracers = atoi(optarg);
			break;
		case 'f' : cmd_file_path = optarg;
			break;
		case 's' : create_spinner = 1;
			break;
		case 'c' : memset(command, 0, sizeof(char)*MAX_BUF_SIZ);
			sprintf(command, "%s\n", optarg);
			read_from_file = 0;
			break;
		case 'h' :
		default: print_usage(argc, argv);
			exit(FAIL);
		}
	}

	/*
	printf("Tracer PID: %d\n", (pid_t)getpid());
	printf("TracerID: %d\n", tracer_id);
	printf("N_EXP_CPUS: %d\n", n_cpus);*/

	if (read_from_file)
		printf("CMDS_FILE_PATH: %s\n", cmd_file_path);
	else
		printf("CMD TO RUN: %s\n", command);

	
	if (create_spinner) {
		create_spinner_task(&spinned_pid);
		cpu_assigned = register_tracer(tracer_id, TRACER_TYPE_APP_VT, REGISTRATION_W_SPINNER, spinned_pid);
	} else
		cpu_assigned = register_tracer(tracer_id, TRACER_TYPE_APP_VT, SIMPLE_REGISTRATION, 0);

	if (cpu_assigned < 0) {
		printf("TracerID: %d, Registration Error. Errno: %d\n", tracer_id,
		    errno);
		exit(FAIL);
	}

	if (read_from_file) {
		fp = fopen(cmd_file_path, "r");
		if (fp == NULL) {
			printf("ERROR opening cmds file\n");
			exit(FAIL);
		}
		while ((line_read = getline(&line, &len, fp)) != -1) {
			num_controlled_processes ++;
			
			run_command_under_pin(line, &new_cmd_pid, total_num_tracers, tracer_id);
			controlled_pids[num_controlled_processes - 1] = new_cmd_pid;
			printf("TracerID: %d, Started Command: %s", tracer_id, line);
		}
		fclose(fp);
	} else {
		
		num_controlled_processes ++;
		run_command_under_pin(command, &new_cmd_pid, total_num_tracers, tracer_id);
		controlled_pids[num_controlled_processes - 1] = new_cmd_pid;
		printf("TracerID: %d, Started Command: %s", tracer_id, command);
	}

	
	// Block here with a call to VT-Module
	printf("Waiting for exit !\n");
	fflush(stdout);
	if (wait_for_exit(tracer_id) < 0) {
		printf("ERROR: IN Wait for exit. Thats not good !\n");
	}
	// Resume from Block. Send KILL Signal to each child. TODO: Think of a more gracefull way to do this.
	printf("Resumed from Wait for Exit! Waiting for processes to finish !\n");
	fflush(stdout);
	for(i = 0; i < num_controlled_processes; i++) {
		kill(controlled_pids[i], SIGKILL);
		waitpid(controlled_pids[i], &status, 0);
	} 

        printf("Exiting Tracer ...\n");
	return 0;
}
