# app/CMakeLists.txt
project(Cycle VERSION 1.9.0)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JuceLibraryCode")

file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${JUCE_DIR}/*.cpp"
    "${JUCE_DIR}/*.c"
)

list(FILTER APP_SOURCES EXCLUDE REGEX ".*tests/.*\\.cpp$")

file(GLOB_RECURSE APP_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/tests/*.cpp"
)

add_executable(${PROJECT_NAME} ${APP_SOURCES})
add_executable(${PROJECT_NAME}_tests ${APP_TEST_SOURCES} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${JUCE_DIR}
    ${JUCE_MODULES_DIR}
    ${IPP_DIR}/include/ipp
)

target_include_directories(${PROJECT_NAME}_tests PRIVATE "/usr/local/include")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    JUCE_APP_CONFIG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/Incl/JucePluginDefines.h"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    LINUX=1
    JUCE_PROJUCER_VERSION=0x80004
    JUCE_MODULE_AVAILABLE_juce_animation=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_plugin_client=1
    JUCE_MODULE_AVAILABLE_juce_audio_processors=1
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_data_structures=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_graphics=1
    JUCE_MODULE_AVAILABLE_juce_gui_basics=1
    JUCE_MODULE_AVAILABLE_juce_gui_extra=1
    JUCE_MODULE_AVAILABLE_juce_opengl=1
    JUCE_MODULE_AVAILABLE_juce_osc=1
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Build_VST=0
    JucePlugin_Build_VST3=1
    JucePlugin_Build_AU=0
    JucePlugin_Build_AUv3=0
    JucePlugin_Build_AAX=0
    JucePlugin_Build_Standalone=1
    JucePlugin_Build_Unity=0
    JucePlugin_Build_LV2=0
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AmaranthLib
    ${IPP_LIBRARIES}
    pthread
    rt
    dl
)

target_link_libraries(${PROJECT_NAME}_tests PRIVATE Catch2::Catch2WithMain)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${IPP_DIR}/lib
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g -ggdb -O0)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

target_link_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden)

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME}_tests)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
