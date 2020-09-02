#!/bin/bash

LLVM_TAG=llvmorg-9.0.1-rc3
LLVM_INSTALL_DIR=$HOME/llvm-project
TITAN_SOURCE_DIR=$HOME/Titan
TITAN_LLVM_SOURCE_DIR=$HOME/Titan/llvm

install_deps () {
    echo 'Installing dependencies ...'
    sudo apt-get install -y git-core build-essential \
                            python-dev autoconf autotools-dev automake \
                            libcap-dev libssl-dev

} 

download_clang_vt () {
    if [ -d $LLVM_INSTALL_DIR ]; then
        echo "Detected an already downloaded llvm-project folder. Skipping download step ..."
        return
    fi
    echo 'Creating llvm-project install directory ...'    
    mkdir -p $LLVM_INSTALL_DIR
    echo 'Deleting any existing files inside installation directory ...'
    rm -rf $LLVM_INSTALL_DIR/*
    echo 'Downloading llvm-project with tag: ' $LLVM_TAG
    git clone https://github.com/llvm/llvm-project.git $LLVM_INSTALL_DIR
    cd $LLVM_INSTALL_DIR && git fetch --all --tags --prune && git checkout tags/$LLVM_TAG
    echo 'Checkout out llvm-project version: ' $LLVM_TAG
}

patch_clang_vt () {

    DISABLE_LOOKAHEAD=$(grep -nr "DISABLE_LOOKAHEAD" $TITAN_SOURCE_DIR/CONFIG | awk -F "=" '{print $2}' | tr -d '[ \n]')
    DISABLE_INSN_CACHE_SIM=$(grep -nr "DISABLE_INSN_CACHE_SIM" $TITAN_SOURCE_DIR/CONFIG | awk -F "=" '{print $2}')
    DISABLE_DATA_CACHE_SIM=$(grep -nr "DISABLE_DATA_CACHE_SIM" $TITAN_SOURCE_DIR/CONFIG | awk -F "=" '{print $2}')

    find $TITAN_LLVM_SOURCE_DIR -name \* -type f ! -path "*.sh" ! -path "*Makefile*" -print \
        | sed -e "s/^${TITAN_LLVM_SOURCE_DIR//\//\\/}//" \
        | cut -d'/' -f2- > /tmp/differences.txt
    cat /tmp/differences.txt | while read LINE
    do

    SRC_FILE=$TITAN_LLVM_SOURCE_DIR/$LINE
    DST_FILE=$LLVM_INSTALL_DIR/llvm/$LINE
    DST_DIR=$LLVM_INSTALL_DIR/llvm/$(dirname $LINE)
    COPY="true"

    if [ -f "$DST_FILE" ]; then
        SIMILARITY="$(cmp --silent $SRC_FILE $DST_FILE; echo $?)"
        if cmp --silent -- "$SRC_FILE" "$DST_FILE"; then  # two files are same
            COPY="false"
        fi
    fi

    if [ $COPY == "true" ] || [ $LINE == "lib/CodeGen/VirtualTimeManagementIncludes.h" ]; then
        echo "Patching " $DST_FILE " from " $SRC_FILE
        cp $SRC_FILE $DST_DIR
        if [ $LINE == "lib/CodeGen/VirtualTimeManagementIncludes.h" ]; then
            if [ $DISABLE_LOOKAHEAD == "yes" ]; then
                sed -i 's/#undef DISABLE_LOOKAHEAD/#define DISABLE_LOOKAHEAD/g' $DST_FILE
            fi

            if [ $DISABLE_INSN_CACHE_SIM == "yes" ]; then
                sed -i 's/#undef DISABLE_INSN_CACHE_SIM/#define DISABLE_INSN_CACHE_SIM/g' $DST_FILE
            fi

            if [ $DISABLE_DATA_CACHE_SIM == "yes" ]; then
                sed -i 's/#undef DISABLE_DATA_CACHE_SIM/#define DISABLE_DATA_CACHE_SIM/g' $DST_FILE
            fi

        fi 
    else
        echo "Skipping applying patch for " $DST_FILE
    fi
    done
}

build_clang_vt () {
    LLVM_BUILD_DIR=$LLVM_INSTALL_DIR/build
    nCpus=$(nproc --all)
    if [ -d $LLVM_BUILD_DIR ]; then  # This is not the first installation attempt
        echo 'Updating installed clang compiler ...'
    else # This is the first installation attempt
        echo 'Creating a fresh virtual time integrated clang compiler install ...'
    fi
    cd $LLVM_BUILD_DIR && make -j$nCpus && sudo make install
}

echo 'Downloading llvm-project if necessary ...'
download_clang_vt

echo 'Patching llvm-project with locally modified files ...'
patch_clang_vt

echo 'Building and installing clang compiler ...'
build_clang_vt