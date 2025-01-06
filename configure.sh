#!/bin/bash

mypwd=$(pwd)

# build dependencies
cd dependencies;
mkdir -p build;
cd build;
cmake ..;
make -j8;

cd ${mypwd};
