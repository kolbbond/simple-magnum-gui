// LuaWrapper.cpp

#include "LuaWrapper.hh"
#include "lua.h"

LuaWrapper::LuaWrapper() {
  // constructor

  // create engine here!
  lua_State *engine = luaL_newstate();
  if (engine == NULL) {
    printf("Error init with lua\n");
    assert(engine == NULL);
  }

  // open libs
  // luaopenL_base(engine); // for print
  luaL_openlibs(engine);

  // then set
  this->set_engine(engine);
}

void LuaWrapper::set_engine(lua_State *L) {
  // set engine pointer into object
  this->engine_ = L;
}

lua_return LuaWrapper::dostring(std::string str) {
  // wrapper for lua dostring

  // check engine set

  // we need a const char*
  const char *str_in = str.c_str();
  const char *str_out;

  // intercept
  printf("str_in: %s\n", str_in);
  if (str == "stackdump") {

    printf("dumping stack\n");
    stackDump(this->engine_);
    last_ret_.code = 0;
    last_ret_.str = str_out;
    return last_ret_;

  } else {

    // run our ret
    const int ret = luaL_dostring(this->engine_, str_in);

    // check return
    if (ret != LUA_OK) {
      // we could assert here or try/catch for example

      // put the lua message in here
      str_out = lua_tostring(this->engine_, -1);

      // pop message
      lua_pop(engine_, 1);

      printf("Error: %s\n", str_out);

    } else if (ret == LUA_OK) {
      str_out = "";

    } else {
      printf("what is this lua flag?\n");
    }

    // store last return status
    last_ret_.code = ret;
    last_ret_.str = str_out;

    // return the lua status
    return last_ret_;
  }
}

void LuaWrapper::dofile(std::string file) {
  // wrapper for lua dofile

  // check engine set

  // we need a const char*
  const char *str_in = file.c_str();

  luaL_dofile(this->engine_, str_in);
}

void LuaWrapper::close() {
  // close lua engine
  lua_close(this->engine_);
}

ShLuaWrapperPr LuaWrapper::create() {
  // factory
  return std::make_shared<LuaWrapper>();
}

void LuaWrapper::stackDump(lua_State *L) {
  int i;
  int top = lua_gettop(L);

  char *str_out;

  for (i = 1; i <= top; i++) { /* repeat for each level */
    int t = lua_type(L, i);
    switch (t) {

    case LUA_TSTRING: /* strings */
      printf("`%s'", lua_tostring(L, i));
      break;

    case LUA_TBOOLEAN: /* booleans */
      printf(lua_toboolean(L, i) ? "true" : "false");
      break;

    case LUA_TNUMBER: /* numbers */
      printf("%g", lua_tonumber(L, i));
      break;

    default: /* other values */
      printf("%s", lua_typename(L, t));
      break;
    }
    printf("  "); /* put a separator */
  }
  printf("\n"); /* end the listing */
}
