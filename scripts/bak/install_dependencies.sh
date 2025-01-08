#!/bin/bash

mypwd=$(pwd)

# build dependencies
cd dependencies;
#rm -rf build;
mkdir -p build;
cd build;
cmake .. -DCMAKE_INSTALL_PREFIX=~/.local;
make -j8;

cd ${mypwd};
