#!/bin/bash

TITAN_DIR=$HOME/Titan
FILE="$HOME/.bashrc"

echo 'Creating aliases for Titan tools ...'
LINE="# <START> Titan tools aliases <START>"
grep -qF -- "$LINE" "$FILE" || echo "$LINE" >> "$FILE"

chmod +x $TITAN_DIR/tools/ttn/ttn.sh
LINE="alias ttn='$TITAN_DIR/tools/ttn/ttn.sh'"
grep -qF -- "$LINE" "$FILE" || echo "$LINE" >> "$FILE"

chmod +x $TITAN_DIR/tools/vt_preprocessing/vtpp.sh
LINE="alias vtpp='$TITAN_DIR/tools/vt_preprocessing/vtpp.sh'"
grep -qF -- "$LINE" "$FILE" || echo "$LINE" >> "$FILE"

chmod +x $TITAN_DIR/tools/instructions/vtins.sh
LINE="alias vtins='$TITAN_DIR/tools/instructions/vtins.sh'"
grep -qF -- "$LINE" "$FILE" || echo "$LINE" >> "$FILE"

LINE="# <END> Titan tools aliases <END>"
grep -qF -- "$LINE" "$FILE" || echo "$LINE" >> "$FILE"

source $HOME/.bashrc

echo 'Initializing ttn ...'
$TITAN_DIR/tools/ttn/ttn.sh init

echo 'Generating instruction timings for all available architectures ...'
$TITAN_DIR/tools/instructions/vtins.sh -a all

echo 'Titan tools installation complete.'




