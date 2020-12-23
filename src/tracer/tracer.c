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
#include <stdio.h>
#include <pwd.h>
#include <arpa/inet.h>
#include "cJSON/cJSON.h"

#define MAX_COMMAND_LENGTH 1000
#define MAX_FILE_PATH_LENGTH 5000
#define MAX_STRING_SIZE 100
#define MAX_CONFIG_PARAM_VALUE_STRING_SIZE 100
#define FAIL -1

#define EXP_CBE 1
#define EXP_CS 2

#ifndef DISBABLE_LOOKAHEAD
#define PROJECT_SRC_DIR_KEY "PROJECT_SRC_DIR"
#endif

#define CPU_CYCLE_NS_KEY "CPU_CYCLES_NS"
#define TIMING_MODEL_KEY "TIMING_MODEL"
#define DEFAULT_CPU_CYCLES_NS 1.0
#define NIC_SPEED_MBPS_KEY "NIC_SPEED_MBPS"
#define DEFAULT_NIC_SPEED_MBPS 1000
#define DEFAULT_TIMING_MODEL "EMPIRICAL"


#ifndef DISABLE_INSN_CACHE_SIM
#define L1_INS_CACHE_SIZE_KEY "L1_INS_CACHE_SIZE_KB"
#define L1_INS_CACHE_LINES_SIZE_KEY "L1_INS_CACHE_LINE_SIZE"
#define L1_INS_CACHE_REPLACEMENT_POLICY_KEY "L1_INS_CACHE_REPLACEMENT_POLICY"
#define L1_INS_CACHE_MISS_CYCLES_KEY "L1_INS_CACHE_MISS_CYCLES"
#define L1_INS_CACHE_ASSOCIATIVITY_KEY "L1_INS_CACHE_ASSOCIATIVITY"
#define DEFAULT_L1_INS_CACHE_SIZE_KB 32
#define DEFAULT_L1_INS_CACHE_LINE_SIZE_BYTES 64
#define DEFAULT_L1_INS_CACHE_REPLACEMENT_POLICY "LRU"
#define DEFAULT_L1_INS_CACHE_MISS_CYCLES 100
#define DEFAULT_L1_INS_CACHE_ASSOCIATIVITY 1
#endif

#ifndef DISABLE_DATA_CACHE_SIM
#define L1_DATA_CACHE_SIZE_KEY "L1_DATA_CACHE_SIZE_KB"
#define L1_DATA_CACHE_LINES_SIZE_KEY "L1_DATA_CACHE_LINE_SIZE"
#define L1_DATA_CACHE_REPLACEMENT_POLICY_KEY "L1_DATA_CACHE_REPLACEMENT_POLICY"
#define L1_DATA_CACHE_MISS_CYCLES_KEY "L1_DATA_CACHE_MISS_CYCLES"
#define L1_DATA_CACHE_ASSOCIATIVITY_KEY "L1_DATA_CACHE_ASSOCIATIVITY"
#define DEFAULT_L1_DATA_CACHE_SIZE_KB 32
#define DEFAULT_L1_DATA_CACHE_LINE_SIZE_BYTES 64
#define DEFAULT_L1_DATA_CACHE_REPLACEMENT_POLICY "LRU"
#define DEFAULT_L1_DATA_CACHE_MISS_CYCLES 100
#define DEFAULT_L1_DATA_CACHE_ASSOCIATIVITY 1
#endif


#include <VT_functions.h>

extern char** environ;


void SetParamConfigString (char * param_config_name,
  char * param_config_value, const cJSON * json_obj, char * default_value) {

  if (cJSON_IsString(json_obj))
    snprintf(param_config_value,
      MAX_CONFIG_PARAM_VALUE_STRING_SIZE, "%s",
      cJSON_GetStringValue(json_obj));
  else
    snprintf(param_config_value,
      MAX_CONFIG_PARAM_VALUE_STRING_SIZE, "%s", default_value);

  if (setenv(param_config_name, param_config_value, 1) < 0)
    perror("Failed to set string environment variable\n");
  else
    printf ("Setting %s to: %s\n", param_config_name, param_config_value);

}

void SetParamConfigFloat (char * param_config_name,
  char * param_config_value, const cJSON * json_obj, float default_value) {
  if (cJSON_IsNumber(json_obj))
    snprintf(param_config_value,
      MAX_CONFIG_PARAM_VALUE_STRING_SIZE, "%f",
      cJSON_GetNumberValue(json_obj));
  else
    snprintf(param_config_value,
      MAX_CONFIG_PARAM_VALUE_STRING_SIZE, "%f", default_value);    

  if (setenv(param_config_name, param_config_value, 1) < 0)
    perror("Failed to set float environment variable\n");
  else
    printf ("Setting %s to: %s\n", param_config_name, param_config_value);

}

void SetParamConfigInt (char * param_config_name,
  char * param_config_value, const cJSON * json_obj, int default_value) {
  if (cJSON_IsNumber(json_obj))
    snprintf(param_config_value,
      MAX_CONFIG_PARAM_VALUE_STRING_SIZE, "%d",
      (int)cJSON_GetNumberValue(json_obj));
  else
    snprintf(param_config_value,
      MAX_CONFIG_PARAM_VALUE_STRING_SIZE, "%d", default_value);  

  if (setenv(param_config_name, param_config_value, 1) < 0)
    perror("Failed to set int environment variable\n");  
  else
    printf ("Setting %s to: %s\n", param_config_name, param_config_value);
}

int ParseTTNProject(char * project_name) {
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) {
    	homedir = getpwuid(getuid())->pw_dir;
    }
    printf("HOME DIRECTORY : %s\n", getenv("HOME"));
    char ttn_config_file[MAX_FILE_PATH_LENGTH];
    cJSON * ttn_config_json;
    cJSON * ttn_config_projects_json;
    cJSON * ttn_project_params_json;
    cJSON * ttn_project_cpu_cycles_ns;
    cJSON * ttn_project_nic_speed_mbps;
    cJSON * ttn_project_timing_model;
    char ttn_project_cpu_cycles_ns_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_nic_speed_mbps_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_timing_model_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];

    #ifndef DISABLE_LOOKAHEAD
    cJSON * ttn_project_src_dir_json;
    char ttn_project_bbl_lookahead_path[MAX_FILE_PATH_LENGTH];
    char ttn_project_loop_lookahead_path[MAX_FILE_PATH_LENGTH];
    #endif

    #ifndef DISABLE_INSN_CACHE_SIM
    cJSON * ttn_project_ins_cache_type;
    cJSON * ttn_project_ins_cache_miss_cycles;
    cJSON * ttn_project_ins_cache_lines;
    cJSON * ttn_project_ins_cache_size_kb;
    cJSON * ttn_project_ins_cache_assoc;
    char ttn_project_ins_cache_type_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_ins_cache_miss_cycles_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_ins_cache_lines_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_ins_cache_size_kb_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_ins_cache_assoc_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    #endif

    #ifndef DISABLE_DATA_CACHE_SIM
    cJSON * ttn_project_data_cache_type;
    cJSON * ttn_project_data_cache_miss_cycles;
    cJSON * ttn_project_data_cache_lines;
    cJSON * ttn_project_data_cache_size_kb;
    cJSON * ttn_project_data_cache_assoc;
    char ttn_project_data_cache_type_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_data_cache_miss_cycles_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_data_cache_lines_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_data_cache_size_kb_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    char ttn_project_data_cache_assoc_string[MAX_CONFIG_PARAM_VALUE_STRING_SIZE];
    #endif

    int status = 0;
    char * file_content = 0;
    long length;


    memset(ttn_config_file, 0, MAX_FILE_PATH_LENGTH);
    memset(ttn_project_cpu_cycles_ns_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_nic_speed_mbps_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_timing_model_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);

    #ifndef DISABLE_INSN_CACHE_SIM
    memset(ttn_project_ins_cache_type_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_ins_cache_miss_cycles_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_ins_cache_lines_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_ins_cache_size_kb_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_ins_cache_assoc_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    #endif

    #ifndef DISABLE_DATA_CACHE_SIM
    memset(ttn_project_data_cache_type_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_data_cache_miss_cycles_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_data_cache_lines_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_data_cache_size_kb_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    memset(ttn_project_data_cache_assoc_string, 0, MAX_CONFIG_PARAM_VALUE_STRING_SIZE);
    #endif

    #ifndef DISABLE_LOOKAHEAD
    memset(ttn_project_bbl_lookahead_path, 0, MAX_FILE_PATH_LENGTH);
    memset(ttn_project_loop_lookahead_path, 0, MAX_FILE_PATH_LENGTH);
    #endif

    snprintf(ttn_config_file, MAX_FILE_PATH_LENGTH, "%s/.ttn/projects.db",
      homedir);

    
    FILE * f = fopen (ttn_config_file, "rb");
    printf("Parsing TTN config file: %s\n", ttn_config_file);
    if (f) {
      fseek (f, 0, SEEK_END);
      length = ftell (f);
      fseek (f, 0, SEEK_SET);
      file_content = malloc (length);
      if (file_content) fread (file_content, 1, length, f);
      fclose (f);
    }

    if (!length || !file_content) {
      printf("TTN config file empty! Ignoring ...\n");
      return 0;
    }

    ttn_config_json = cJSON_Parse(file_content);
    if (!ttn_config_json) {
      const char *error_ptr = cJSON_GetErrorPtr();
      if (error_ptr != NULL)
        fprintf(stderr, "TTN config file parse error: %s. Ignoring ...\n", error_ptr);
      status = 0;
      goto end;
    }

    ttn_config_projects_json = cJSON_GetObjectItemCaseSensitive(
        ttn_config_json, "projects");
    if (!ttn_config_projects_json) {
      fprintf(stderr, "TTN config missing projects. Ignoring ...\n");
      status = 0;
      goto end;
    }

    ttn_project_params_json = cJSON_GetObjectItemCaseSensitive(
      ttn_config_projects_json, project_name);

    if (!ttn_project_params_json) {
      fprintf(stderr, "TTN project: %s not recognized. Ignoring ...",
        project_name);
      status = 0;
      goto end;
    }
    
    #ifndef DISABLE_LOOKAHEAD
    ttn_project_src_dir_json = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, PROJECT_SRC_DIR_KEY);
    #endif
    
    ttn_project_cpu_cycles_ns = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, CPU_CYCLE_NS_KEY);

    ttn_project_nic_speed_mbps = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, NIC_SPEED_MBPS_KEY);

    ttn_project_timing_model = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, TIMING_MODEL_KEY);
    
    #ifndef DISABLE_INSN_CACHE_SIM
    ttn_project_ins_cache_type = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_INS_CACHE_SIZE_KEY);
    ttn_project_ins_cache_miss_cycles = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_INS_CACHE_MISS_CYCLES_KEY);
    ttn_project_ins_cache_lines = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_INS_CACHE_LINES_SIZE_KEY);
    ttn_project_ins_cache_size_kb = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_INS_CACHE_SIZE_KEY);
    ttn_project_ins_cache_assoc = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_INS_CACHE_ASSOCIATIVITY_KEY);
    SetParamConfigInt("VT_L1_INS_CACHE_MISS_CYCLES",
      ttn_project_ins_cache_miss_cycles_string,
      ttn_project_ins_cache_miss_cycles, DEFAULT_L1_INS_CACHE_MISS_CYCLES);
    SetParamConfigInt("VT_L1_INS_CACHE_LINES", ttn_project_ins_cache_lines_string,
      ttn_project_ins_cache_lines, DEFAULT_L1_INS_CACHE_LINE_SIZE_BYTES);
    SetParamConfigInt("VT_L1_INS_CACHE_SIZE_KB",
      ttn_project_ins_cache_size_kb_string,
      ttn_project_ins_cache_size_kb, DEFAULT_L1_INS_CACHE_SIZE_KB);
    SetParamConfigInt("VT_L1_INS_CACHE_ASSOC",
      ttn_project_ins_cache_assoc_string,
      ttn_project_ins_cache_assoc, DEFAULT_L1_INS_CACHE_ASSOCIATIVITY);
    SetParamConfigString("VT_L1_INS_CACHE_REPLACEMENT_POLICY",
      ttn_project_ins_cache_type_string,
      ttn_project_ins_cache_type, DEFAULT_L1_INS_CACHE_REPLACEMENT_POLICY);
    #endif

    #ifndef DISABLE_DATA_CACHE_SIM
    ttn_project_data_cache_type = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_DATA_CACHE_REPLACEMENT_POLICY_KEY);
    ttn_project_data_cache_miss_cycles = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_DATA_CACHE_MISS_CYCLES_KEY);
    ttn_project_data_cache_lines = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_DATA_CACHE_LINES_SIZE_KEY);
    ttn_project_data_cache_size_kb = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_DATA_CACHE_SIZE_KEY);
    ttn_project_data_cache_assoc = cJSON_GetObjectItemCaseSensitive(
      ttn_project_params_json, L1_DATA_CACHE_ASSOCIATIVITY_KEY);
    SetParamConfigInt("VT_L1_DATA_CACHE_MISS_CYCLES",
      ttn_project_data_cache_miss_cycles_string,
      ttn_project_data_cache_miss_cycles, DEFAULT_L1_DATA_CACHE_MISS_CYCLES);
    SetParamConfigInt("VT_L1_DATA_CACHE_LINES",
      ttn_project_data_cache_lines_string,
      ttn_project_data_cache_lines, DEFAULT_L1_DATA_CACHE_LINE_SIZE_BYTES);
    SetParamConfigInt("VT_L1_DATA_CACHE_SIZE_KB",
      ttn_project_data_cache_size_kb_string,
      ttn_project_data_cache_size_kb, DEFAULT_L1_DATA_CACHE_SIZE_KB); 
    SetParamConfigInt("VT_L1_DATA_CACHE_ASSOC",
      ttn_project_data_cache_assoc_string,
      ttn_project_data_cache_assoc, DEFAULT_L1_DATA_CACHE_ASSOCIATIVITY);   
    SetParamConfigString("VT_L1_DATA_CACHE_REPLACEMENT_POLICY",
      ttn_project_data_cache_type_string,
      ttn_project_data_cache_type, DEFAULT_L1_DATA_CACHE_REPLACEMENT_POLICY);
    #endif

    SetParamConfigFloat("VT_CPU_CYLES_NS", ttn_project_cpu_cycles_ns_string,
      ttn_project_cpu_cycles_ns, DEFAULT_CPU_CYCLES_NS);

    SetParamConfigFloat("VT_NIC_SPEED_MBPS", ttn_project_nic_speed_mbps_string,
      ttn_project_nic_speed_mbps, DEFAULT_NIC_SPEED_MBPS);

    SetParamConfigString("VT_TIMING_MODEL",
      ttn_project_timing_model_string,
      ttn_project_timing_model, DEFAULT_TIMING_MODEL);
    
    #ifndef DISABLE_LOOKAHEAD
    if (cJSON_IsString(ttn_project_src_dir_json)) {
      snprintf(ttn_project_bbl_lookahead_path,
        MAX_FILE_PATH_LENGTH, "%s/.ttn/lookahead/bbl_lookahead.info",
        cJSON_GetStringValue(ttn_project_src_dir_json));
      snprintf(ttn_project_loop_lookahead_path,
        MAX_FILE_PATH_LENGTH, "%s/.ttn/lookahead/loop_lookahead.info",
        cJSON_GetStringValue(ttn_project_src_dir_json));
      if (setenv("VT_BBL_LOOKAHEAD_FILE", ttn_project_bbl_lookahead_path, 1) < 0 )
        perror("Failed to set env VT_BBL_LOOKAHEAD_FILE\n");
      else
        printf ("Setting VT_BBL_LOOKAHEAD_FILE to: %s\n",
          ttn_project_bbl_lookahead_path);

      if (setenv("VT_LOOP_LOOKAHEAD_FILE", ttn_project_loop_lookahead_path, 1) < 0)
        perror("Failed to set env VT_LOOP_LOOKAHEAD_FILE\n");
      else
        printf ("Setting VT_LOOP_LOOKAHEAD_FILE to: %s\n",
          ttn_project_loop_lookahead_path);
    }
    #endif

    status = 1;
end:
    cJSON_Delete(ttn_config_json);
    free(file_content);
    return status;
}

void SetEnvVariables(int tracer_id, int timeline_id, int exp_type,
  char * ttn_project_name, uint32_t src_ip_addr) {

  char tracer_id_env_variable[MAX_STRING_SIZE];
  char timeline_id_env_variable[MAX_STRING_SIZE];
  char exp_type_env_variable[MAX_STRING_SIZE];
  char src_ip_addr_str[MAX_STRING_SIZE];


  memset(tracer_id_env_variable, 0,  sizeof(char) * MAX_STRING_SIZE);
  memset(timeline_id_env_variable, 0, sizeof(char) * MAX_STRING_SIZE);
  memset(exp_type_env_variable, 0, sizeof(char) * MAX_STRING_SIZE);
  memset(src_ip_addr_str, 0, sizeof(char) * MAX_STRING_SIZE);
  
  if (snprintf(tracer_id_env_variable,
               MAX_STRING_SIZE, "%d", tracer_id) >= MAX_STRING_SIZE) {
    printf ("Tracer-id: %d has too many digits. Max allowed digits: %d\n",
      tracer_id, MAX_STRING_SIZE);
    exit(FAIL);
  }
  
  if (snprintf(timeline_id_env_variable,
      MAX_STRING_SIZE, "%d", timeline_id) >= MAX_STRING_SIZE) {
    printf ("Timeline-id: %d has too many digits. Max allowed digits: %d\n",
      timeline_id, MAX_STRING_SIZE);
    exit(FAIL);
  }

  if (snprintf(exp_type_env_variable,
      MAX_STRING_SIZE, "%d", exp_type) >= MAX_STRING_SIZE) {
    printf ("Exp-type: %d has too many digits. Max allowed digits: %d\n",
      timeline_id, MAX_STRING_SIZE);
    exit(FAIL);
  }

  if (snprintf(src_ip_addr_str,
      MAX_STRING_SIZE, "%d", src_ip_addr) >= MAX_STRING_SIZE) {
    printf ("Inet_addr(src_ip): %d has too many digits. Max allowed digits: %d\n",
      src_ip_addr, MAX_STRING_SIZE);
    exit(FAIL);
  }

  if (setenv("VT_TRACER_ID", tracer_id_env_variable, 1) < 0 )
      perror("Failed to set env VT_TRACER_ID\n");

  if (setenv("VT_TIMELINE_ID", timeline_id_env_variable, 1) < 0)
    perror("Failed to set env VT_TIMELINE_ID\n");
  
  if (setenv("VT_EXP_TYPE", exp_type_env_variable, 1) < 0)
    perror("Failed to set env VT_EXP_TYPE\n");

  #ifndef DISABLE_VT_SOCKET_LAYER
  if (setenv("VT_SOCKET_LAYER_IP", src_ip_addr_str, 1) < 0)
    perror("Failed to set env VT_SOCKET_LAYER_IP\n");
  #endif

  if (!ParseTTNProject(ttn_project_name))
    perror("Failure to parse and set ttn project env variables \n");

  setenv("LD_PRELOAD", "/usr/lib/libvtintercept.so", 1);
}

int RunCommandUnderVtManagement(
    char *orig_command_str, pid_t *child_pid, int tracer_id, int timeline_id,
    int exp_type, char *ttn_project_name, uint32_t src_ip_addr) {
  char **args;
  char full_command_str[MAX_COMMAND_LENGTH];
  char *iter = full_command_str;

  int i = 0;
  int n_tokens = 0;
  int token_no = 0;
  int token_found = 0;
  int ret;
  int fd;
  int matched_quotes = 1;
  pid_t child;

  memset(full_command_str, 0, sizeof(char) * MAX_COMMAND_LENGTH);  
  memcpy(full_command_str, orig_command_str, MAX_COMMAND_LENGTH);

  while (full_command_str[i] != '\0' && full_command_str[i] != '\n') {
    if (i != 0 && full_command_str[i - 1] != ' ' && full_command_str[i] == ' ')
      n_tokens++;
    i++;
  }

  args = malloc(sizeof(char *) * (n_tokens + 2));
  if (!args) {
    printf("Malloc error in run_command\n");
    exit(FAIL);
  }
  args[n_tokens + 1] = NULL;
  i = 0;

  while (full_command_str[i] != '\n' && full_command_str[i] != '\0') {

    if (full_command_str[i] == '"') {
      if (!matched_quotes) matched_quotes = 1; else matched_quotes = 0;
    }

    if (i == 0) {
      args[0] = full_command_str;
      token_no++;
      token_found = 1;
    } else {
      if (full_command_str[i] == ' ' && matched_quotes) {
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
          if ((fd = open(args[i + 1], O_RDWR | O_CREAT, 0666)) == -1)
            perror("open");
            
          ftruncate(fd, 0);
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
        break;
      }
      if (strcmp(args[i], "<") == 0) {
        args[i] = '\0';
        if (args[i + 1]) {
          if ((fd = open(args[i + 1], O_RDONLY)) < 0)
            perror("open");

          dup2(fd, 0);
          close(fd);
        }
      }
      printf("args[%d] = %s\n", i, args[i]);
      i++;
    }
    
    printf("Setting appropriate environment variables ...\n");
    SetEnvVariables(tracer_id, timeline_id, exp_type, ttn_project_name, src_ip_addr);
    printf("Starting command: %s\n", args[0]);
    fflush(stdout);
    fflush(stderr);
    execve(args[0], &args[0], environ);
    free(args);
    exit(0);
  }

  *child_pid = child;
  return 0;
}

void PrintUsage(int argc, char* argv[]) {
  fprintf(stderr, "\n");
  fprintf(stderr, "For displaying this message: %s [ -h | --help ]\n", argv[0]);
  fprintf(stderr, "\n");
  fprintf(stderr, "General usage: \n");
  fprintf(stderr, "\n");
  fprintf(stderr,
          "%s -i <TRACER_ID> -t <TIMELINE_ID> -e <EXP_TYPE>"
          " -c \"CMD with args\" -p <TTN PROJECT NAME> -a <Optional src-ip-addr>\n", argv[0]);
  fprintf(stderr, "\n");
  fprintf(stderr,
          "This program executes the specified CMD with arguments "
          "in trace mode under the control of Titan virtual time module\n");
  fprintf(stderr, "\n");
}


int main(int argc, char * argv[]) {

  size_t len = 0;
  int tracer_id = 0, timeline_id, exp_type;
  char command[MAX_COMMAND_LENGTH];
  char project[MAX_COMMAND_LENGTH];
  char srcIPAddr[MAX_COMMAND_LENGTH];
  int option = 0;
  int srcIPProvided = 0;
  int i, status;
  pid_t controlled_pid;


  memset(command, 0, sizeof(char)*MAX_COMMAND_LENGTH);
  memset(project, 0, sizeof(char)*MAX_COMMAND_LENGTH);
  memset(srcIPAddr, 0, sizeof(char)*MAX_COMMAND_LENGTH);
  timeline_id = 0;

  if (argc < 4 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
    PrintUsage(argc, argv);
    exit(FAIL);
  }


  while ((option = getopt(argc, argv, "i:e:t:n:c:p:a:h")) != -1) {
    switch (option) {
    case 'i' :  tracer_id = atoi(optarg);
                if (tracer_id < 0) {
                  printf("Tracer-id must be positive \n"); exit(FAIL);
                }
                break;
    case 't' :  timeline_id = atoi(optarg);
                if (timeline_id < 0) {
                  printf("Timeline-id must be positive \n"); exit(FAIL);
                }
                break;
    case 'e' :  exp_type = atoi(optarg);
                if (exp_type != EXP_CBE && exp_type != EXP_CS) {
                  printf (
                    "Only experiment types EXP_CBE (<=> 1) or EXP_CS (<=> 2) "
                    "are currently supported\n");
                  exit(FAIL);
                }
                break;
    case 'c' :  if (strlen(optarg) > MAX_COMMAND_LENGTH) {
                  printf ("Command: %s too long. Max allowed characters: %d\n",
                  optarg, MAX_COMMAND_LENGTH);
                  exit(FAIL);
                }
                snprintf(command, MAX_COMMAND_LENGTH, "%s", optarg);
                break;
    case 'p' :  if (strlen(optarg) > MAX_COMMAND_LENGTH) {
                  printf ("TTN Project Name: %s too long. Max allowed characters: %d\n",
                  optarg, MAX_COMMAND_LENGTH);
                  exit(FAIL);
                }
                snprintf(project, MAX_COMMAND_LENGTH, "%s", optarg);
                break;
    case 'a' :  if (strlen(optarg) > MAX_COMMAND_LENGTH) {
                  printf ("Src-ip string: %s too long. Max allowed characters: %d\n",
                  optarg, MAX_COMMAND_LENGTH);
                  exit(FAIL);
                }
                snprintf(srcIPAddr, MAX_COMMAND_LENGTH, "%s", optarg);
                srcIPProvided = 1;
                break;
    case 'h' :
    default  :  PrintUsage(argc, argv);
                exit(FAIL);
    }
  }

  FILE *fp;
  char path[1035];

  /* Open the command for reading. */
  fp = popen("/usr/sbin/arp -f /tmp/arp_entries.txt", "r");
  if (fp == NULL) {
    printf("No static arp entries set !\n" );
  } else {
    printf ("Attempting to set static arp entries ...\n");
     /* Read the output a line at a time - output it. */
     while (fgets(path, sizeof(path), fp) != NULL) {
	      printf("%s", path);
        fflush(stdout);
     }
     /* close */
     pclose(fp);
  }

  #ifndef DISABLE_VT_SOCKET_LAYER
  if (!srcIPProvided) {
    snprintf(srcIPAddr, MAX_COMMAND_LENGTH, "%s", "127.0.0.1");
  }
  printf("Tracer: %d >> Using SRC IP: %s for any spawned virtual tcp-stacks\n",
    tracer_id, srcIPAddr);
  printf ("Tracer: %d >> Updating firewall rules to drop outgoing TCP-RST packets\n", tracer_id);

  fp = popen("/sbin/iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP", "r");
  if (fp == NULL) {
    printf("Failed to add firewall rules !\n" );
  } else {
    printf ("Attempting to set firewall rules ...\n");
     /* Read the output a line at a time - output it. */
     while (fgets(path, sizeof(path), fp) != NULL) {
	      printf("%s", path);
        fflush(stdout);
     }
     /* close */
     pclose(fp);
  }

  #endif

  printf("Tracer: %d >> CMD TO RUN: %s\n", tracer_id, command);		

  RunCommandUnderVtManagement(
    command, &controlled_pid, tracer_id, timeline_id, exp_type, project,
    inet_addr(srcIPAddr));
  printf("Tracer: %d >> Started Command: %s, PID: %d. Timeline-id: %d\n",
    tracer_id, command, controlled_pid, timeline_id);

  // Block here with a call to VT-Module
  printf("Tracer: %d >> Waiting for virtual time experiment to finish ...\n",
    tracer_id);
  fflush(stdout);

  
  if (WaitForExit(tracer_id) < 0) // TODO: Better error handling here 
    printf("Tracer: %d >> ERROR: in wait for exit. Thats not good !\n",
      tracer_id);

  // Resume from Block. Send KILL Signal to each child.
  // TODO: Think of a more gracefull way to do this.
  printf("Tracer: %d >> Resumed. Waiting for processes to finish ...\n", tracer_id);
  fflush(stdout);

  usleep(2000000);

  kill(controlled_pid, SIGKILL);
  
  printf("Tracer: %d >> Waiting to read child status ...\n", tracer_id);
  fflush(stdout);
  waitpid(controlled_pid, &status, 0);
 
  printf("Tracer: %d >> Exiting ...\n", tracer_id);
  return 0;
}
