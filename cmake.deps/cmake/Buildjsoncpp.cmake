# jsoncpp options
# from bash script
set(jsoncpp_CMAKE_ARGS
  )

get_externalproject_options(jsoncpp ${DEPS_IGNORE_SHA})
ExternalProject_Add(jsoncpp
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}/jsoncpp
  SOURCE_DIR ${DEPS_BUILD_DIR}/src/jsoncpp
  CMAKE_ARGS ${DEPS_CMAKE_ARGS} ${jsoncpp_CMAKE_ARGS}
  ${EXTERNALPROJECT_OPTIONS})



