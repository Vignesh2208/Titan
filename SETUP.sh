#!/bin/bash

LLVM_TAG=llvmorg-9.0.1-rc3
LLVM_INSTALL_DIR=$HOME/llvm-project
TITAN_SOURCE_DIR=$HOME/Titan
TITAN_LLVM_SOURCE_DIR=$HOME/Titan/llvm
BASHRC_FILE="$HOME/.bashrc"

usage () {
  echo './SETUP.sh [commands]'
  echo ''
  echo ''
  echo 'commands: '
  echo ''
  echo 'install_deps : build and install titan project dependencies. Run first time'
  echo 'reinstall: rebuilds and installs titan project files and dependencies'
  echo 'clean: clean/remove titan project build files'
  echo 'build : build the titan project files'
  echo 'install : install the titan project files'
  echo 'uninstall : uninstall the titan project files and dependencies (llvm-project and syscall_intercept_lib)'
  echo ''
}

#default values
CMD=""

POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"
case $key in
    uninstall)
    CMD="uninstall"
    shift
    ;;
    clean)
    CMD="clean"
    shift
    ;;
    install_deps)
    CMD="install_deps"
    shift
    ;;
    build)
    CMD="build"
    shift
    ;;
    install)
    CMD="install"
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

download_llvm_project () {
    if [ -d $LLVM_INSTALL_DIR ]; then
        echo "Detected an already downloaded llvm-project folder. Skipping download step ..."
        return
    fi
    echo 'llvm-project: Creating install directory ...'    
    mkdir -p $LLVM_INSTALL_DIR
    echo 'llvm-project: Deleting any existing files inside installation directory ...'
    rm -rf $LLVM_INSTALL_DIR/*
    echo 'llvm-project: Downloading from repository with git tag: ' $LLVM_TAG
    git clone https://github.com/llvm/llvm-project.git $LLVM_INSTALL_DIR
    cd $LLVM_INSTALL_DIR && git fetch --all --tags --prune && git checkout tags/$LLVM_TAG
    echo 'llvm-project: Checking out version: ' $LLVM_TAG
}

patch_llvm_project () {

    DISABLE_LOOKAHEAD=$(grep -nr "DISABLE_LOOKAHEAD" $TITAN_SOURCE_DIR/CONFIG | awk -F "=" '{print $2}' | tr -d '[ \n]')
    DISABLE_INSN_CACHE_SIM=$(grep -nr "DISABLE_INSN_CACHE_SIM" $TITAN_SOURCE_DIR/CONFIG | awk -F "=" '{print $2}')
    DISABLE_DATA_CACHE_SIM=$(grep -nr "DISABLE_DATA_CACHE_SIM" $TITAN_SOURCE_DIR/CONFIG | awk -F "=" '{print $2}')
    DISABLE_VTL=$(grep -nr "DISABLE_VTL" $TITAN_SOURCE_DIR/CONFIG | awk -F "=" '{print $2}')
    COLLECT_STATS=$(grep -nr "COLLECT_STATS" $TITAN_SOURCE_DIR/CONFIG | awk -F "=" '{print $2}')


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
        echo "llvm-project: Patching " $DST_FILE " from " $SRC_FILE
        cp $SRC_FILE $DST_DIR
        if [ $LINE == "lib/CodeGen/VirtualTimeManagementIncludes.h" ]; then
            if [ $DISABLE_LOOKAHEAD == "yes" ]; then
                sed -i 's/#undef DISABLE_LOOKAHEAD/#define DISABLE_LOOKAHEAD/g' $DST_FILE
            fi

	    if [ $DISABLE_VTL == "yes" ]; then
                sed -i 's/#undef DISABLE_VTL/#define DISABLE_VTL/g' $DST_FILE
            fi

            if [ $DISABLE_INSN_CACHE_SIM == "yes" ]; then
                sed -i 's/#undef DISABLE_INSN_CACHE_SIM/#define DISABLE_INSN_CACHE_SIM/g' $DST_FILE
            fi

            if [ $DISABLE_DATA_CACHE_SIM == "yes" ]; then
                sed -i 's/#undef DISABLE_DATA_CACHE_SIM/#define DISABLE_DATA_CACHE_SIM/g' $DST_FILE
            fi

            if [ $COLLECT_STATS == "yes" ]; then
                sed -i 's/#undef COLLECT_STATS/#define COLLECT_STATS/g' $DST_FILE
            fi

        fi 
    else
        echo "llvm-project: Skipping applying patch for " $DST_FILE
    fi
    done
}

build_llvm_project () {
    echo "--------------------------------"
    echo "Buidling llvm-project ... "
    echo "--------------------------------"

    echo 'Downloading llvm-project if necessary ...'
    download_llvm_project

    LLVM_BUILD_DIR=$LLVM_INSTALL_DIR/build
    nCpus=$(nproc --all)
    if [ -d $LLVM_BUILD_DIR ]; then  # This is not the first installation attempt
 	

        echo 'llvm-project: Updating installed clang compiler ...'
    else # This is the first installation attempt
        echo 'llvm-project: Creating a fresh virtual time integrated clang compiler install ...'
        mkdir -p $LLVM_BUILD_DIR
        cd $LLVM_BUILD_DIR && cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS='clang' -DCMAKE_BUILD_TYPE=Release ../llvm
	    cd $LLVM_BUILD_DIR && make -j$nCpus
    fi

    echo 'Patching llvm-project with locally modified files ...'
    patch_llvm_project

    cd $LLVM_BUILD_DIR && make -j$nCpus
    
}

build_syscall_intercept_lib () {
    echo "--------------------------------"
    echo "Building syscall intercept ... "
    echo "--------------------------------"

    rm -rf $TITAN_SOURCE_DIR/ld_preloading/syscall_intercept/build
    mkdir -p $TITAN_SOURCE_DIR/ld_preloading/syscall_intercept/build
    cd $TITAN_SOURCE_DIR/ld_preloading/syscall_intercept/build && cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc ../
    cd $TITAN_SOURCE_DIR/ld_preloading/syscall_intercept/build && make -j8
}

build_titan_files () {
    echo "--------------------------------"
    echo "Buidling titan-files ... "
    echo "--------------------------------"
    cd $TITAN_SOURCE_DIR && make build
}

clean_project_files () {
    echo "----------------------------------------"
    echo "Cleaning any existing project-files ... "
    echo "----------------------------------------"

    cd $TITAN_SOURCE_DIR && sudo make clean
}

install_deps () {
    echo 'Installing all dependent external packages ...'
    sudo apt-get install -y git-core build-essential \
                            python3-dev autoconf autotools-dev automake \
                            libcap-dev libssl-dev pkg-config libcapstone-dev \
                            python3-pip cmake

} 


install_tools () {

    echo "--------------------------------"
    echo "Installing titan tools ... "
    echo "--------------------------------"

    echo 'Creating aliases for Titan tools ...'
    LINE="# <START> Titan tools aliases <START>"
    grep -qF -- "$LINE" "$BASHRC_FILE" || echo "$LINE" >> "$BASHRC_FILE"

    chmod +x $TITAN_SOURCE_DIR/tools/ttn/ttn.sh
    alias ttn=$TITAN_SOURCE_DIR/tools/ttn/ttn.sh
    LINE="alias ttn='$TITAN_SOURCE_DIR/tools/ttn/ttn.sh'"
    grep -qF -- "$LINE" "$BASHRC_FILE" || echo "$LINE" >> "$BASHRC_FILE"

    chmod +x $TITAN_SOURCE_DIR/tools/vt_preprocessing/vtpp.sh
    alias vtpp=$TITAN_SOURCE_DIR/tools/vt_preprocessing/vtpp.sh
    LINE="alias vtpp='$TITAN_SOURCE_DIR/tools/vt_preprocessing/vtpp.sh'"
    grep -qF -- "$LINE" "$BASHRC_FILE" || echo "$LINE" >> "$BASHRC_FILE"

    chmod +x $TITAN_SOURCE_DIR/tools/instructions/vtins.sh
    alias vtins=$TITAN_SOURCE_DIR/tools/instructions/vtins.sh
    LINE="alias vtins='$TITAN_SOURCE_DIR/tools/instructions/vtins.sh'"
    grep -qF -- "$LINE" "$BASHRC_FILE" || echo "$LINE" >> "$BASHRC_FILE"

    LINE="# <END> Titan tools aliases <END>"
    grep -qF -- "$LINE" "$BASHRC_FILE" || echo "$LINE" >> "$BASHRC_FILE"

    echo 'Initializing ttn ...'
    $TITAN_SOURCE_DIR/tools/ttn/ttn.sh init

    mkdir -p $HOME/.ttn
    chmod -R 777 $HOME/.ttn

    echo 'Generating instruction timings for all available architectures ...'
    $TITAN_SOURCE_DIR/tools/instructions/vtins.sh -a all
    echo 'Titan tools installation complete.'
}

install_llvm_project () {
    echo "--------------------------------"
    echo "Installing llvm-project ... "
    echo "--------------------------------"
    cd $LLVM_INSTALL_DIR/build && sudo make install
}

install_syscall_intercept_lib () {
    echo "--------------------------------"
    echo "Installing syscall-intercept ... "
    echo "--------------------------------"
    cd $TITAN_SOURCE_DIR/ld_preloading/syscall_intercept/build && sudo make install
}

install_titan_files () {
    echo "--------------------------------"
    echo "Installing titan files ... "
    echo "--------------------------------"
    cd $TITAN_SOURCE_DIR && sudo make install
}



if [ $CMD == "install_deps" ]; then
    echo "--------------------------------------------"
    echo "Building and installing all dependencies ..."
    echo "--------------------------------------------"
    install_deps
    build_llvm_project
    install_llvm_project
    build_syscall_intercept_lib
    install_syscall_intercept_lib
fi

if [ $CMD == "clean" ]; then
    clean_project_files
fi

if [ $CMD == "uninstall" ]; then
    clean_project_files
    # Uninstall clang and syscall_intercept_lib here
fi

if [ $CMD == "build" ]; then
    build_titan_files
fi

if [ $CMD == "install" ]; then
    install_tools
    install_titan_files
fi

if [ $CMD == "reinstall" ]; then
    build_llvm_project
    install_llvm_project
    build_syscall_intercept_lib
    install_syscall_intercept_lib
    build_titan_files
    install_tools
    install_titan_files
fi
