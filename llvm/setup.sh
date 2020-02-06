#!/bin/bash

if [ ! -d "$HOME/llvm-project" ]
then
    echo 'Cloning LLVM project ...'
    git clone https://github.com/llvm/llvm-project.git
    git checkout llvmorg-9.0.1-rc3
fi

echo 'Copying patched files ...'
cp -v include/llvm/CodeGen/Passes.h $HOME/llvm-project/llvm/include/llvm/CodeGen/
cp -v include/llvm/InitializePasses.h $HOME/llvm-project/llvm/include/llvm/
cp -v lib/CodeGen/* $HOME/llvm-project/llvm/lib/CodeGen/

mkdir -p $HOME/llvm-project/build
cd $HOME/llvm-project/build && cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS='clang' -DCMAKE_BUILD_TYPE=Release ../llvm

echo 'Building LLVM and Clang ...'
cd $HOME/llvm-project/build && sudo make install -j8
