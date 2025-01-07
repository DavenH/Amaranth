# lib/CMakeLists.txt
project(AmaranthLib VERSION 1.0.0)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JuceLibraryCode")

file(GLOB_RECURSE LIB_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${JUCE_DIR}/*.cpp"
        "${JUCE_DIR}/*.c"
)

add_library(${PROJECT_NAME} STATIC ${LIB_SOURCES})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${VST3_DIR}
    ${JUCE_DIR}
    ${JUCE_MODULES_DIR}
    ${IPP_DIR}/include/ipp
)

if(UNIX AND NOT APPLE)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${ALSA_INCLUDE_DIRS}
        ${FREETYPE_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
        ${GTK3_INCLUDE_DIRS}
        ${WEBKIT_INCLUDE_DIRS}
    )
endif()

set(COMMON_DEFINITIONS
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<CONFIG:Debug>:_DEBUG=1>
    JUCE_PROJUCER_VERSION=0x80004
    JUCE_MODULE_AVAILABLE_juce_animation=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_plugin_client=1
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
    JUCE_MODULE_AVAILABLE_juce_osc=1
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_STANDALONE_APPLICATION=1
    JUCE_APP_VERSION=1.0.0
    JUCE_APP_VERSION_HEX=0x10000
)

if(UNIX AND NOT APPLE)
    list(APPEND COMMON_DEFINITIONS LINUX=1 JUCER_LINUX_MAKE_6D53C8B4=1)
elseif(APPLE)
    list(APPEND COMMON_DEFINITIONS MAC=1)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC ${COMMON_DEFINITIONS})

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${ALSA_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${CURL_LIBRARIES}
        ${GTK3_LIBRARIES}
        ${WEBKIT_LIBRARIES}
        pthread
        rt
        dl
    )
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${COCOA_FRAMEWORK}
        ${CORE_AUDIO_FRAMEWORK}
        ${CORE_MIDI_FRAMEWORK}
        ${AUDIO_TOOLBOX_FRAMEWORK}
        ${ACCELERATE_FRAMEWORK}
        ${WEBKIT_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${CORE_FOUNDATION_FRAMEWORK}
        ${CARBON_FRAMEWORK}
        ${APP_KIT_FRAMEWORK}
        ${QUARTZ_CORE_FRAMEWORK}
    )
endif()

# still want IPP on older Intel Silicon Macs
if(NOT (APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64"))
    target_link_libraries(${PROJECT_NAME} PUBLIC
        IPP::ippcore
        IPP::ipps
        IPP::ippcv
        IPP::ippi
        IPP::ippvm
    )
endif()

target_link_directories(${PROJECT_NAME} PUBLIC
    ${IPP_DIR}/lib
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g -ggdb -O0)

    if(BUILD_TESTING)
        file(GLOB_RECURSE TEST_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
        )

        list(APPEND TEST_SOURCES ${IM_SOURCES})
        add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

        target_include_directories(${PROJECT_NAME}_tests PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${VST3_DIR}
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

        target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
                LIB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

        target_link_libraries(${PROJECT_NAME}_tests
            PRIVATE
            # link all the stuff built in the main project library,
            # saves us from having to build all of it again
            ${PROJECT_NAME}
            Catch2::Catch2WithMain
        )

        include(CTest)
        include(Catch)
        catch_discover_tests(${PROJECT_NAME}_tests)
    endif()
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

if(NOT APPLE)
    target_link_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden)
endif()
