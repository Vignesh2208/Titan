#!/bin/bash

TITAN_DIR=$HOME/Titan
export PYTHONPATH="${PYTHONPATH}:${TITAN_DIR}"
OUTPUT_DIR=""
SOURCE_DIR=""

usage () {
  echo 'ttn [commands] [options] '
  echo ''
  echo ''
  echo 'commands: '
  echo ''
  echo 'init : ttn initialization. Needs to be run only once typically'
  echo 'add: adds a project to internal db/updates an existing projects parameters'
  echo 'activate : sets project as active'
  echo 'deactivate: unsets project as active'
  echo 'delete : delete project from internal db'
  echo 'reset: re-initializes the specified project clang parameters'
  echo 'list : list all projects in db'
  echo 'show : display parameters of the selected project'
  echo 'extract : extract and store lookahead information associated with project'
  echo ''
  echo ''
  echo 'Additional Options:'
  echo ''
  echo 'General Options:'
  echo '-p|--project_name : Name of the project'
  echo '-s|--project_src_dir : Project source directory'
  echo '-a|--arch : project compilation target architecture name'
  echo '--nic_mbps : modelled nic speed in mbps'
  echo '--cpu_mhz : cpu speed in mhz'
  echo '--timing_model : timing model: EMPIRICAL (default) or ANALYTICAL (experimental)'
  echo '--rob_size : re-order buffer size of modelled processor'
  echo '--dispatch_units : number of instruction dispatch units of modelled processor (experimental - used in analytical model)'
  
  echo ''
  echo 'Instruction Cache Options: [DISABLE_INSN_CACHE_SIM build CONFIG flag (inside ~/Titan/CONFIG) must be set to no]'
  
  echo '--l1_ins_cache_size_kb : l1-instruction cache size kb.'
  echo '--l1_ins_cache_lines : l1-instruction cache line size (bytes)'
  echo '--l1_ins_cache_policy : l1-instruction cache replacement policy (random or lru)'
  echo '--l1_ins_cache_miss_cycles : l1-instruction cache number of cycles per miss'
  echo '--l1_ins_cache_assoc : l1-instruction cache associativity'

  echo ''
  echo 'Instruction Cache Options: [DISABLE_DATA_CACHE_SIM build CONFIG flag (inside ~/Titan/CONFIG) must be set to no]'

  echo '--l1_data_cache_size_kb : l1-data cache size kb'
  echo '--l1_data_cache_lines : l1-data cache line size (bytes)'
  echo '--l1_data_cache_policy : l1-data cache replacement policy (random or lru)'
  echo '--l1_data_cache_miss_cycles : l1-data cache number of cycles per miss'
  echo '--l1_data_cache_assoc : l1-data cache associativity'
  
}
CMD=""
ARG_BUILDER=""

POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"
case $key in
    -p|--project_name)
    PROJECT_NAME="$2"
    shift
    shift
    ;;
    -s|--project_src_dir)
    PROJECT_SOURCE_DIR="$2"
    shift
    shift
    ;;
    -a|--arch)
    PROJECT_ARCH="$2"
    shift
    shift
    ;;
    --l1_ins_cache_size_kb)
    L1_INS_CACHE_SIZE_KB="$2"
    shift
    shift
    ;;
    --l1_ins_cache_lines)
    L1_INS_CACHE_LINES="$2"
    shift
    shift
    ;;
    --l1_ins_cache_policy)
    L1_INS_CACHE_REPLACEMENT_POLICY="$2"
    shift
    shift
    ;;
    --l1_ins_cache_miss_cycles)
    L1_INS_CACHE_MISS_CYCLES="$2"
    shift
    shift
    ;;
    --l1_ins_cache_assoc)
    L1_INS_CACHE_ASSOC="$2"
    shift
    shift
    ;;
    --l1_data_cache_size_kb)
    L1_DATA_CACHE_SIZE_KB="$2"
    shift
    shift
    ;;
    --l1_data_cache_lines)
    L1_DATA_CACHE_LINES="$2"
    shift
    shift
    ;;
    --l1_data_cache_policy)
    L1_DATA_CACHE_REPLACEMENT_POLICY="$2"
    shift
    shift
    ;;
    --l1_data_cache_miss_cycles)
    L1_DATA_CACHE_MISS_CYCLES="$2"
    shift
    shift
    ;;
    --l1_data_cache_assoc)
    L1_DATA_CACHE_ASSOC="$2"
    shift
    shift
    ;;
    --cpu_mhz)
    CPU_SPEED_MHZ="$2"
    shift
    shift
    ;;
    --nic_mbps)
    NIC_MBPS="$2"
    shift
    shift
    ;;
    --timing_model)
    TIMING_MODEL="$2"
    shift
    shift
    ;;
    --rob_size)
    ROB_SIZE="$2"
    shift
    shift
    ;;
    --dispatch_units)
    NUM_DISPATCH_UNITS="$2"
    shift
    shift
    ;;
    init)
    CMD="init"
    shift
    ;;
    activate)
    CMD="activate"
    shift
    ;;
    add)
    CMD="add"
    shift
    ;;
    deactivate)
    CMD="deactivate"
    shift
    ;;
    reset)
    CMD="reset"
    shift
    ;;
    delete)
    CMD="delete"
    shift
    ;;
    list)
    CMD="list"
    shift
    ;;
    show)
    CMD="show"
    shift
    ;;
    extract)
    CMD="extract"
    shift
    ;;
    *)
    POSITIONAL+=("$1")
    shift
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

if [ -z "$CMD" ]
then
  echo 'ERROR: No command specified !'
  usage
  exit 0
fi

ARG_BUILDER+="--$CMD"

if [ -n "$PROJECT_NAME" ]
then
  ARG_BUILDER+=" --project_name=$PROJECT_NAME"
fi

if [ -n "$PROJECT_SOURCE_DIR" ]
then
  ARG_BUILDER+=" --project_src_dir=$PROJECT_SOURCE_DIR"
fi

if [ -n "$PROJECT_ARCH" ]
then
  ARG_BUILDER+=" --arch=$PROJECT_ARCH"
fi

if [ -n "$L1_INS_CACHE_SIZE_KB" ]
then
  ARG_BUILDER+=" --l1_ins_cache_size_kb=$L1_INS_CACHE_SIZE_KB"
fi

if [ -n "$L1_INS_CACHE_LINES" ]
then
  ARG_BUILDER+=" --l1_ins_cache_lines=$L1_INS_CACHE_LINES"
fi

if [ -n "$L1_INS_CACHE_REPLACEMENT_POLICY" ]
then
  ARG_BUILDER+=" --l1_ins_cache_policy=$L1_INS_CACHE_REPLACEMENT_POLICY"
fi

if [ -n "$L1_INS_CACHE_MISS_CYCLES" ]
then
  ARG_BUILDER+=" --l1_ins_cache_miss_cycles=$L1_INS_CACHE_MISS_CYCLES"
fi


if [ -n "$L1_INS_CACHE_ASSOC" ]
then
  ARG_BUILDER+=" --l1_ins_cache_assoc=$L1_INS_CACHE_ASSOC"
fi

if [ -n "$L1_DATA_CACHE_SIZE_KB" ]
then
  ARG_BUILDER+=" --l1_data_cache_size_kb=$L1_DATA_CACHE_SIZE_KB"
fi

if [ -n "$L1_DATA_CACHE_LINES" ]
then
  ARG_BUILDER+=" --l1_data_cache_lines=$L1_DATA_CACHE_LINES"
fi

if [ -n "$L1_DATA_CACHE_REPLACEMENT_POLICY" ]
then
  ARG_BUILDER+=" --l1_data_cache_policy=$L1_DATA_CACHE_REPLACEMENT_POLICY"
fi

if [ -n "$L1_DATA_CACHE_MISS_CYCLES" ]
then
  ARG_BUILDER+=" --l1_data_cache_miss_cycles=$L1_DATA_CACHE_MISS_CYCLES"
fi

if [ -n "$L1_DATA_CACHE_ASSOC" ]
then
  ARG_BUILDER+=" --l1_data_cache_assoc=$L1_DATA_CACHE_ASSOC"
fi

if [ -n "$CPU_SPEED_MHZ" ]
then
  ARG_BUILDER+=" --cpu_mhz=$CPU_SPEED_MHZ"
fi


if [ -n "$TIMING_MODEL" ]
then
  ARG_BUILDER+=" --timing_model=$TIMING_MODEL"
fi

if [ -n "$ROB_SIZE" ]
then
  ARG_BUILDER+=" --rob_size=$ROB_SIZE"
fi

if [ -n "$NUM_DISPATCH_UNITS" ]
then
  ARG_BUILDER+=" --dispatch_units=$NUM_DISPATCH_UNITS"
fi


if [ -n "$NIC_MBPS" ]
then
  ARG_BUILDER+=" --nic_mbps=$NIC_MBPS"
fi

chmod +x $TITAN_DIR/tools/ttn/ttn.py
$TITAN_DIR/tools/ttn/ttn.py $ARG_BUILDER
