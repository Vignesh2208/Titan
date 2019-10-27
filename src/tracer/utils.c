#include "utils.h"
#include "tracer.h"

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

void print_tracee_list(llist *tracee_list) {
  llist_elem *head = tracee_list->head;
  tracee_entry *tracee;
  LOG("Active tracees: ");
  while (head != NULL) {
    tracee = (tracee_entry *)head->item;
    LOG("%d: %lu, %d, %d\n", tracee->pid, tracee->vrun_time, tracee->n_preempts,
        tracee->n_sleeps);
    head = head->next;
  }
}

int create_spinner_task(pid_t *child_pid) {
  pid_t child;

  child = fork();
  if (child == (pid_t)-1) {
    LOG("fork() failed in create_spinner_task: %s.\n", strerror(errno));
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

int run_command(char *full_command_str, pid_t *child_pid) {
  char **args;
  char *iter = full_command_str;

  int i = 0;
  int n_tokens = 0;
  int token_no = 0;
  int token_found = 0;
  int ret;
  pid_t child;

  while (full_command_str[i] != '\0') {
    if (i != 0 && full_command_str[i - 1] != ' ' &&
        full_command_str[i] == ' ') {
      n_tokens++;
    }
    i++;
  }

  args = malloc(sizeof(char *) * (n_tokens + 2));

  if (!args) {
    LOG("Malloc error in run_command\n");
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
    LOG("fork() failed in run_command: %s.\n", strerror(errno));
    exit(-1);
  }

  if (!child) {
    i = 0;
    while (args[i] != NULL) {
      if (strcmp(args[i], ">>") == 0 || strcmp(args[i], ">") == 0) {
        args[i] = '\0';
        /*file descriptor to the file we will redirect ls's output*/
        int fd;

        if (args[i + 1]) {
          if ((fd = open(args[i + 1], O_RDWR | O_CREAT)) == -1) {
            perror("open");
            exit(-1);
          }
          /*copy the file descriptor fd into standard output*/
          dup2(fd, STDOUT_FILENO);
          /* close the file descriptor as we don't need it more  */
          close(fd);
        }
        break;
      }
      if (strcmp(args[i], "<") == 0) {
        args[i] = '\0';
        /*file descriptor to the file we will redirect ls's output*/
        int fd;
        // printf("Stdin file : %s\n", args[i+1]);
        if (args[i + 1]) {
          if ((fd = open(args[i + 1], O_RDONLY)) < 0) {
            perror("open");
            exit(-1);
          }
          /*copy the file descriptor fd into standard input*/
          dup2(fd, 0);
          /* close the file descriptor as we don't need it more  */
          close(fd);
        }
      }
      i++;
    }

    prctl(PR_SET_DUMPABLE, (long)1);
    prctl(PR_SET_PTRACER, (long)getppid());
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    fflush(stdout);
    fflush(stderr);
    execvp(args[0], &args[0]);
    free(args);
    exit(2);
  }

  *child_pid = child;
  return 0;
}
