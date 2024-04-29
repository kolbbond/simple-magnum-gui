// lua example embed from wiki

// #include <stdio.h>
#include "LuaWrapper.hh"
#include <cstdio>
#include <iostream>

// for sleep
#include <string>
#include <unistd.h>

int main() {
  // create lua
  LuaWrapper L = LuaWrapper();

  // print from here
  printf("hello world from C\n");

  // first test LuaWrapper dostring
  L.dostring("print(\"hello world from lua\")");

  // user input
  std::string str_in;
  const char *str_out;

  L.close();

  return 0;
}
