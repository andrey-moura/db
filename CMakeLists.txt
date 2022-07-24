#Require a minimum version
cmake_minimum_required(VERSION 3.10)

project(uva-database)

# Minimun C++ version
set(CMAKE_CXX_STANDARD 23)

add_library(uva-database
	${CMAKE_CURRENT_LIST_DIR}/src/sqlite3.c
	${CMAKE_CURRENT_LIST_DIR}/src/database.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(uva-database Threads::Threads ${CMAKE_DL_LIBS})

include_directories(${CMAKE_CURRENT_LIST_DIR})
