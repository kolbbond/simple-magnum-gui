# License notices of smg and everything it redistributes; MIT/zlib/OFL require them to travel with
# the binaries. Shared by the cpack examples package and the Linux tools install.
#   smg_install_licenses(<destination> <component> [EXCLUDE_FROM_ALL])

set(_smg_root ${CMAKE_CURRENT_LIST_DIR}/..)
set(SMG_LICENSES
    "smg|${_smg_root}/LICENSE"
    "corrade|${_smg_root}/external/corrade/COPYING"
    "magnum|${_smg_root}/external/magnum/COPYING"
    "magnum-integration|${_smg_root}/external/magnum-integration/COPYING"
    "magnum-plugins|${_smg_root}/external/magnum-plugins/COPYING"
    "imgui|${_smg_root}/external/magnum-integration/src/MagnumExternal/ImGui/LICENSE.txt"
    "implot|${_smg_root}/external/implot/LICENSE"
    "implot3d|${_smg_root}/external/implot3d/LICENSE"
    "imguifiledialog|${_smg_root}/external/imguifiledialog/LICENSE"
    "magnum-bloom|${_smg_root}/external/bloom/PROVENANCE.md")
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    list(APPEND SMG_LICENSES "sdl2|${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/sdl2/copyright")
endif()
set(SMG_FONT_LICENSES_DIR ${_smg_root}/assets/LICENSES)

function(smg_install_licenses destination component)
    set(_extra ${ARGN})
    foreach(_entry ${SMG_LICENSES})
        string(REPLACE "|" ";" _pair "${_entry}")
        list(GET _pair 0 _name)
        list(GET _pair 1 _file)
        if(EXISTS "${_file}")
            get_filename_component(_ext "${_file}" LAST_EXT)
            if(NOT _ext)
                set(_ext ".txt")
            endif()
            install(FILES "${_file}" DESTINATION ${destination} RENAME "${_name}${_ext}" COMPONENT ${component} ${_extra})
        else()
            message(STATUS "smg: license file missing for ${_name}: ${_file}")
        endif()
    endforeach()
    install(DIRECTORY ${SMG_FONT_LICENSES_DIR}/ DESTINATION ${destination}/fonts COMPONENT ${component} ${_extra})
endfunction()
