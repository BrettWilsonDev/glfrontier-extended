set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
cmake_minimum_required(VERSION 3.22)

# FE2 Galaxy Atlas: the game's galaxy map, zoomable and searchable.
# Built on its own (see build_FE2GalaxyViewer.bat). Every star, name and system
# detail it shows comes from running the game's own code: the translated
# fe2/fe2_modded.s.c is compiled in (see src/galaxy/fe2vm.h). The atlas itself
# is shared with the game (src/galaxy).
# Linked statically, so FE2GalaxyViewer.exe runs on any machine by itself.
project(FE2GalaxyViewer C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GLF_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
function(add_git_dependency name url tag)
    FetchContent_Declare(${name}
        GIT_REPOSITORY ${url}
        GIT_TAG ${tag}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(${name})
endfunction()

# SDL2 (static, only what the tool uses)
set(BUILD_SHARED_LIBS OFF)
foreach(sub VIDEO EVENTS TIMERS THREADS CPUINFO ATOMIC)
    set(SDL_${sub} ON CACHE BOOL "" FORCE)
endforeach()
foreach(sub AUDIO RENDER JOYSTICK HAPTIC SENSOR POWER FILE FILESYSTEM MISC TEST EXAMPLES DEMOS)
    set(SDL_${sub} OFF CACHE BOOL "" FORCE)
endforeach()
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
add_git_dependency(SDL2 https://github.com/libsdl-org/SDL.git SDL2)

# glad 1.x, OpenGL 3.3 core
set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile")
set(GLAD_API "gl=" CACHE STRING "API type/version pairs, no version means latest")
set(GLAD_GENERATOR "c" CACHE STRING "Language to generate the binding for")
find_package(OpenGL REQUIRED)
add_git_dependency(glad https://github.com/Dav1dde/glad.git v0.1.36)

# Dear ImGui, same version as the game
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG="imconfig_galaxy.h")
target_link_libraries(imgui PUBLIC SDL2-static)

##########################################################################################
# The game code, callable (fe2vm.c)
##########################################################################################

# The translated game code has Start680x0_at (tools/as68k), which the atlas
# calls into (src/galaxy/fe2vm.c); compiled here as in the game.
set(FE2_GAME_C ${GLF_ROOT}/fe2/fe2_modded.s.c)
set(FE2VM_DEFS LITTLE_ENDIAN FE2_USE_MODDED=1 FE2GALAXY_TOOL)
set(FE2VM_INCLUDES ${GLF_ROOT} ${GLF_ROOT}/fe2 ${GLF_ROOT}/src ${GLF_ROOT}/src/galaxy)

# the game code is huge, so it is compiled in two halves, as in the game
add_library(fe2vm_part_one OBJECT ${FE2_GAME_C})
add_library(fe2vm_part_two OBJECT ${FE2_GAME_C})
target_compile_definitions(fe2vm_part_one PRIVATE PART1 ${FE2VM_DEFS})
target_compile_definitions(fe2vm_part_two PRIVATE PART2 ${FE2VM_DEFS})
foreach(part fe2vm_part_one fe2vm_part_two)
    target_include_directories(${part} PRIVATE ${FE2VM_INCLUDES})
endforeach()
if(NOT MSVC)
    target_compile_options(fe2vm_part_one PRIVATE -O1 -fomit-frame-pointer -fcommon -w)
    target_compile_options(fe2vm_part_two PRIVATE -O0 -fomit-frame-pointer -fcommon -w)
endif()

##########################################################################################
# FE2GalaxyViewer.exe
##########################################################################################

set(GALAXY_DIR ${GLF_ROOT}/src/galaxy)
add_executable(FE2GalaxyViewer
    app.cpp
    style.cpp
    ${GALAXY_DIR}/atlas.cpp
    ${GALAXY_DIR}/galaxy.cpp
    ${GALAXY_DIR}/map_view.cpp
    ${GALAXY_DIR}/fe2vm.c
    $<TARGET_OBJECTS:fe2vm_part_one>
    $<TARGET_OBJECTS:fe2vm_part_two>)
target_compile_definitions(FE2GalaxyViewer PRIVATE ${FE2VM_DEFS})
target_include_directories(FE2GalaxyViewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FE2VM_INCLUDES})

set(GALAXY_LIBS imgui SDL2main SDL2-static glad)
if(WIN32)
    list(APPEND GALAXY_LIBS shell32)
endif()

if(MSVC)
    target_link_libraries(FE2GalaxyViewer ${GALAXY_LIBS})
    set_target_properties(FE2GalaxyViewer PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
else()
    target_compile_options(FE2GalaxyViewer PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall>)
    target_link_libraries(FE2GalaxyViewer ${GALAXY_LIBS} -static-libgcc -static-libstdc++ -static -pthread)
    if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(FE2GalaxyViewer PRIVATE -mwindows) # no console window; --dump attaches to the parent's
    endif()
endif()
