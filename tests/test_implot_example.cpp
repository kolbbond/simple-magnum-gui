#include "ImPlotExample.hh"
#include "cstdio"

int main(int argc, char **argv) {

  // make the gui class (inherits Platform)
  ImPlotExample tex({argc, argv});

  // exec calls mainloopiteration a bunch
  // this checks events and draws
  bool done = false;
  while (!done) {
    done = tex.mainLoopIteration();
    printf("%d\n",done);

    // get events?
  }

  // exit
  tex.exit();
}

