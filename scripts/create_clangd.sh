#!/bin/bash
# create a .clangd for this build

# make a clangd file with our cwd
printf "${IBlue}create a .clangd to this directory!${NC}\n"
printf "${IYellow}$(pwd)\n${NC}"
printf "CompileFlags:\n" > .clangd
printf "\tAdd:\n" >> .clangd

# change the pwd
printf "\t- -I$(pwd)/imgui\n" >> .clangd
printf "\t- -I$(pwd)/implot\n" >> .clangd
