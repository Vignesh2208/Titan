#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <getopt.h>
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
#define MAX_BUF_SIZ 100
#define MAX_STRING_SIZE 100
#define FAIL -1

#define EXP_CBE 1
#define EXP_CS 2

#include <VT_functions.h>

extern char** environ;


int run_command_under_vt_management(char *orig_command_str, pid_t *child_pid,
                          int tracer_id, int timeline_id, int exp_type) {
  char **args;
  char full_command_str[MAX_COMMAND_LENGTH];
  char tracer_id_env_variable[MAX_STRING_SIZE];
  char timeline_id_env_variable[MAX_STRING_SIZE];
  char exp_type_env_variable[MAX_STRING_SIZE];
  char *iter = full_command_str;

  int i = 0;
  int n_tokens = 0;
  int token_no = 0;
  int token_found = 0;
  int ret;
  int fd;
  pid_t child;

  memset(tracer_id_env_variable, 0,  sizeof(char) * MAX_STRING_SIZE);
  memset(timeline_id_env_variable, 0, sizeof(char) * MAX_STRING_SIZE);
  memset(exp_type_env_variable, 0, sizeof(char) * MAX_STRING_SIZE);

  sprintf(tracer_id_env_variable, "VT_TRACER_ID=%d", tracer_id);
  sprintf(timeline_id_env_variable, "VT_TIMELINE_ID=%d", timeline_id);
  sprintf(exp_type_env_variable, "VT_EXP_TYPE=%d", exp_type);
  
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
  full_command_str[i] = '\0';

  child = fork();
  if (child == (pid_t)-1) {
    printf("fork() failed in run_command: %s.\n", strerror(errno));
    exit(FAIL);
  }

  if (!child) {
    i = 0;
    while (args[i] != NULL) {
      if (strcmp(args[i], ">>") == 0 || strcmp(args[i], ">") == 0) {
        args[i] = '\0';
        if (args[i + 1]) {
          if ((fd = open(args[i + 1], O_RDWR | O_CREAT)) == -1) {
            perror("open");
            exit(FAIL);
          }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
        break;
      }
      if (strcmp(args[i], "<") == 0) {
        args[i] = '\0';
        if (args[i + 1]) {
          if ((fd = open(args[i + 1], O_RDONLY)) < 0) {
            perror("open");
            exit(FAIL);
          }
          dup2(fd, 0);
          close(fd);
        }
      }
      i++;
    }
    fflush(stdout);
    fflush(stderr);
    setenv("VT_TRACER_ID", tracer_id_env_variable, 1);
    setenv("VT_TIMELINE_ID", timeline_id_env_variable, 1);
    setenv("VT_EXP_TYPE", exp_type_env_variable, 1);
    setenv("LD_PRELOAD", "/usr/lib/libvtintercept.so", 1);

    execve(args[0], &args[0], environ);
    free(args);
    exit(0);
  }

  *child_pid = child;
  return 0;
}

void print_usage(int argc, char* argv[]) {
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
	fprintf(stderr,	"%s -i TRACER_ID -t TIMELINE_ID -e EXP_TYPE"
	        "-c \"CMD with args\" \n", argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr,
          "This program executes the specified CMD with arguments "
          "in trace mode under the control of VT-Module\n");
	fprintf(stderr, "\n");
}


int main(int argc, char * argv[]) {

	size_t len = 0;
	int tracer_id = 0, timeline_id, exp_type;
	char command[MAX_BUF_SIZ];
	int option = 0;
	int i, status;
  pid_t controlled_pid;


	if (argc < 4 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		print_usage(argc, argv);
		exit(FAIL);
	}


	while ((option = getopt(argc, argv, "i:e:t:n:c:h")) != -1) {
		switch (option) {
		case 'i' : tracer_id = atoi(optarg);
			break;
    case 't' : timeline_id = atoi(optarg);
      break;
    case 'e' : exp_type = atoi(optarg);
      break;
		case 'c' : memset(command, 0, sizeof(char)*MAX_BUF_SIZ);
			sprintf(command, "%s\n", optarg);
			break;
		case 'h' :
		default: print_usage(argc, argv);
			       exit(FAIL);
		}
	}

	
	printf("CMD TO RUN: %s\n", command);		

  if (tracer_id <= 0) {
    printf("Tracer ID must be positive !\n");
    exit(FAIL);
  }

  if (exp_type != EXP_CS && exp_type != EXP_CBE) {
    printf("exp_type must be either EXP_CBE or EXP_CS\n");
    exit(FAIL);
  }

  if (exp_type == EXP_CS && timeline_id < 0) {
    printf("Timeline ID must be >= 0\n");
    exit(FAIL);
  }

	run_command_under_vt_management(
    command, &controlled_pid, tracer_id, timeline_id, exp_type);
	printf("TracerID: %d, Started Command: %s", tracer_id, command);

	// Block here with a call to VT-Module
	printf("Waiting for exit !\n");
	fflush(stdout);
	if (wait_for_exit(tracer_id) < 0) {
		printf("ERROR: IN Wait for exit. Thats not good !\n");
	}
	// Resume from Block. Send KILL Signal to each child.
  // TODO: Think of a more gracefull way to do this.
	printf("Resumed from Wait for Exit! Waiting for processes to finish !\n");
	fflush(stdout);
	
	kill(controlled_pid, SIGKILL);
	waitpid(controlled_pid, &status, 0);
	 
  printf("Exiting Tracer: %d\n", tracer_id);
	return 0;
}
