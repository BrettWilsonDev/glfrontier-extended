cmake_minimum_required(VERSION 3.22)

# Get project name from the folder name
set(PROJECT_NAME GLFrontier)
project(${PROJECT_NAME} C)

set(CMAKE_C_STANDARD 99)

##########################################################################################
# Configurable options
##########################################################################################

# library toggles
option(USE_SDL2 "Use SDL2" ON)
option(USE_PHYSFS "Use PHYSFS" ON)
option(USE_GLAD "Use GLAD" ON)

# build options
option(BUILD_SDL_SHARED_LIB "Build SDL as a shared library" OFF)
option(USE_GL_RENDERER "Use OpenGL renderer instead of software based SDL internal renderer" ON)

# set(USE_GL_RENDERER OFF CACHE BOOL "Use OpenGL renderer instead of software based SDL internal renderer" FORCE)

##########################################################################################
# Android: disable GLAD, use GLES3 directly
##########################################################################################

if(ANDROID)
    set(USE_GLAD OFF CACHE BOOL "" FORCE)
endif()

##########################################################################################
# MSVC runtime linking
##########################################################################################

# this is here to ensure the runtime is linked staticly for msvc
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
endif()

##########################################################################################
# Platform Configuration
##########################################################################################

if(NOT DEFINED PLATFORM)
    set(PLATFORM "default")
endif()

##########################################################################################
# Add dependencies with FetchContent
##########################################################################################

# libraries array to be linked
set(LIBRARIES ${PROJECT_NAME})

include(FetchContent)
function(add_git_dependency libName gitURL gitTag)

    FetchContent_Declare(${libName}
        GIT_REPOSITORY ${gitURL}
        GIT_TAG ${gitTag}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    if(NOT ("${libName}" STREQUAL "libname"))
        FetchContent_MakeAvailable(${libName})
    endif()

endfunction()

##########################################################################################
# SDL2 Setup
##########################################################################################

if(USE_SDL2 AND NOT ${PLATFORM} STREQUAL "Web")
    set(CMAKE_USE_FORWARD_SLASHES ON)

    set(SDL2IMAGE_INSTALL OFF)
    set(BUILD_SHARED_LIBS FALSE)

    if(BUILD_SDL_SHARED_LIB)
        set(BUILD_SHARED_LIBS TRUE)
        option(SDL_SHARED "Build SDL as a shared library" ON)
    endif()

    set(SDL_AUDIO ON CACHE BOOL "" FORCE)
    set(SDL_VIDEO ON CACHE BOOL "" FORCE)
    set(SDL_TIMERS ON CACHE BOOL "" FORCE)
    set(SDL_THREADS ON CACHE BOOL "" FORCE)
    set(SDL_RENDER ON CACHE BOOL "" FORCE)
    set(SDL_EVENTS ON CACHE BOOL "" FORCE)
    set(SDL_CPUINFO ON CACHE BOOL "" FORCE)
    set(SDL_ATOMIC ON CACHE BOOL "" FORCE)

    if(ANDROID)
        set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
        set(SDL_HAPTIC ON CACHE BOOL "" FORCE)
        set(SDL_SENSOR ON CACHE BOOL "" FORCE)
        set(SDL_POWER ON CACHE BOOL "" FORCE)
        set(SDL_FILE OFF CACHE BOOL "" FORCE)
        set(SDL_FILESYSTEM OFF CACHE BOOL "" FORCE)
        set(SDL_MISC OFF CACHE BOOL "" FORCE)
        set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
        set(SDL_HIDAPI_JOYSTICK OFF CACHE BOOL "" FORCE)
    else()
        set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
        set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
        set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
        set(SDL_POWER OFF CACHE BOOL "" FORCE)
        set(SDL_FILE OFF CACHE BOOL "" FORCE)
        set(SDL_FILESYSTEM OFF CACHE BOOL "" FORCE)
        set(SDL_MISC OFF CACHE BOOL "" FORCE)
    endif()

    set(SDL2MAIN OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_DEMOS OFF CACHE BOOL "" FORCE)

    set(dep SDL2)
    add_git_dependency(
        ${dep}
        https://github.com/libsdl-org/SDL.git
        SDL2
    )

    FetchContent_GetProperties(${dep})
    if(NOT ${dep}_POPULATED)
    endif()

    if(ANDROID)
        list(APPEND LIBRARIES SDL2-static)
    else()
        list(APPEND LIBRARIES SDL2main)
        if(BUILD_SDL_SHARED_LIB)
            list(APPEND LIBRARIES SDL2)
        else()
            list(APPEND LIBRARIES SDL2-static)
        endif()
    endif()

    include_directories(RECURSE ${sdl2_SOURCE_DIR}/include/SDL2/)
endif()

if((USE_SDL2) AND (${PLATFORM} STREQUAL "Web"))
    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG SDL2
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    if(NOT SDL2_POPULATED)
        FetchContent_Populate(SDL2)
    endif()

    include_directories(RECURSE ${sdl2_SOURCE_DIR}/include)
endif()

##########################################################################################
# Physfx Setup
##########################################################################################

if(USE_PHYSFS)
    set(PHYSFS_STATIC_BUILD ON CACHE BOOL "Build shared library")
    set(PHYSFS_BUILD_SHARED OFF CACHE BOOL "Build shared library")
    set(PHYSFS_BUILD_TEST OFF CACHE BOOL "Build tests")
    set(PHYSFS_BUILD_DOCS OFF CACHE BOOL "Build documentation")

    set(dep physfs)
    add_git_dependency(
        ${dep}
        https://github.com/icculus/physfs.git
        main
    )

    FetchContent_GetProperties(${dep})

    list(APPEND LIBRARIES PhysFS::PhysFS-static)
endif()

##########################################################################################
# Glad Setup
##########################################################################################

if(${PLATFORM} STREQUAL "Web")
    # set(USE_GL_RENDERER ON CACHE BOOL "Use OpenGL renderer instead of software based SDL internal renderer" FORCE) # web dose not support gl for now
    set(USE_GLAD OFF CACHE BOOL "Use Glad for OpenGL bindings" FORCE)
endif()

if(USE_GL_RENDERER)
    if(USE_GLAD)
        set(dep glad)
        add_git_dependency(
            ${dep}
            https://github.com/Dav1dde/glad.git
            master
        )

        FetchContent_GetProperties(${dep})
        if(NOT ${dep}_POPULATED)
            FetchContent_Populate(${dep})

            set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile")
            set(GLAD_API "gl=" CACHE STRING "API type/version pairs, like \"gl=3.2,gles=\", no version means latest")
            set(GLAD_GENE*RATOR "c" CACHE STRING "Language to generate the binding for")

            find_package(OpenGL REQUIRED)

            add_subdirectory(${glad_SOURCE_DIR} ${glad_BINARY_DIR})
        endif()

        # add the dependency here to be linked
        list(APPEND LIBRARIES ${dep})
    endif()
endif()

##########################################################################################
# Android: link GLES3 + EGL instead of GLAD
##########################################################################################

if(ANDROID AND USE_GL_RENDERER)
    find_library(GLES3_LIB GLESv3)
    find_library(EGL_LIB EGL)
    find_library(ANDROID_LIB android)
    find_library(LOG_LIB log)
    list(APPEND LIBRARIES ${GLES3_LIB} ${EGL_LIB} ${ANDROID_LIB} ${LOG_LIB})
endif()

##########################################################################################
# nuklear Setup
##########################################################################################

#define USE_NK
add_definitions(-DUSE_NK)

if(USE_GL_RENDERER)
    add_library(nuklear OBJECT ${CMAKE_CURRENT_LIST_DIR}/vendor/nuklear/nuklear_sdl_gl3_impl.c)
    if(USE_GLAD)
        target_link_libraries(nuklear PUBLIC glad)
    endif()
else()
    add_library(nuklear OBJECT ${CMAKE_CURRENT_LIST_DIR}/vendor/nuklear/nuklear_sdl_impl.c)
endif()
target_include_directories(nuklear PUBLIC
    ${sdl2_SOURCE_DIR}/include
    $<BUILD_INTERFACE:${glad_BINARY_DIR}/include>
)

list(APPEND LIBRARIES nuklear)

##########################################################################################
# glutess Setup
##########################################################################################

add_library(glutess OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/dict.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/geom.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/memalloc.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/mesh.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/normal.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/priorityq.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/render.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/sweep.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/tess.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/tessellate.c
    ${CMAKE_CURRENT_LIST_DIR}/vendor/glutess/tessmono.c
)
list(APPEND LIBRARIES glutess)

##########################################################################################
# FE2 m68k emulator setup
##########################################################################################

# include the host and m68000 headers
include_directories(${CMAKE_CURRENT_LIST_DIR})

# add music support
add_definitions(-DOGG_MUSIC)

# build fe2 to use little endian
add_definitions(-DLITTLE_ENDIAN)

# disable the warnings flood
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything")
endif()

if(${PLATFORM} STREQUAL "Web")
    # list(APPEND LIBRARIES ${CMAKE_CURRENT_LIST_DIR}/fe2/web/libfe2_emscripten_part_two.a)
    # list(APPEND LIBRARIES ${CMAKE_CURRENT_LIST_DIR}/fe2/web/libfe2_emscripten_part_one.a)

    list(APPEND LIBRARIES ${CMAKE_CURRENT_LIST_DIR}/fe2/web/libfe2_emscripten_async_part_two.a)
    list(APPEND LIBRARIES ${CMAKE_CURRENT_LIST_DIR}/fe2/web/libfe2_emscripten_async_part_one.a)

    # add_library(fe2_part_one STATIC fe2/fe2.s.c)
    # target_compile_options(fe2_part_one PRIVATE -DPART1 -O0)
    # target_link_options(fe2_part_one PRIVATE -s ASYNCIFY)

    # add_library(fe2_part_two STATIC fe2/fe2.s.c)
    # target_compile_options(fe2_part_two PRIVATE -DPART2 -O0)
    # target_link_options(fe2_part_two PRIVATE -s ASYNCIFY)

    # add_library(fe2_part_one STATIC fe2/fe2.s.c)
    # target_compile_options(fe2_part_one PRIVATE -DPART1 -O0)
    # list(APPEND LIBRARIES fe2_part_one)

    # add_library(fe2_part_two STATIC fe2/fe2.s.c)
    # target_compile_options(fe2_part_two PRIVATE -DPART2 -O0)
    # list(APPEND LIBRARIES fe2_part_two)
else()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        add_library(fe2_part_one OBJECT fe2/fe2.s.c)
        target_compile_options(fe2_part_one PRIVATE -DPART1)
        list(APPEND LIBRARIES fe2_part_one)

        add_library(fe2_part_two OBJECT fe2/fe2.s.c)
        target_compile_options(fe2_part_two PRIVATE -DPART2)
        list(APPEND LIBRARIES fe2_part_two)
    else()
        add_library(fe2_part_one OBJECT fe2/fe2.s.c)
        target_compile_options(fe2_part_one PRIVATE -DPART1 -O1 -fomit-frame-pointer -fcommon)
        list(APPEND LIBRARIES fe2_part_one)

        add_library(fe2_part_two OBJECT fe2/fe2.s.c)
        target_compile_options(fe2_part_two PRIVATE -DPART2 -O0 -fomit-frame-pointer -fcommon)
        list(APPEND LIBRARIES fe2_part_two)
    endif()

endif()
# build using OpenGL if not build using sdl renderer as a fall back
if(USE_GL_RENDERER)
    add_definitions(-DWITH_GL)
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR}/fe2)
include_directories(${CMAKE_CURRENT_LIST_DIR}/fe2/audio)

##########################################################################################
# Project executable setup
##########################################################################################

file(GLOB_RECURSE HEADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/*.h"
)
set(HEADER_DIRS "")
foreach(HEADER_FILE ${HEADER_FILES})
    get_filename_component(DIR "${HEADER_FILE}" DIRECTORY)
    list(APPEND HEADER_DIRS "${DIR}")
endforeach()

# Remove duplicate paths
list(REMOVE_DUPLICATES HEADER_DIRS)

# Add each unique header directory
foreach(DIR ${HEADER_DIRS})
    include_directories("${DIR}")
endforeach()

# Adding our source files
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/*.c")

# Define PROJECT_INCLUDE to be the path to the include directory of the project
set(PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src/")

# Declaring our executable
if(ANDROID)
    add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCES})
else()
    add_executable(${PROJECT_NAME})
    target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_INCLUDE})

##########################################################################################
# Compiler and linker flags
##########################################################################################

if(NOT ${PLATFORM} STREQUAL "Web" AND NOT ANDROID)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static -pthread")
        target_link_libraries(${LIBRARIES} -static-libgcc -static-libstdc++ -static -lpthread -Wl,-Bstatic -lstdc++)
        # target_link_libraries(${LIBRARIES})
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(${LIBRARIES} -static-libgcc -static-libstdc++ -static -pthread -Wl,-Bstatic -lstdc++)
        # target_link_libraries(${LIBRARIES})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel" OR CMAKE_C_COMPILER_ID STREQUAL "Intel")
        target_link_libraries(${LIBRARIES} -static-intel -lpthread -Wl,-Bstatic -lstdc++)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_link_libraries(${LIBRARIES})

        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
        )

        # dont forget to set  "cmake.configureSettings": { "CMAKE_BUILD_TYPE": "${buildType}" } in settings.json for vs code
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            message("-- console enabled")
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
        else()
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
        endif()
    else()
        target_link_libraries(${LIBRARIES})
    endif()
else()
    target_link_libraries(${LIBRARIES})
endif()

##########################################################################################
# Build flags and additional settings
##########################################################################################

add_definitions(-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

# === Debug / Release flags ===
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -DDEBUG")
    message("-- Debug build: console enabled")
else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
    if(WIN32)
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
        endif()
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
        endif()
    endif()
endif()

# for web build when using emscripten
# === Asyncify settings (for web build) ===
if(ASYNCIFY)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ASYNCIFY")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s TOTAL_STACK=8MB")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ASYNCIFY_STACK_SIZE=131072") # 128KB is usually enough

    # Memory settings tuned for mobile + desktop balance
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s INITIAL_MEMORY=128MB")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s MAXIMUM_MEMORY=512MB") # Safe realistic max for most mobiles
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
endif()

# === Common compiler flags ===
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# === Web / Emscripten specific settings ===
if(${PLATFORM} STREQUAL "Web")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")

    # Optimization for size + speed on web (better than -Os in many game cases)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")

    if(USE_SDL2)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
            -s USE_SDL=2 \
            -s USE_SDL_IMAGE=2 \
            -s MAX_WEBGL_VERSION=2 \
            -s FULL_ES3=1 \
            --use-preload-plugins \
            -s FORCE_FILESYSTEM=1 \
            -lidbfs.js \
            -s GL_UNSAFE_OPTS=0 \
            --shell-file ../vendor/minshell.html")
    endif()
endif()

# Ensure that hot-reload is enabled for VS
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC" OR CMAKE_C_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /ZI")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
endif()
