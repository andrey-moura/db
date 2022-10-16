#Require a minimum version
cmake_minimum_required(VERSION 3.10)

project(database)

add_executable(database ${CMAKE_CURRENT_LIST_DIR}/src/database_tool.cpp)

target_link_libraries(database uva-database)

add_custom_command(TARGET database 
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:database> ${CMAKE_SOURCE_DIR}/bin)