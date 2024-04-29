// LuaWrapper.hh

#ifndef LuaWrapper_hh
#define LuaWrapper_hh

// includes

// Lua is written in C
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <assert.h>
#include <memory>
#include <string>

typedef std::shared_ptr<class LuaWrapper> ShLuaWrapperPr;
struct lua_return {
  int code;
  std::string str;
};

class LuaWrapper {

  // properties
protected:
  lua_State *engine_;

  // last return statement and success flag
  lua_return last_ret_;

  // methods
public:
  // default constructor
  LuaWrapper();

  // set engine
  void set_engine(lua_State *);

  // lua wrappers
  lua_return dostring(std::string);
  void dofile(std::string);

  // close
  void close();

  static ShLuaWrapperPr create();

  static void stackDump(lua_State *L);
};

#endif
