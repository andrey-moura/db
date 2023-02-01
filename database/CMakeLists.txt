#Require a minimum version
cmake_minimum_required(VERSION 3.10)

project(database)

include(${CONSOLE_ROOT_DIR}/console.cmake)

add_console_app(database)

target_link_libraries(database uva-console uva-core uva-routing uva-file)

add_custom_command(TARGET database
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:database> ${CMAKE_SOURCE_DIR}/bin/database)

