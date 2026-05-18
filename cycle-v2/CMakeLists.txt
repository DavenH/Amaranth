project(CycleV2 VERSION 0.1.0)

if(NOT BUILD_TYPE STREQUAL "Standalone")
    return()
endif()

file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

set(GRAPH_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/NodeGraph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphValidator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphCompiler.cpp"
)

set(RUNTIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/GraphRuntime.cpp"
)

set(BASE_DEFINITIONS
    JUCE_PROJUCER_VERSION=0x80004
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Build_Standalone=1
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

if(UNIX AND NOT APPLE)
    list(APPEND BASE_DEFINITIONS LINUX=1)
elseif(APPLE)
    list(APPEND BASE_DEFINITIONS MAC=1)
endif()

add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${APP_SOURCES})

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
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g -ggdb -O0)

    if(BUILD_TESTING)
        file(GLOB_RECURSE TEST_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
        )

        add_executable(${PROJECT_NAME}_tests ${GRAPH_SOURCES} ${RUNTIME_SOURCES} ${TEST_SOURCES})

        target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
            BUILD_TESTING=1
            JUCE_APP_CONFIG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/incl/JucePluginDefines.h"
        )

        target_include_directories(${PROJECT_NAME}_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${JUCE_MODULES_DIR}
            ${IPP_DIR}/include/ipp
        )

        target_link_libraries(${PROJECT_NAME}_tests PRIVATE
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
