# Out-of-tree consumer: builds against an *installed* smg via find_package only,
# the way downstream projects do. Configure with -DCMAKE_PREFIX_PATH=<smg prefix>.
cmake_minimum_required(VERSION 3.16)
project(smg-consumer LANGUAGES CXX)

find_package(smg 0.1 REQUIRED)

# a stray system Magnum/Corrade would compile and link fine, then mismatch libsmg at runtime
get_filename_component(SMG_CONSUMER_PREFIX "${smg_DIR}/../../.." REALPATH)
foreach(_dir IN ITEMS MAGNUM_INCLUDE_DIR CORRADE_INCLUDE_DIR)
    get_filename_component(_real "${${_dir}}" REALPATH)
    string(FIND "${_real}" "${SMG_CONSUMER_PREFIX}/" _pos)
    if(NOT _pos EQUAL 0)
        message(FATAL_ERROR "${_dir}=${_real} is outside the smg prefix ${SMG_CONSUMER_PREFIX}")
    endif()
endforeach()

add_executable(smg_consumer main.cpp)
target_link_libraries(smg_consumer PRIVATE smg::smg)
target_compile_definitions(smg_consumer PRIVATE SMG_CONSUMER_PREFIX="${SMG_CONSUMER_PREFIX}")

enable_testing()
add_test(NAME smg_consumer COMMAND smg_consumer)
