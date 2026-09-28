# Out-of-tree consumer: builds against an *installed* smg via find_package only,
# the way downstream projects do. Configure with -DCMAKE_PREFIX_PATH=<smg prefix>.
cmake_minimum_required(VERSION 3.16)
project(smg-consumer LANGUAGES CXX)

find_package(smg 0.1 REQUIRED)

add_executable(smg_consumer main.cpp)
target_link_libraries(smg_consumer PRIVATE smg::smg)

enable_testing()
add_test(NAME smg_consumer COMMAND smg_consumer)
