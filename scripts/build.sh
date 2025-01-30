#!/bin/bash

cmake -B build -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build --parallel --target install
