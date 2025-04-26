cmake_minimum_required(VERSION 3.10)
project(as68k C)

# Set the C standard and optimization flags
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall")

# List your source files
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