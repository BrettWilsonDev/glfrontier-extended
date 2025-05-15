cmake_minimum_required(VERSION 3.10)
project(as68k C)

# Set the C standard and optimization flags
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall")

set(SOURCES
    output.c
    output_c.c
    output_i386.c
    as68k.c
    dict.c
)

message(STATUS "Building as68k tool...")

# Add the executable target
add_executable(as68k ${SOURCES})

set(script_name "build-fe2-bin-c.bat")
# Windows custom target
add_custom_target(build_script_windows
    COMMAND ${CMAKE_COMMAND} -E echo "@echo off" > ${CMAKE_BINARY_DIR}/${script_name}.bat VERBATIM
    COMMAND ${CMAKE_COMMAND} -E echo "call \"%~dp0as68k\" fe2.s.cpp --output-c fe2.s" >> ${CMAKE_BINARY_DIR}/${script_name}.bat VERBATIM
    COMMAND ${CMAKE_COMMAND} -E echo "pause" >> ${CMAKE_BINARY_DIR}/${script_name}.bat VERBATIM
    COMMENT "Writing build-fe2-c.bat (Windows)"
)
# Unix/Linux custom target
add_custom_target(build_script_unix
    COMMAND ${CMAKE_COMMAND} -E echo "#!/bin/sh" > ${CMAKE_BINARY_DIR}/${script_name}.sh VERBATIM
    COMMAND ${CMAKE_COMMAND} -E echo "./as68k fe2.s.cpp --output-c fe2.s" >> ${CMAKE_BINARY_DIR}/${script_name}.sh VERBATIM
    COMMAND ${CMAKE_COMMAND} -E echo "read -p \"Press enter to continue...\"" >> ${CMAKE_BINARY_DIR}/${script_name}.sh VERBATIM
    COMMAND chmod +x ${CMAKE_BINARY_DIR}/${script_name}.sh
    COMMENT "Writing build-fe2-c.sh (Unix/Linux)"
)
if(WIN32)
    add_dependencies(as68k build_script_windows)
else()
    add_dependencies(as68k build_script_unix)
endif()