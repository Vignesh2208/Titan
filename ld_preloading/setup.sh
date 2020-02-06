#!/bin/bash



echo 'Installing dependencies ...'
sudo apt-get install pkg-config libcapstone-dev -y

echo 'Compiling Syscall Intercept library ...'
rm -rf $HOME/Titan/ld_preloading/syscall_intercept/build
mkdir -p $HOME/Titan/ld_preloading/syscall_intercept/build
cd $HOME/Titan/ld_preloading/syscall_intercept/build && cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc ../

echo 'Installing Syscall Intercept library ...'
cd $HOME/Titan/ld_preloading/syscall_intercept/build && make -j8 && sudo make install
