# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.29)
project(Amaranth VERSION 1.0.0)

# Global settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(_NO_FLOAT_16 1)
enable_testing()

list(APPEND CMAKE_MODULE_PATH "/usr/local/lib/cmake/Catch2")

# Find common packages
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

# Common paths
set(JUCE_MODULES_DIR "/home/daven/SDKs/JUCE/modules")
set(VST3_DIR "/home/daven/SDKs/vst3sdk")
set(IPP_DIR "/opt/intel/oneapi/ipp/latest")
set(IPP_TL_VARIANT "OpenMP")
set(IPP_LIBRARIES ippcore ipps ippi ippvm ippm)
set(IPP_ARCH intel64)

# Add subdirectories
add_subdirectory(lib)
add_subdirectory(cycle)
