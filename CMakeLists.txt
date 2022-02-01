message(STATUS ${CMAKE_CURRENT_LIST_DIR})
add_library(uva-database
	${CMAKE_CURRENT_LIST_DIR}/src/sqlite3.c
	${CMAKE_CURRENT_LIST_DIR}/src/database.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(uva-database Threads::Threads ${CMAKE_DL_LIBS})

include_directories(${CMAKE_CURRENT_LIST_DIR})
