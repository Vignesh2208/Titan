#!/bin/bash

TITAN_DIR=$HOME/Titan
export PYTHONPATH="${PYTHONPATH}:${TITAN_DIR}"
OUTPUT_DIR=""
SOURCE_DIR=""

usage () {
  echo 'vtpp -s|--source_dir <Project source directory path> -o|--output_dir <Lookahead information storage path>'
}

POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -s|--source_dir)
    SOURCE_DIR="$2"
    shift
    shift
    ;;
    -o|--output_dir)
    OUTPUT_DIR="$2"
    shift
    shift
    ;;
    *)
    POSITIONAL+=("$1")
    shift
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

if [ -z "$SOURCE_DIR" ]
then
  echo 'ERROR: Project source directory is not specified !'
  usage
  exit 0
fi

if [ -z "$OUTPUT_DIR" ]
then
  OUTPUT_DIR=$SOURCE_DIR/.ttn/lookahead
  mkdir -p $OUTPUT_DIR
fi

chmod +x $TITAN_DIR/tools/vt_preprocessing/vt_preprocessor.py
$TITAN_DIR/tools/vt_preprocessing/vt_preprocessor.py --source_dir=$SOURCE_DIR --output_dir=$OUTPUT_DIR
