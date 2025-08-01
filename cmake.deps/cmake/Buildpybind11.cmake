# build pybind11
# pybind11 options

set(pybind11_CMAKE_ARGS
)

get_externalproject_options(pybind11 ${DEPS_IGNORE_SHA})
ExternalProject_Add(pybind11
    DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}/pybind11
    SOURCE_DIR ${DEPS_BUILD_DIR}/src/pybind11

    #LIST_SEPARATOR | # alternate list separator
    CMAKE_ARGS ${DEPS_CMAKE_ARGS} ${pybind11_CMAKE_ARGS}
    ${EXTERNALPROJECT_OPTIONS}

)


