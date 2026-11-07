# Faster SDL2 configures.
#
# SDL's CMakeLists runs a couple of hundred compiler checks ("Looking for ...",
# "Performing Test ...") and has no switch to skip them. Each is a separate
# compile, which takes minutes on Windows, and every new build directory
# repeats them. CMake skips a check whose result is already in the cache, so the
# results are saved once per toolchain and loaded into new build directories.
#
# Windows hosts only: there the results depend on nothing but the toolchain.
# On Linux and macOS installing a -dev package can change them, and configuring
# is quick anyway.
#
# The results are in .cache/sdl-checks; delete that directory to redo the checks.

set(GLF_SDL_CHECKS_DIR "${CMAKE_CURRENT_LIST_DIR}/../.cache/sdl-checks")

# The file holding the results for the current toolchain
function(_glf_sdl_checks_file out)
    string(SHA1 key "${CMAKE_SYSTEM_NAME}|${CMAKE_SYSTEM_VERSION}|${CMAKE_SYSTEM_PROCESSOR}|${ANDROID_PLATFORM}|${CMAKE_C_COMPILER}|${CMAKE_C_COMPILER_VERSION}")
    string(SUBSTRING "${key}" 0 16 key)
    set(${out} "${GLF_SDL_CHECKS_DIR}/${key}.cmake" PARENT_SCOPE)
endfunction()

# Call before SDL is added
function(glf_sdl_checks_load)
    if(NOT CMAKE_HOST_WIN32)
        return()
    endif()
    _glf_sdl_checks_file(file)
    if(EXISTS "${file}")
        include("${file}")
    endif()
endfunction()

# Call after SDL is added: only a configure that got this far saves anything
function(glf_sdl_checks_save)
    if(NOT CMAKE_HOST_WIN32)
        return()
    endif()
    set(content "")
    get_cmake_property(vars CACHE_VARIABLES)
    foreach(var ${vars})
        if(var MATCHES "^(HAVE_|LIBC_|CHECK_CPU_ARCHITECTURE_|CPU_|STDC_)")
            get_property(type CACHE "${var}" PROPERTY TYPE)
            if(type STREQUAL "INTERNAL")
                string(APPEND content "set(${var} \"$CACHE{${var}}\" CACHE INTERNAL \"\")\n")
            endif()
        endif()
    endforeach()
    _glf_sdl_checks_file(file)
    file(WRITE "${file}" "${content}")
endfunction()
