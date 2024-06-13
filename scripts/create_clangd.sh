#!/bin/bash
# create a .clangd for this build

# make a clangd file with our cwd
printf "${IBlue}create a .clangd to this directory!${NC}\n"
printf "${IYellow}$(pwd)\n${NC}"
printf "CompileFlags:\n" > .clangd
printf "\tAdd:\n" >> .clangd

# change the pwd
printf "\t- \"-I/usr/include/SDL2\"\n" >> .clangd

# matlab includes
printf "\t- \"-I$(pwd)/include\"\n" >> .clangd
printf "\t- \"-I$(pwd)/implot\"\n" >> .clangd
printf "\t- \"-I${MATLAB_PATH}/extern/include\"\n" >> .clangd

# lua includes
printf "\t- \"-I/usr/include/lua5.4\"\n" >> .clangd

# magnum
printf "\t- \"-I/usr/local/include/MagnumExternal/ImGui/\"\n" >> .clangd
