#!/bin/bash

#This little script serves to install correctly clang in Travis CI
#This complication seems to be due to the particular environment
if [ $V == 3.9 ]; then  sudo -E ln -sn /usr/share/llvm-3.9/cmake /usr/lib/llvm-3.9/lib/cmake/clang;     fi
if [ $V == 4.0 ]; then  sudo -E ln -sn /usr/share/llvm-4.0/cmake /usr/lib/llvm-4.0/lib/cmake/clang;     fi
if [ $V == 4.0 ]; then  sudo -E ln -sn /usr/share/llvm-4.0/cmake /usr/lib/llvm-4.0/lib/cmake/clang-4.0; fi
if [ $V == 4.0 ]; then  sudo -E ln -s  /usr/bin/*                /usr/lib/llvm-4.0/bin || true;         fi
