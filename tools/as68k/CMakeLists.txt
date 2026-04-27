cmake_minimum_required(VERSION 3.10)
project(as68k C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall")

set(SOURCES
    output.c
    output_c.c
    output_i386.c
    as68k.c
    dict.c
)

add_executable(as68k ${SOURCES})

set(script_name "build-fe2-bin-c")

if(WIN32)
    # This runs AFTER as68k is built, putting everything in the same folder
    add_custom_command(TARGET as68k POST_BUILD
        # 1. Copy xxd.exe
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/xxd.exe" "$<TARGET_FILE_DIR:as68k>/xxd.exe"
        
        # 2. Create the Batch file line by line
        COMMAND ${CMAKE_COMMAND} -E echo "@echo off" > "$<TARGET_FILE_DIR:as68k>/${script_name}.bat"
        COMMAND ${CMAKE_COMMAND} -E echo "pushd %~dp0" >> "$<TARGET_FILE_DIR:as68k>/${script_name}.bat"
        COMMAND ${CMAKE_COMMAND} -E echo "as68k.exe fe2.s.c --output-c fe2.s" >> "$<TARGET_FILE_DIR:as68k>/${script_name}.bat"
        COMMAND ${CMAKE_COMMAND} -E echo "xxd.exe -i fe2.s.bin fe2_bin.h" >> "$<TARGET_FILE_DIR:as68k>/${script_name}.bat"
        COMMAND ${CMAKE_COMMAND} -E echo "popd" >> "$<TARGET_FILE_DIR:as68k>/${script_name}.bat"
        COMMAND ${CMAKE_COMMAND} -E echo "pause" >> "$<TARGET_FILE_DIR:as68k>/${script_name}.bat"
        
        COMMENT "Copying xxd.exe and generating build script in output directory"
        VERBATIM
    )
else()
    # Unix equivalent
    add_custom_command(TARGET as68k POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "#!/bin/sh" > "$<TARGET_FILE_DIR:as68k>/${script_name}.sh"
        COMMAND ${CMAKE_COMMAND} -E echo "cd \"$(dirname \"$0\")\"" >> "$<TARGET_FILE_DIR:as68k>/${script_name}.sh"
        COMMAND ${CMAKE_COMMAND} -E echo "./as68k fe2.s.c --output-c fe2.s" >> "$<TARGET_FILE_DIR:as68k>/${script_name}.sh"
        COMMAND ${CMAKE_COMMAND} -E echo "xxd -i fe2.s.bin fe2_bin.h" >> "$<TARGET_FILE_DIR:as68k>/${script_name}.sh"
        COMMAND ${CMAKE_COMMAND} -E echo "read -p \"Press enter to continue...\"" >> "$<TARGET_FILE_DIR:as68k>/${script_name}.sh"
        COMMAND chmod +x "$<TARGET_FILE_DIR:as68k>/${script_name}.sh"
        VERBATIM
    )
endif()