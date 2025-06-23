#!/bin/bash
RED='\033[0;31m'          # Red
NRM='\033[0m'

printf "${RED} install corrade ${NC}\n"
cd dependencies/corrade
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=${HOME}/.local
make -j8
make install

printf "${RED} install magnum ${NC}\n"
cd ../../magnum
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=${HOME}/.local \
    -DMAGNUM_WITH_SDL2APPLICATION=ON \
    -DMAGNUM_WITH_ANYIMAGEIMPORTER=ON \
    -DMAGNUM_WITH_ANYIMAGECONVERTER=ON \
    -DMAGNUM_WITH_ANYSCENECONVERTER=ON \
    -DMAGNUM_WITH_ANYSCENEIMPORTER=ON \
    -DMAGNUM_WITH_OBJIMPORTER=ON \
    -DMAGNUM_WITH_MAGNUMFONT=ON \
    -DMAGNUM_WITH_AUDIO=ON
make -j8
make install

printf "${RED} install magnum-plugins ${NC}\n"
cd ../../magnum-plugins
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=${HOME}/.local \
    -DMAGNUM_WITH_DRWAVIMPORTER=ON \
    -DMAGNUM_WITH_JPEGIMPORTER=ON \
    -DMAGNUM_WITH_PNGIMPORTER=ON \
    -DMAGNUM_WITH_FREETYPEFONT=ON \
    -DMAGNUM_WITH_PNGIMAGECONVERTER=ON;

make -j8
make install

printf "${RED} install magnum-integration ${NC}\n"
cd ../../magnum-integration
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=${HOME}/.local \
    -DMAGNUM_WITH_IMGUI=ON;
make -j8
make install



