project(uva-database)

set (CMAKE_CXX_STANDARD 20)

add_library(uva-database
	${CMAKE_CURRENT_LIST_DIR}/src/sqlite3.c
	${CMAKE_CURRENT_LIST_DIR}/src/database.cpp
)

# cspec
include(include/cspec/CMakeLists.txt)

find_package(Threads REQUIRED)
target_link_libraries(uva-database Threads::Threads ${CMAKE_DL_LIBS})

include_directories(${CMAKE_CURRENT_LIST_DIR})
