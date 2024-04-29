// LuaWrapper.hh

#ifndef LuaWrapper_hh
#define LuaWrapper_hh

// includes

// Lua is written in C
extern "C"{ 
#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>
}

#include <string>
#include <assert.h>

class LuaWrapper {
    
    // properties
    protected:
        lua_State* engine_;

    // methods
    public:
        // default constructor
        LuaWrapper(); 

        // set engine 
        void set_engine(lua_State*);

        // lua wrappers
        void dostring(std::string);
        void dofile(std::string);

        // close
        void close();


};

#endif
