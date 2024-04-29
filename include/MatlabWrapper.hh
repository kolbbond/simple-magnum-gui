// MatlabWrapper.hh

#ifndef MatlabWrapper_hh
#define MatlabWrapper_hh

// includes

// Matlab is written in C
#include "MatlabEngine.hpp"
#include "MatlabEngine/engine_exception.hpp"
#include "MatlabEngine/engine_factory.hpp"
#include "MatlabEngine/matlab_engine.hpp"
#include "MatlabExecutionInterface/util.hpp"
#include "MatlabExecutionInterface/value_future.hpp"

#include <string>
#include <assert.h>

class MatlabWrapper {
    
    // properties
    protected:
        lua_State* engine_;

    // methods
    public:
        // default constructor
        MatlabWrapper(); 

        // set engine 
        void set_engine(lua_State*);

        // lua wrappers
        void dostring(std::string);
        void dofile(std::string);

        // close
        void close();


};

#endif
