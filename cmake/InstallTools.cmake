# The examples as command-line tools (Linux). Each is its own install component, left out of a plain
# `cmake --install`, so tools are picked at install time:
#   cmake --install build --component tool-fview --prefix ~/.local     (or: make install-tools)
# A tool lands as bin/<SMG_TOOL_PREFIX><name>, a link to lib/smg-tools/bin/<name>, beside private
# copies of the libraries and Magnum plugins it loads: the install outlives the build tree, and never
# writes a bare bin/<name> that could clobber another program.

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

get_property(_smg_tools GLOBAL PROPERTY SMG_EXAMPLE_TARGETS)
if(NOT _smg_tools)
    return()
endif()

set(SMG_TOOL_PREFIX "smg-" CACHE STRING "name prefix of installed tools: bin/<prefix><example>")
set(SMG_TOOLS_DIR "${CMAKE_INSTALL_LIBDIR}/smg-tools")

# where the build put the libraries the tools load, plus any prefix Magnum was found in
set(_smg_tool_lib_dirs "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
foreach(_prefix ${CMAKE_PREFIX_PATH})
    list(APPEND _smg_tool_lib_dirs "${_prefix}/lib" "${_prefix}/${CMAKE_INSTALL_LIBDIR}")
endforeach()
set(_smg_tool_plugin_dir "")
foreach(_dir ${_smg_tool_lib_dirs})
    if(NOT _smg_tool_plugin_dir AND IS_DIRECTORY "${_dir}/magnum/importers")
        set(_smg_tool_plugin_dir "${_dir}/magnum")
    endif()
endforeach()

set(_smg_tool_link_code [=[
set(_bin "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/@CMAKE_INSTALL_BINDIR@")
set(_exe "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/@SMG_TOOLS_DIR@/bin/@_t@")
file(MAKE_DIRECTORY "${_bin}")
file(RELATIVE_PATH _rel "${_bin}" "${_exe}")
message(STATUS "Linking: ${_bin}/@SMG_TOOL_PREFIX@@_t@ -> ${_rel}")
file(CREATE_LINK "${_rel}" "${_bin}/@SMG_TOOL_PREFIX@@_t@" SYMBOLIC)
list(APPEND CMAKE_INSTALL_MANIFEST_FILES "${_bin}/@SMG_TOOL_PREFIX@@_t@")
]=])

foreach(_t ${_smg_tools})
    # $ORIGIN: libraries beside it (the cpack examples package); $ORIGIN/..: lib/smg-tools
    set_property(TARGET ${_t} PROPERTY INSTALL_RPATH "$ORIGIN;$ORIGIN/..")
    install(TARGETS ${_t} RUNTIME DESTINATION ${SMG_TOOLS_DIR}/bin COMPONENT tool-${_t} EXCLUDE_FROM_ALL)

    set(SMG_DEPLOY_EXECUTABLE "$<TARGET_FILE:${_t}>")
    set(SMG_DEPLOY_DIRECTORIES "${_smg_tool_lib_dirs}")
    set(SMG_DEPLOY_PLUGIN_DIR "${_smg_tool_plugin_dir}")
    configure_file(${CMAKE_CURRENT_LIST_DIR}/deploy_tool.cmake.in ${CMAKE_BINARY_DIR}/deploy_tool_${_t}.cmake.gen @ONLY)
    file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/deploy_tool_${_t}-$<CONFIG>.cmake INPUT ${CMAKE_BINARY_DIR}/deploy_tool_${_t}.cmake.gen)
    install(SCRIPT ${CMAKE_BINARY_DIR}/deploy_tool_${_t}-$<CONFIG>.cmake COMPONENT tool-${_t} EXCLUDE_FROM_ALL)

    string(CONFIGURE "${_smg_tool_link_code}" _code @ONLY)
    install(CODE "${_code}" COMPONENT tool-${_t} EXCLUDE_FROM_ALL)
endforeach()

string(REPLACE ";" " " _smg_tool_names "${_smg_tools}")
message(STATUS "smg tools: ${_smg_tool_names} (install: make install-tools TOOLS=\"...\")")
