# Third-party libraries, built from the stripped-down copies in vendor/
# (see vendor/README.md; nothing is downloaded at build time).
# Shared by the game and the tools (tools/fe2GalaxyViewer, tools/fe2ShipBuilder).
#
#   glf_add_sdl2()    SDL2 (set the SDL_<SUBSYSTEM> options first)
#   glf_add_physfs()  PhysFS::PhysFS-static, directories only
#   glf_add_glad()    glad: OpenGL 3.3 core loader (desktop)
#   glf_add_imgui()   imgui: core + SDL2 / OpenGL3 backends

set(GLF_VENDOR_DIR "${CMAKE_CURRENT_LIST_DIR}/../vendor")
get_filename_component(GLF_VENDOR_DIR "${GLF_VENDOR_DIR}" ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/SdlCheckCache.cmake")

# The SDL2 headers alone (the web build: emscripten brings its own SDL2)
set(GLF_SDL2_INCLUDE_DIR "${GLF_VENDOR_DIR}/SDL2/include")

macro(glf_sdl_force_off)
    foreach(opt ${ARGN})
        set(${opt} OFF CACHE BOOL "" FORCE)
    endforeach()
endmacro()

function(glf_add_sdl2)
    if(APPLE)
        message(FATAL_ERROR "vendor/SDL2 is stripped down to Windows, Linux and Android: "
            "add SDL's macOS sources (cocoa, coreaudio, ...) to build here")
    endif()
    # Drivers whose sources are not in vendor/SDL2. SDL would turn some of them
    # on when it finds their libraries, so they are forced off.
    glf_sdl_force_off(
        SDL_TEST SDL_INSTALL_TESTS SDL_VULKAN SDL_RENDER_D3D SDL_DISKAUDIO SDL_OFFSCREEN
        SDL_KMSDRM SDL_RPI SDL_VIVANTE SDL_DIRECTFB
        SDL_JACK SDL_SNDIO SDL_ESD SDL_ARTS SDL_NAS SDL_FUSIONSOUND SDL_OSS
        SDL_HIDAPI_LIBUSB)
    # HIDAPI only on Android: SDL's Java side needs its native code there
    if(NOT ANDROID)
        glf_sdl_force_off(SDL_HIDAPI)
    endif()
    glf_sdl_checks_load()
    add_subdirectory("${GLF_VENDOR_DIR}/SDL2" "${CMAKE_BINARY_DIR}/_vendor/SDL2" EXCLUDE_FROM_ALL)
    glf_sdl_checks_save()
endfunction()

function(glf_add_physfs)
    # Only directories are mounted (src/host_files.c): the archivers are off
    set(dir "${GLF_VENDOR_DIR}/physfs/src")
    add_library(physfs-static STATIC
        ${dir}/physfs.c ${dir}/physfs_byteorder.c ${dir}/physfs_unicode.c
        ${dir}/physfs_archiver_dir.c
        ${dir}/physfs_platform_windows.c ${dir}/physfs_platform_posix.c
        ${dir}/physfs_platform_unix.c ${dir}/physfs_platform_android.c
        ${dir}/physfs_platform_emscripten.c)
    add_library(PhysFS::PhysFS-static ALIAS physfs-static)
    target_include_directories(physfs-static PUBLIC ${dir})
    target_compile_definitions(physfs-static PRIVATE PHYSFS_SUPPORTS_DEFAULT=0)
    if(WIN32)
        target_compile_definitions(physfs-static PUBLIC PHYSFS_STATIC)
        target_link_libraries(physfs-static PRIVATE advapi32 shell32)
    elseif(UNIX AND NOT ANDROID AND NOT EMSCRIPTEN)
        find_package(Threads REQUIRED)
        target_link_libraries(physfs-static PRIVATE Threads::Threads)
    endif()
    if(MSVC)
        target_compile_options(physfs-static PRIVATE /w)
    else()
        target_compile_options(physfs-static PRIVATE -w)
    endif()
endfunction()

function(glf_add_glad)
    find_package(OpenGL REQUIRED)
    add_library(glad STATIC "${GLF_VENDOR_DIR}/glad/src/glad.c")
    target_include_directories(glad PUBLIC "${GLF_VENDOR_DIR}/glad/include")
endfunction()

# Link it to SDL afterwards (target_link_libraries(imgui PUBLIC <SDL target>))
function(glf_add_imgui)
    set(dir "${GLF_VENDOR_DIR}/imgui")
    add_library(imgui STATIC
        ${dir}/imgui.cpp ${dir}/imgui_draw.cpp ${dir}/imgui_tables.cpp ${dir}/imgui_widgets.cpp
        ${dir}/backends/imgui_impl_sdl2.cpp ${dir}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(imgui PUBLIC ${dir} ${dir}/backends)
endfunction()
