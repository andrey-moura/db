#Require a minimum version
cmake_minimum_required(VERSION 3.10)

project(uva-database)

add_library(uva-database
	${CMAKE_CURRENT_LIST_DIR}/src/sqlite3.c
	${CMAKE_CURRENT_LIST_DIR}/src/database.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(uva-database Threads::Threads ${CMAKE_DL_LIBS} uva-string)

include_directories(${CMAKE_CURRENT_LIST_DIR})

include("${CMAKE_CURRENT_LIST_DIR}/sample/CMakeLists.txt")

#create tests for parent project
cspec_configure("uva-database" ${CMAKE_CURRENT_LIST_DIR} "uva-string;uva-database;uva-faker;uva-core;uva-console")