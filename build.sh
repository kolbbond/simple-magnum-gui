#!/bin/bash

mypwd=$(pwd)

# build dependencies
#rm -rf build;
echo "option: $1"
if [ -z "$1" ]; then
    mkdir -p build;
    cd build;
    cmake .. -DCMAKE_INSTALL_PREFIX=~/.local;
    make -j8;
elif [ $1 == "install" ]; then
    cd build;
    make install;
elif [ $1 == "test" ]; then
    cd build;
    make test;
fi

cd ${mypwd};
