cmake_minimum_required(VERSION 3.16)
project(drod)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build variant options
option(BUILD_STEAM "Build Steam version" OFF)
option(ENABLE_CARAVELNET "Enable CaravelNet functionality" ON)

# Build type configurations
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "-pipe -W -Wall -Wno-unused -Wno-unused-parameter -Wno-uninitialized")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -D_DEBUG -DENABLE_BREAKPOINTS -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Architecture flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")

# Distribution flags
add_definitions(-DUSE_SDL_MIXER -DDROD_AUTODETECT_REVISION)

if (ENABLE_CARAVELNET)
    add_definitions(-DCARAVELBUILD)
endif ()

# Steam-specific configuration
if(BUILD_STEAM)
    add_definitions(-DSTEAMBUILD)
    add_definitions(-DSTEAMBUILD_TSS_APP)

    # Steam SDK path - you can override this with -DSTEAM_SDK_PATH=...
    # /media/ext-data/sdks/steamworks_sdk
    set(STEAM_SDK_PATH "~/steamworks_sdk" CACHE PATH "Path to Steam SDK")

    if(EXISTS "${STEAM_SDK_PATH}/public/steam")
        include_directories("${STEAM_SDK_PATH}/public/steam")
        link_directories("${STEAM_SDK_PATH}/redistributable_bin/linux64")
        link_directories("${STEAM_SDK_PATH}/redistributable_bin/linux32")

        # Add Steam API library
        set(STEAM_LIBRARIES steam_api)
        message(STATUS "Steam SDK found at: ${STEAM_SDK_PATH}")
    else()
        message(WARNING "Steam SDK not found at: ${STEAM_SDK_PATH}")
        message(WARNING "Please set STEAM_SDK_PATH to correct location.")
    endif()
endif()

# Release build gets LTO linking flags and strip symbols
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=auto -s")
endif()

# Force static linking for most libraries (like ninjamaker)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic")
# Add static libraries in the order ninjamaker uses
set(STATIC_LIBRARIES
    SDL2_mixer SDL2_ttf SDL2 theora vorbisfile vorbis ogg jpeg mk4
    freetype png16 curl ssl crypto expat jsoncpp
)
# Dynamic libraries that must remain dynamic
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bdynamic -lnghttp2 -lz -lpthread -lm -lrt -ldl")

# Static libs path from main DROD project
set(STATIC_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Master/Linux/static-libs")
set(CMAKE_PREFIX_PATH "${STATIC_LIBS_DIR}")

# Include directories - mimic ninja order exactly
include_directories("${CMAKE_BINARY_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${STATIC_LIBS_DIR}/include")
include_directories("${STATIC_LIBS_DIR}/include/SDL2")

# Library directories
link_directories("${STATIC_LIBS_DIR}/lib")

# Set PKG_CONFIG_PATH to find our static libs
set(ENV{PKG_CONFIG_PATH} "${STATIC_LIBS_DIR}/lib/pkgconfig")

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(FREETYPE2 REQUIRED freetype2)
pkg_check_modules(LIBPNG REQUIRED libpng)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(EXPAT REQUIRED expat)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(ZLIB REQUIRED zlib)

# Generate drod-revision.h (similar to ninja's approach)
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_REVISION)
    set(GIT_REVISION "unknown")
endif()

# Create custom command to generate drod-revision.h
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/drod-revision.h
    COMMAND ${CMAKE_COMMAND} -E echo "#define DROD_VERSION_REVISION ${GIT_REVISION}" > ${CMAKE_BINARY_DIR}/drod-revision.h.new
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/drod-revision.h.new ${CMAKE_BINARY_DIR}/drod-revision.h
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/drod-revision.h.new
    COMMENT "Generating drod-revision.h"
    VERBATIM
)

# Create custom target that always runs to check for revision changes
add_custom_target(update_revision ALL
    DEPENDS ${CMAKE_BINARY_DIR}/drod-revision.h
)

# Add subdirectories
add_subdirectory(BackEndLib)
add_subdirectory(FrontEndLib)
add_subdirectory(DRODLib)
add_subdirectory(DROD)

# CaravelNet is always built but can be optionally linked
if(ENABLE_CARAVELNET)
    add_subdirectory(CaravelNet)
endif()