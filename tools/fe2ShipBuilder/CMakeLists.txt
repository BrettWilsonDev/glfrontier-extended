set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
cmake_minimum_required(VERSION 3.22)

# FE2 Ship Builder: make ships and put them in the game.
# Built on its own (see build_fe2ShipBuilder.bat). The modded game binary
# (fe2/fe2_bin.h, fe2/fe2_labels.h) and the game font (src/screen_font.h)
# are compiled in, and everything is linked statically, so FE2ShipBuilder.exe
# runs on any machine by itself.
project(FE2ShipBuilder C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GLF_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# SDL2, glad and imgui come from vendor/ (nothing is downloaded)
include(${GLF_ROOT}/cmake/GlfDeps.cmake)

# SDL2 (static, only what the studio uses)
set(BUILD_SHARED_LIBS OFF)
foreach(sub VIDEO EVENTS TIMERS THREADS CPUINFO ATOMIC)
    set(SDL_${sub} ON CACHE BOOL "" FORCE)
endforeach()
foreach(sub AUDIO RENDER VULKAN OPENGLES JOYSTICK HAPTIC SENSOR POWER FILE FILESYSTEM MISC TEST EXAMPLES DEMOS)
    set(SDL_${sub} OFF CACHE BOOL "" FORCE)
endforeach()
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
glf_add_sdl2()

# glad: OpenGL 3.3 core
glf_add_glad()

# Dear ImGui, same copy as the game
glf_add_imgui()
target_link_libraries(imgui PUBLIC SDL2-static)

file(GLOB STUDIO_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(FE2ShipBuilder ${STUDIO_SOURCES})
target_include_directories(FE2ShipBuilder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GLF_ROOT}/src
    ${GLF_ROOT}/fe2)

set(STUDIO_LIBS imgui SDL2main SDL2-static glad)
if(WIN32)
    list(APPEND STUDIO_LIBS comdlg32 shell32)
endif()

if(MSVC)
    target_link_libraries(FE2ShipBuilder ${STUDIO_LIBS})
    set_target_properties(FE2ShipBuilder PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
else()
    target_compile_options(FE2ShipBuilder PRIVATE -Wall)
    target_link_libraries(FE2ShipBuilder ${STUDIO_LIBS} -static-libgcc -static-libstdc++ -static -pthread)
    if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(FE2ShipBuilder PRIVATE -mwindows) # no console window
    endif()
endif()
