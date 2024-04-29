// matlab example embed from docs
#include "MatlabEngine.hpp"
#include <memory>
#include <stdio.h>

//using namespace matlab::engine;
int main() {

  // create matlab engine
  printf("starting matlab engine: \n");
  std::unique_ptr<matlab::engine::MATLABEngine> engine = matlab::engine::startMATLAB();
  printf("GOOD!\n");

  // linear index 
  std::vector<double> cppData{4, 8, 12, 16, 20, 24};

  // 2 X 3 matlab data array
  printf("creating data\n");
  matlab::data::ArrayFactory factory;
  auto inputArray = factory.createArray({2, 3}, cppData.cbegin(), cppData.cend());

  // pass data and call from matlab
  printf("evaluating\n");
  auto result = engine->feval(u"sqrt", inputArray);
  printf("result: %0.2f\n",result);

  // call sqrt with native types
  double result2 = engine->feval<double>(u"sqrt",double(27));
  printf("result2: %0.2f\n",result2);


  // return
  return 0;
}
