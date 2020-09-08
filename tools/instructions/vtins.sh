#!/bin/bash

TITAN_DIR=$HOME/Titan
export PYTHONPATH="${PYTHONPATH}:${TITAN_DIR}"


usage () {
  echo 'Unrecognized argument. Usage: '
  echo 'vtins -a|--arch_name [DEFAULT=all] -o|--output_dir [DEFAULT=$HOME/.ttn/instructions] -c|--clear -l|--list'
}

OUTPUT_DIR=$HOME/.ttn/instructions
ARG_BUILDER=""

POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"
case $key in
    -a|--arch_name)
    ARCH="$2"
    shift
    shift
    ;;
    -o|--output_dir)
    OUTPUT_DIR="$2"
    shift
    shift
    ;;
    -c|--clear_all)
    CLEAR="TRUE"
    shift
    ;;
    -l|--list)
    LIST="TRUE"
    shift
    ;;
    *)
    POSITIONAL+=("$1")
    usage
    exit 0
    shift
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

chmod +x $TITAN_DIR/tools/instructions/instruction_latency_preprocessor.py

if [ -n "$ARCH" ]
then
  ARG_BUILDER+=" --arch_name=$ARCH"
fi


ARG_BUILDER+=" --output_dir=$OUTPUT_DIR"

if [ -n "$LIST" ]
then
  $TITAN_DIR/tools/instructions/instruction_latency_preprocessor.py --list $ARG_BUILDER
  exit 0
fi

if [ -n "$CLEAR" ]
then
  echo 'Removing any existing instruction timings files inside: ' $OUTPUT_DIR
  rm -rf $OUTPUT_DIR/*
  exit 0
fi



$TITAN_DIR/tools/instructions/instruction_latency_preprocessor.py $ARG_BUILDER
