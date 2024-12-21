# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.29)
set(CMAKE_CXX_COMPILER g++-11)
project(Amaranth VERSION 1.0.0)

# Environment

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

file(STRINGS .env ENV_FILE)
foreach(VAR ${ENV_FILE})
    string(REGEX MATCH "^[^=]*" KEY ${VAR})
    string(REGEX REPLACE "^[^=]*=" "" VALUE ${VAR})
    set(ENV{${KEY}} ${VALUE})
endforeach()

# Global settings

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(_NO_FLOAT_16 1)
set(JUCE_MODULES_DIR $ENV{JUCE_MODULES_DIR})
set(VST3_DIR $ENV{VST3_SDK_DIR})
set(IPP_DIR $ENV{IPP_DIR})
set(IPP_TL_VARIANT "OpenMP")
set(IPP_LIBRARIES ippcore ipps ippi ippvm ippm)
set(IPP_ARCH intel64)

enable_testing()

list(APPEND CMAKE_MODULE_PATH $ENV{CATCH2_CMAKE_DIR})

find_package(PkgConfig REQUIRED)
find_package(Catch2 3 REQUIRED)
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

add_subdirectory(lib)
add_subdirectory(cycle)
