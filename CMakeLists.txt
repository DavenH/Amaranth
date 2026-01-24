# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.29)
#set(CMAKE_CXX_COMPILER g++-11)

if(APPLE)
    if(NOT CMAKE_OSX_SYSROOT OR NOT EXISTS "${CMAKE_OSX_SYSROOT}")
        execute_process(
            COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE MACOS_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE XCRUN_RESULT
        )
        if(MACOS_SDK_PATH AND EXISTS "${MACOS_SDK_PATH}")
            set(CMAKE_OSX_SYSROOT "${MACOS_SDK_PATH}" CACHE PATH "macOS SDK path" FORCE)
            message(STATUS "Using macOS SDK: ${CMAKE_OSX_SYSROOT}")
        else()
            message(WARNING "Unable to resolve macOS SDK path via xcrun (result ${XCRUN_RESULT}).")
        endif()
    endif()
endif()

project(Amaranth VERSION 1.0.0)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
    message(FATAL_ERROR
        "\n"
        "ERROR: No .env file found in project root!\n"
        "Please run the environment setup script to create one:\n"
        "\n"
        "    .scripts/make_env.sh\n"
        "\n"
        "This will help you configure the necessary SDK paths for building.\n"
    )
endif()

# Environment
file(STRINGS .env ENV_FILE)
foreach(VAR ${ENV_FILE})
    string(REGEX MATCH "^[^=]*" KEY ${VAR})
    string(REGEX REPLACE "^[^=]*=" "" VALUE ${VAR})
    set(ENV{${KEY}} ${VALUE})
endforeach()

# Global settings
if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)

    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -Wno-deprecated-declarations")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -Wno-deprecated-declarations")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(IPP_ARCH "arm64")
    else()
        set(IPP_ARCH "intel64")
    endif()
else()
    set(IPP_ARCH "intel64")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(_NO_FLOAT_16 1)

set(JUCE_MODULES_DIR $ENV{JUCE_MODULES_DIR})
set(VST3_DIR $ENV{VST3_SDK_DIR})
set(IPP_DIR $ENV{IPP_DIR})
set(IPP_TL_VARIANT "OpenMP")

if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    add_definitions(-DUSE_ACCELERATE)
else()
    find_package(IPP REQUIRED)
    add_definitions(-DUSE_IPP)
endif()

enable_testing()

list(APPEND CMAKE_MODULE_PATH $ENV{CATCH2_CMAKE_DIR})

find_package(Catch2 3 REQUIRED)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)

    # Linux-specific dependencies
    pkg_check_modules(ALSA REQUIRED alsa)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    pkg_check_modules(OPENGL REQUIRED gl)
    pkg_check_modules(CURL REQUIRED libcurl)
    pkg_check_modules(GTK3 REQUIRED gtk+-x11-3.0)
    pkg_check_modules(WEBKIT webkit2gtk-4.0)
    if(NOT WEBKIT_FOUND)
        pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
    endif()
elseif(APPLE)
    find_library(ACCELERATE_FRAMEWORK Accelerate REQUIRED)
    find_library(APP_KIT_FRAMEWORK AppKit REQUIRED)
    find_library(AUDIO_TOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
    find_library(CARBON_FRAMEWORK Carbon REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(CORE_AUDIO_FRAMEWORK CoreAudio REQUIRED)
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(CORE_MIDI_FRAMEWORK CoreMIDI REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(QUARTZ_CORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(WEBKIT_FRAMEWORK WebKit REQUIRED)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
    find_library(CORE_AUDIO_KIT_FRAMEWORK CoreAudioKit REQUIRED)

endif()

add_subdirectory(lib)
add_subdirectory(oscillo)
add_subdirectory(cycle)
