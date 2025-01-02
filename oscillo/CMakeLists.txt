project(Oscillo VERSION 0.0.1)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/JuceLibraryCode")

file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${JUCE_DIR}/*.cpp"
    "${JUCE_DIR}/*.c"
)

set(BASE_DEFINITIONS
    LINUX=1
    JUCE_PROJUCER_VERSION=0x80004
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Build_Standalone=1
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_USE_MP3AUDIOFORMAT=1
)

add_executable(${PROJECT_NAME} ${APP_SOURCES})
set(TARGET_TYPE "Standalone")

target_compile_definitions(Oscillo
    PRIVATE
    ${BASE_DEFINITIONS}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    JUCE_APP_CONFIG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/incl/JucePluginDefines.h"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${JUCE_DIR}
    ${JUCE_MODULES_DIR}
    ${IPP_DIR}/include/ipp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AmaranthLib
)