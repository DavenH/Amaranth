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

add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${APP_SOURCES}
        src/AppSettings.h)
set(TARGET_TYPE "Standalone")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ${BASE_DEFINITIONS}
    JUCE_APP_CONFIG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/incl/JucePluginDefines.h"
)

if(APPLE)
    set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/Oscillo.icns)
    set_source_files_properties(${APP_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON})

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist
        MACOSX_BUNDLE_ICON_FILE "Oscillo.icns"
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${JUCE_MODULES_DIR}
    ${IPP_DIR}/include/ipp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AmaranthLib
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g -ggdb -O0)

    if(BUILD_TESTING)
        file(GLOB_RECURSE TEST_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
        )

#        list(APPEND TEST_SOURCES ${IM_SOURCES})
        add_executable(${PROJECT_NAME}_tests ${APP_SOURCES} ${TEST_SOURCES})

        target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
            BUILD_TESTING=1
            JUCE_APP_CONFIG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/incl/JucePluginDefines.h"
        )

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

        target_link_libraries(${PROJECT_NAME}_tests
            PRIVATE
            AmaranthLib
            Catch2::Catch2WithMain
        )

        include(CTest)
        include(Catch)
        catch_discover_tests(${PROJECT_NAME}_tests)
    endif()
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
    )
elseif(APPLE)
    install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION Applications
    )
endif()
