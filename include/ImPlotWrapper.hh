// ImPlotWrapper.hh

#ifndef ImPlotWrapper_hh
#define ImPlotWrapper_hh

#include <memory>
#include <guild/guild>

typedef std::shared_ptr<class ImPlotWrapper> ShImPlotWrapperPr;

class ImPlotWrapper {

  // properties
protected:
  // methods
public:
  // constructor
  ImPlotWrapper();

  // initialize context
  int init();

  // destroy
  int close();

  // factory
  static ShImPlotWrapperPr create();
};

#endif
