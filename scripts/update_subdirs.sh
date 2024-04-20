#!/bin/bash
# update the sub repos and reset them

# @hey, prolly add some checks

printf "${IBlue} updating imgui\n${NC}"

# for each sub dir
cd imgui
mv .not_git .git
git fetch; git status;
git pull
mv .git .not_git

cd ..

printf "${IBlue} updating implot\n${NC}"
cd implot
mv .not_git .git
git fetch; git status;
git pull
mv .git .not_git

cd ..

printf "${IBlue} updating magnum\n${NC}"
cd magnum
mv .not_git .git
git fetch; git status;
git pull
mv .git .not_git

cd ..

printf "${IBlue} updating magnum-integration\n${NC}"
cd magnum-integration
mv .not_git .git
git fetch; git status;
git pull
mv .git .not_git
