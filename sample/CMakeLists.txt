#Require a minimum version
cmake_minimum_required(VERSION 3.10)
project(database-sample)
set (CMAKE_CXX_STANDARD 20)
include(../CMakeLists.txt)
add_executable(database-sample main.cpp)
target_link_libraries(database-sample uva-database)