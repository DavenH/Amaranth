# lib/CMakeLists.txt
project(AmaranthLib VERSION 1.0.0)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JuceLibraryCode")

file(GLOB_RECURSE LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${JUCE_DIR}/*.cpp"
    "${JUCE_DIR}/*.c"
)

list(FILTER LIB_SOURCES EXCLUDE REGEX ".*tests/.*\\.cpp$")

file(GLOB_RECURSE LIB_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/tests/*.cpp"
)

# Create library target
add_library(${PROJECT_NAME} STATIC ${LIB_SOURCES}
        src/Design/Updating/UpdateType.h)
add_executable(${PROJECT_NAME}_tests ${LIB_TEST_SOURCES} ${LIB_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${JUCE_DIR}
    ${JUCE_MODULES_DIR}
    ${IPP_DIR}/include/ipp
    ${ALSA_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
    ${WEBKIT_INCLUDE_DIRS}
)

target_include_directories(${PROJECT_NAME}_tests PRIVATE "/usr/local/include")

# Library compile definitions
target_compile_definitions(${PROJECT_NAME} PUBLIC
    LINUX=1
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<CONFIG:Debug>:_DEBUG=1>
    JUCE_PROJUCER_VERSION=0x80004
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_processors=1
    JUCE_MODULE_AVAILABLE_juce_audio_utils=1
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_data_structures=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_graphics=1
    JUCE_MODULE_AVAILABLE_juce_gui_basics=1
    JUCE_MODULE_AVAILABLE_juce_gui_extra=1
    JUCE_MODULE_AVAILABLE_juce_opengl=1
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_STANDALONE_APPLICATION=1
    JUCER_LINUX_MAKE_6D53C8B4=1
    JUCE_APP_VERSION=1.0.0
    JUCE_APP_VERSION_HEX=0x10000
    JucePlugin_Build_VST=0
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
    JucePlugin_Build_AUv3=0
    JucePlugin_Build_AAX=0
    JucePlugin_Build_Standalone=0
    JucePlugin_Build_Unity=0
    JucePlugin_Build_LV2=0
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${ALSA_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${GTK3_LIBRARIES}
    ${WEBKIT_LIBRARIES}
    ${IPP_LIBRARIES}
    pthread
    rt
    dl
)

target_link_libraries(${PROJECT_NAME}_tests PRIVATE Catch2::Catch2WithMain)

target_link_directories(${PROJECT_NAME} PUBLIC
    ${IPP_DIR}/lib
)

# Compiler options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g -ggdb -O0)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

target_link_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden)

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME}_tests)
