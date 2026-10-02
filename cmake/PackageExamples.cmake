# Standalone package of the examples: `cpack` in the build dir makes a zip (and an NSIS installer
# on Windows) holding bin/<example>.exe plus every DLL and Magnum plugin they load. Only the
# `examples` component is packaged, so headers, import libs and CMake configs stay out.

get_property(_smg_examples GLOBAL PROPERTY SMG_EXAMPLE_TARGETS)
if(NOT _smg_examples)
    return()
endif()

# where to look for the examples' DLLs: our own output, every prefix we found packages in, vcpkg
set(_smg_dll_dirs "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
foreach(_prefix ${CMAKE_PREFIX_PATH})
    file(TO_CMAKE_PATH "${_prefix}" _prefix) # a -D path from cmd.exe has backslashes
    list(APPEND _smg_dll_dirs "${_prefix}/bin" "${_prefix}/lib")
endforeach()
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    list(APPEND _smg_dll_dirs "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
endif()

# Magnum's runtime plugins (the window icon needs AnyImageImporter + StbImageImporter); a shared
# Magnum looks for them in magnum/ next to its DLLs
set(_smg_plugin_dir "")
foreach(_dir ${_smg_dll_dirs})
    if(NOT _smg_plugin_dir AND IS_DIRECTORY "${_dir}/magnum/importers")
        set(_smg_plugin_dir "${_dir}/magnum")
    endif()
endforeach()
if(_smg_plugin_dir)
    install(DIRECTORY "${_smg_plugin_dir}/" DESTINATION ${CMAKE_INSTALL_BINDIR}/magnum COMPONENT examples
        FILES_MATCHING PATTERN "*.dll" PATTERN "*.so" PATTERN "*.conf")
else()
    message(STATUS "smg examples package: no Magnum plugin dir found, window icons will be skipped")
endif()

# resolve the DLL closure at install time (the executables exist by then)
set(_smg_example_files "")
foreach(_t ${_smg_examples})
    list(APPEND _smg_example_files "$<TARGET_FILE:${_t}>")
endforeach()
set(SMG_DEPLOY_EXECUTABLES "${_smg_example_files}")
set(SMG_DEPLOY_DIRECTORIES "${_smg_dll_dirs}")
configure_file(${CMAKE_CURRENT_LIST_DIR}/deploy_examples.cmake.in
    ${CMAKE_BINARY_DIR}/deploy_examples.cmake.gen @ONLY)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/deploy_examples-$<CONFIG>.cmake
    INPUT ${CMAKE_BINARY_DIR}/deploy_examples.cmake.gen)
install(SCRIPT ${CMAKE_BINARY_DIR}/deploy_examples-$<CONFIG>.cmake COMPONENT examples)

# MSVC runtime, so it runs without the VC++ redistributable installed
if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT examples)
    include(InstallRequiredSystemLibraries)
endif()

install(FILES ${CMAKE_CURRENT_LIST_DIR}/../packaging/README.txt DESTINATION . COMPONENT examples)

# CPack
set(CPACK_PACKAGE_NAME "smg-examples")
set(CPACK_PACKAGE_VENDOR "kolbbond")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "simple-magnum-gui example programs")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "smg-examples")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/package")
set(CPACK_COMPONENTS_ALL examples)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON) # else the zip ignores CPACK_COMPONENTS_ALL
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON) # unzip into smg-examples-<ver>-win64/
set(CPACK_COMPONENT_EXAMPLES_DISPLAY_NAME "Examples")
set(CPACK_COMPONENT_EXAMPLES_REQUIRED ON)

# Start-menu entries: exe name;label pairs
set(CPACK_PACKAGE_EXECUTABLES "")
foreach(_t ${_smg_examples})
    list(APPEND CPACK_PACKAGE_EXECUTABLES ${_t} "smg ${_t}")
endforeach()

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "smg examples ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "smg examples")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "${CMAKE_INSTALL_BINDIR}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_LIST_DIR}/../assets/smg.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_LIST_DIR}/../assets/smg.ico")
    set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON) # the repo has no LICENSE file yet
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
