// LuaWrapper.cpp

#include "LuaWrapper.hh"

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

void LuaWrapper::dostring(std::string str) {
  // wrapper for lua dostring

  // check engine set

  // we need a const char*
  const char *str_in = str.c_str();

  // run our ret
  const int ret = luaL_dostring(this->engine_, str_in);

  // check return
  if (ret != LUA_OK) {
    printf("Error: %s\n", lua_tostring(this->engine_, -1));
    lua_pop(engine_, 1); // pop error message
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
