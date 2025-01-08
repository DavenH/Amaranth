project(Oscillo VERSION 0.0.1)

file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

set(BASE_DEFINITIONS
    JUCE_PROJUCER_VERSION=0x80004
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Build_Standalone=1
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

# Platform-specific definitions
if(UNIX AND NOT APPLE)
    list(APPEND BASE_DEFINITIONS LINUX=1)
elseif(APPLE)
    list(APPEND BASE_DEFINITIONS MAC=1)
endif()

add_executable(${PROJECT_NAME} ${APP_SOURCES})
set(TARGET_TYPE "Standalone")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ${BASE_DEFINITIONS}
    JUCE_APP_CONFIG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/incl/JucePluginDefines.h"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${JUCE_MODULES_DIR}
    ${IPP_DIR}/include/ipp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AmaranthLib
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COCOA_FRAMEWORK}
        ${CORE_AUDIO_FRAMEWORK}
        ${CORE_MIDI_FRAMEWORK}
        ${AUDIO_TOOLBOX_FRAMEWORK}
        ${ACCELERATE_FRAMEWORK}
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g -ggdb -O0)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
    )
elseif(APPLE)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION Applications
    )
endif()