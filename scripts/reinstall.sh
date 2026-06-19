#!/bin/bash

rm -rf build;
mkdir build;
cd build;
cmake .. -DCMAKE_INSTALL_PREFIX="$HOME/.local";
make -j64;
sudo make install;

