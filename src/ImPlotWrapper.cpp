// ImPlotWrapper.cpp
// 240219 -

// include header
#include "ImPlotWrapper.hh"

// constructor
ImPlotWrapper::ImPlotWrapper() {}

int ImPlotWrapper::init(){
    // initialize implot context
  printf("creating ImPlot context\n");
  ImPlot::CreateContext();

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
