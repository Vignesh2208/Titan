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
  echo 'activate : set project as active and add it to internal db'
  echo 'deactivate: unset project as active'
  echo 'delete : delete project from internal db'
  echo 'reset: re-initializes the specified project parameters'
  echo 'list : list all projects in db'
  echo 'show : display parameters of the selected project'
  echo 'extract : extract and store lookahead information associated with project'
  echo ''
  echo ''
  echo 'options:'
  echo ''
  echo '-p|--project_name : Name of the project'
  echo '-s|--project_src_dir : Project source directory'
  echo '-a|--arch : project compilation target architecture name'
  echo '--ins_cache_size_kb : instruction cache size kb'
  echo '--ins_cache_lines : instruction cache lines'
  echo '--ins_cache_type : instruction cache type'
  echo '--ins_cache_miss_cycles : instruction cache number of cycles per miss'
  echo '--data_cache_size_kb : data cache size kb'
  echo '--data_cache_lines : data cache lines'
  echo '--data_cache_type : data cache type'
  echo '--data_cache_miss_cycles : data cache number of cycles per miss'
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
    -o|--project_src_dir)
    PROJECT_SOURCE_DIR="$2"
    shift
    shift
    ;;
    -a|--arch)
    PROJECT_ARCH="$2"
    shift
    shift
    ;;
    --ins_cache_size_kb)
    INS_CACHE_SIZE_KB="$2"
    shift
    shift
    ;;
    --ins_cache_lines)
    INS_CACHE_LINES="$2"
    shift
    shift
    ;;
    --ins_cache_type)
    INS_CACHE_TYPE="$2"
    shift
    shift
    ;;
    --ins_cache_miss_cycles)
    INS_CACHE_MISS_CYCLES="$2"
    shift
    shift
    ;;
    --data_cache_size_kb)
    DATA_CACHE_SIZE_KB="$2"
    shift
    shift
    ;;
    --data_cache_lines)
    DATA_CACHE_LINES="$2"
    shift
    shift
    ;;
    --data_cache_type)
    DATA_CACHE_TYPE="$2"
    shift
    shift
    ;;
    --data_cache_miss_cycles)
    DATA_CACHE_MISS_CYCLES="$2"
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

if [ -n "$INS_CACHE_SIZE_KB" ]
then
  ARG_BUILDER+=" --ins_cache_size_kb=$INS_CACHE_SIZE_KB"
fi

if [ -n "$INS_CACHE_LINES" ]
then
  ARG_BUILDER+=" --ins_cache_lines=$INS_CACHE_LINES"
fi

if [ -n "$INS_CACHE_TYPE" ]
then
  ARG_BUILDER+=" --ins_cache_type=$INS_CACHE_TYPE"
fi

if [ -n "$INS_CACHE_MISS_CYCLES" ]
then
  ARG_BUILDER+=" --ins_cache_miss_cycles=$INS_CACHE_MISS_CYCLES"
fi

if [ -n "$DATA_CACHE_SIZE_KB" ]
then
  ARG_BUILDER+=" --data_cache_size_kb=$DATA_CACHE_SIZE_KB"
fi

if [ -n "$DATA_CACHE_LINES" ]
then
  ARG_BUILDER+=" --data_cache_lines=$DATA_CACHE_LINES"
fi

if [ -n "$DATA_CACHE_TYPE" ]
then
  ARG_BUILDER+=" --data_cache_type=$DATA_CACHE_TYPE"
fi

if [ -n "$DATA_CACHE_MISS_CYCLES" ]
then
  ARG_BUILDER+=" --data_cache_miss_cycles=$DATA_CACHE_MISS_CYCLES"
fi


chmod +x $TITAN_DIR/tools/ttn/ttn.py
$TITAN_DIR/tools/ttn/ttn.py $ARG_BUILDER
