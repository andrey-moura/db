#Require a minimum version
cmake_minimum_required(VERSION 3.10)

project(uva-db)

# Postgres is optional

set(PQXX /usr/include/pqxx)

if(EXISTS PQXX)
	find_library(PQXX_LIB pqxx)
	find_library(PQ_LIB pq)

	include_directories(PQXX)
endif()

message(status "Found PQXX_LIB: ${pqxx}")

add_library(uva-db
	${CMAKE_CURRENT_LIST_DIR}/src/sqlite3.c
	
	${CMAKE_CURRENT_LIST_DIR}/src/basic_active_record.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/basic_active_record_column.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/db.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/postgres.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sqlite3.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(uva-db Threads::Threads ${CMAKE_DL_LIBS} uva-string uva-core uva-console uva-time)

if(PQXX_LIB)
	target_link_libraries(uva-db ${PQXX_LIB} ${PQ_LIB})
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR})

include("${CMAKE_CURRENT_LIST_DIR}/sample/CMakeLists.txt")

if(TARGET cspec)
	#create tests for project
	cspec_configure("uva-db" ${CMAKE_CURRENT_LIST_DIR} "uva-string;uva-db;uva-faker;uva-core;uva-console")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/database/CMakeLists.txt")
