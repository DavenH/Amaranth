# cycle/CMakeLists.txt
project(Cycle VERSION 1.9.0)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/JuceLibraryCode")
set(CYCLE_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JuceLibraryCode")
set(AAX_SDK_DIR "${JUCE_MODULES_DIR}/juce_audio_plugin_client/AAX/SDK")
set(CYCLE_VERSION_BASE_REVISION 2603)
set(CYCLE_VERSION_SOURCE_REVISION 503)

find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE CYCLE_GIT_COMMIT_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(CYCLE_GIT_COMMIT_COUNT MATCHES "^[0-9]+$")
        set(CYCLE_VERSION_SOURCE_REVISION "${CYCLE_GIT_COMMIT_COUNT}")
    endif()
endif()

math(EXPR CYCLE_VERSION_REVISION "${CYCLE_VERSION_BASE_REVISION} + ${CYCLE_VERSION_SOURCE_REVISION}")
set(CYCLE_VERSION_SHORT "${PROJECT_VERSION}")
set(CYCLE_VERSION_STRING "${PROJECT_VERSION}.${CYCLE_VERSION_REVISION}")
set(CYCLE_VERSION_CODE "0x10900")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/CycleVersion.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CycleVersion.h"
    @ONLY
)

file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${JUCE_DIR}/*.cpp"
    "${JUCE_DIR}/*.c"
)

if(BUILD_TYPE STREQUAL "Standalone")
    # These compile to empty objects in standalone/test builds, which only adds ranlib noise.
    list(FILTER APP_SOURCES EXCLUDE REGEX ".*/include_juce_audio_processors_(ara|lv2_libs)\\.cpp$")
endif()

list(FILTER APP_SOURCES EXCLUDE REGEX ".*tests/.*\\.cpp$")

file(GLOB_RECURSE APP_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/tests/*.cpp"
)

if(APPLE)
    file(GLOB JUCE_OBJCXX_SOURCES
        "${JUCE_DIR}/*.cpp"
        "${JUCE_DIR}/*.mm"
        "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_*.cpp"
        "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_*.mm"
    )

    set_source_files_properties(
        ${JUCE_OBJCXX_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
endif()

set_source_files_properties(
    "${JUCE_DIR}/include_juce_graphics_Harfbuzz.cpp"
    PROPERTIES
    COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-Wno-nontrivial-memcall>"
)

# Define different targets based on build type
if(BUILD_TYPE STREQUAL "Standalone")
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${APP_SOURCES})
    set(TARGET_TYPE "Standalone")
else()
    if(APPLE)
        set(PLUGIN_CLIENT_SOURCES
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_AAX.mm"
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_AAX_utils.cpp"
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_AU_1.mm"
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_AU_2.mm"
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_LV2.mm"
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_VST3.mm"
        )
    else()
        set(PLUGIN_CLIENT_SOURCES
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_AAX.cpp"
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_AAX_utils.cpp"
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_LV2.cpp"
            "${CYCLE_JUCE_DIR}/include_juce_audio_plugin_client_VST3.cpp"
        )
    endif()

    add_library(${PROJECT_NAME} MODULE ${APP_SOURCES} ${PLUGIN_CLIENT_SOURCES})
    set(TARGET_TYPE "Plugin")
endif()

if(APPLE AND BUILD_TYPE STREQUAL "Standalone")
    set(CYCLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/CycleInfo.plist)
    set(CYCLE_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/Cycle.icns)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in"
        "${CYCLE_INFO_PLIST}"
        @ONLY
    )

    if(EXISTS ${CYCLE_INFO_PLIST})
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CYCLE_INFO_PLIST}
        )
    endif()

    if(EXISTS ${CYCLE_ICON})
        set_source_files_properties(${CYCLE_ICON} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources(${PROJECT_NAME} PRIVATE ${CYCLE_ICON})
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_ICON_FILE "Cycle.icns"
        )
    endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${JUCE_DIR}
    ${CYCLE_JUCE_DIR}
    ${JUCE_MODULES_DIR}
    ${JUCE_MODULES_DIR}/juce_audio_plugin_client/AU
    ${JUCE_MODULES_DIR}/juce_audio_processors/format_types/LV2_SDK
    ${JUCE_MODULES_DIR}/juce_audio_processors/format_types/LV2_SDK/lv2
    ${AAX_SDK_DIR}
    ${AAX_SDK_DIR}/Interfaces
    ${AAX_SDK_DIR}/Interfaces/ACF
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CYCLE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    JUCE_APP_CONFIG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/Incl/JucePluginDefines.h"
)

# Base definitions for both builds
set(BASE_DEFINITIONS
    JUCE_PROJUCER_VERSION=0x80004
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
#    JUCE_STRICT_REFCOUNTEDPOINTER=1
#    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_MP3AUDIOFORMAT=1
)

if(BUILD_TYPE STREQUAL "Plugin")
    list(APPEND BASE_DEFINITIONS
        JUCE_STANDALONE_APPLICATION=0
        JucePlugin_Build_VST3=1
        JucePlugin_Build_AAX=1
        JucePlugin_Build_LV2=1
    )

    if(APPLE)
        list(APPEND BASE_DEFINITIONS JucePlugin_Build_AU=1)
    endif()
else()
    list(APPEND BASE_DEFINITIONS
        JUCE_STANDALONE_APPLICATION=1
        JucePlugin_Build_Standalone=1
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    )
endif()

# Platform-specific definitions
if(UNIX AND NOT APPLE)
    list(APPEND BASE_DEFINITIONS LINUX=1)
elseif(APPLE)
    list(APPEND BASE_DEFINITIONS MAC=1)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE ${BASE_DEFINITIONS})

if(BUILD_TYPE STREQUAL "Plugin")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/JuceLV2Defines.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/JuceLV2Defines.h"
        @ONLY
    )
endif()

if(APPLE AND BUILD_TYPE STREQUAL "Plugin")
    set(PLUGIN_FORMAT_LOWER "vst3")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/PluginInfo.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/CycleVST3Info.plist"
        @ONLY
    )

    set(PLUGIN_FORMAT_LOWER "aax")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/PluginInfo.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/CycleAAXInfo.plist"
        @ONLY
    )

    set(PLUGIN_FORMAT_LOWER "au")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/PluginInfo.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/CycleAUInfo.plist"
        @ONLY
    )

    set(AAX_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/Cycle.aaxplugin")
    set(AU_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/Cycle.component")
    set(LV2_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/Cycle.lv2")

    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION vst3
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/CycleVST3Info.plist"
        OUTPUT_NAME "Cycle"
        PREFIX ""
        SUFFIX ".vst3"
        XCODE_ATTRIBUTE_GENERATE_PKGINFO_FILE YES
        XCODE_ATTRIBUTE_LIBRARY_STYLE Bundle
        XCODE_ATTRIBUTE_WRAPPER_EXTENSION vst3
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${AAX_BUNDLE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${AU_BUNDLE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${LV2_BUNDLE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${AAX_BUNDLE_DIR}/Contents/MacOS"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${AAX_BUNDLE_DIR}/Contents/Resources"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${AU_BUNDLE_DIR}/Contents/MacOS"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${AU_BUNDLE_DIR}/Contents/Resources"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${LV2_BUNDLE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${AAX_BUNDLE_DIR}/Contents/MacOS/Cycle"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/CycleAAXInfo.plist" "${AAX_BUNDLE_DIR}/Contents/Info.plist"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${AU_BUNDLE_DIR}/Contents/MacOS/Cycle"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/CycleAUInfo.plist" "${AU_BUNDLE_DIR}/Contents/Info.plist"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${LV2_BUNDLE_DIR}/Cycle"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/lv2/manifest.ttl" "${LV2_BUNDLE_DIR}/manifest.ttl"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/lv2/Cycle.ttl" "${LV2_BUNDLE_DIR}/Cycle.ttl"
        VERBATIM
    )

    add_custom_target(${PROJECT_NAME}_AAX DEPENDS ${PROJECT_NAME})
    add_custom_target(${PROJECT_NAME}_AU DEPENDS ${PROJECT_NAME})
    add_custom_target(${PROJECT_NAME}_LV2 DEPENDS ${PROJECT_NAME})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    AmaranthLib
)

# might be redundant
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        pthread
        rt
        dl
    )
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COCOA_FRAMEWORK}
        ${CORE_AUDIO_FRAMEWORK}
        ${CORE_MIDI_FRAMEWORK}
        ${AUDIO_TOOLBOX_FRAMEWORK}
        ${ACCELERATE_FRAMEWORK}
        ${OPENGL_FRAMEWORK}
        ${WEBKIT_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
    )
endif()

target_link_directories(${PROJECT_NAME} PRIVATE
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g -ggdb -O0)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

if(NOT APPLE)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${IPP_DIR}/include/ipp
    )

    target_link_directories(${PROJECT_NAME} PRIVATE
        ${IPP_DIR}/lib
    )

    target_link_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden)
endif()

if(BUILD_TESTING)
    # Add test executable only for explicit test builds.
    add_executable(${PROJECT_NAME}_tests ${APP_TEST_SOURCES} ${APP_SOURCES})

    target_include_directories(${PROJECT_NAME}_tests PRIVATE
            "/usr/local/include"
            ${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${JUCE_DIR}
            ${JUCE_MODULES_DIR}
            ${ALSA_INCLUDE_DIRS}
            ${FREETYPE_INCLUDE_DIRS}
            ${FONTCONFIG_INCLUDE_DIRS}
            ${OPENGL_INCLUDE_DIRS}
            ${CURL_INCLUDE_DIRS}
            ${GTK3_INCLUDE_DIRS}
            ${WEBKIT_INCLUDE_DIRS}
    )

    target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
        BUILD_TESTING=1
        CYCLE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        JUCE_APP_CONFIG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/Incl/JucePluginDefines.h"
    )

    target_link_libraries(${PROJECT_NAME}_tests PRIVATE
        AmaranthLib
        Catch2::Catch2WithMain
    )

    if(NOT APPLE)
        target_include_directories(${PROJECT_NAME}_tests PRIVATE
            ${IPP_DIR}/include/ipp
        )
    endif()

    include(Catch)
    catch_discover_tests(${PROJECT_NAME}_tests)
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
    )
elseif(APPLE)
    if(BUILD_TYPE STREQUAL "Standalone")
        install(TARGETS ${PROJECT_NAME}
            BUNDLE DESTINATION Applications
        )
    else()
        install(TARGETS ${PROJECT_NAME}
            BUNDLE DESTINATION "Library/Audio/Plug-Ins/VST3"
            LIBRARY DESTINATION "Library/Audio/Plug-Ins/VST3"
        )
        install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Cycle.aaxplugin"
            DESTINATION "Library/Application Support/Avid/Audio/Plug-Ins"
        )
        install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Cycle.component"
            DESTINATION "Library/Audio/Plug-Ins/Components"
        )
        install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Cycle.lv2"
            DESTINATION "Library/Audio/Plug-Ins/LV2"
        )
    endif()
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_custom_target(${PROJECT_NAME}_installer_zip
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/build/packages"
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/scripts/package_installer_zip.py"
            "--manifest" "${CMAKE_CURRENT_SOURCE_DIR}/installer.json"
            "--artifact-root" "${CMAKE_SOURCE_DIR}/build/standalone-release/cycle"
            "--artifact-root" "${CMAKE_SOURCE_DIR}/build/plugin-release/cycle"
            "--artifact-root" "${CMAKE_CURRENT_SOURCE_DIR}"
            "--output" "${CMAKE_SOURCE_DIR}/build/packages/Cycle.zip"
        COMMENT "Packaging Cycle installer zip"
        VERBATIM
    )
endif()
