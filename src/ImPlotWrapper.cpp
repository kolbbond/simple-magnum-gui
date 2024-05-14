// ImPlotWrapper.cpp
// 240219 -

// include header
#include "ImPlotWrapper.hh"
#include "implot.h"


// constructor
ImPlotWrapper::ImPlotWrapper() {}

int ImPlotWrapper::init(){
    // initialize implot context
  printf("creating ImPlot context\n");
  // need to initialize with the imgui context
  //ImPlot::CreateContext();

  // success?
  return 1;

}

int ImPlotWrapper::close(){
    // destroy
  ImPlot::DestroyContext();

  // success?
  return 1;
}

// factory
ShImPlotWrapperPr ImPlotWrapper::create() {
  return std::make_shared<ImPlotWrapper>();
}
