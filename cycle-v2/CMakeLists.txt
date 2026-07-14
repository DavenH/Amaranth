project(CycleV2 VERSION 0.1.0)

if(NOT BUILD_TYPE STREQUAL "Standalone")
    return()
endif()

file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

list(APPEND APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/App/CycleV2Automation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/App/CycleV2Automation.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/NodeCanvasGlRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/NodeCanvasGlRenderer.h"
)
list(REMOVE_DUPLICATES APP_SOURCES)

set(GRAPH_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/NodeDefinition.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/NodeGraph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphDomainResolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphRenderSemanticResolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphValidator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphCompiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphCommandDispatcher.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphDocument.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphEditor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphNodeFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphSerializer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graph/GraphTransaction.cpp"
)

set(RUNTIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/GraphAudioExecutor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/GraphInvalidation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/GraphPreviewExecutor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/GraphPresentationModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/GraphRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/NodeUpdateGraph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/NodeAudioProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/NodeDspConfiguration.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/NodeModuleRegistry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/NodePreviewProcessor.cpp"
)

set(CANVAS_ARCHITECTURE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/NodeAutomationFacade.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/NodeCanvasScene.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/NodeCanvasViewport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/NodeViewModule.cpp"
)

set(NODE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Effect2D/CurveNodeEditors.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Effect2D/CurveNodeModels.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Effect2D/CurvePanelInfrastructure.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Effect2D/Effect2DExpandedEditorComponent.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Effect2D/Effect2DMeshState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Effect2D/Effect2DPanelBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Effect2D/Effect2DWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Effects/EffectSignalProcessors.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Envelope/EnvelopeMeshState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Envelope/EnvelopeSignalProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/FFT/FftBlockwiseDsp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/FFT/FftGridwiseDsp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/FFT/FftSignalProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshBlockwiseDsp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshControlsComponent.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshExpandedEditorComponent.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshGridwiseDsp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshGuideAttachmentMenu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshGuideAttachmentTarget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshInvalidation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshInteractor2D.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshInteractor3D.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshMeshFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshMeshEditState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshNodeModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshPanelBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshPanelDataSource.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshPanelEnvironment.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshPanelHosts.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshPanelRasterizer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshPanel2D.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshPanel3D.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshRenderProfile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshSidePanelRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshSliceRenderer2D.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshSurfaceRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Trimesh/TrimeshWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Nodes/Waveshaper/WaveshaperSignalProcessor.cpp"
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

add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${APP_SOURCES} ${NODE_SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ${BASE_DEFINITIONS}
    CYCLE_V2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
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

        add_executable(${PROJECT_NAME}_tests ${GRAPH_SOURCES} ${RUNTIME_SOURCES} ${NODE_SOURCES} ${CANVAS_ARCHITECTURE_SOURCES} ${TEST_SOURCES})

        target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
            BUILD_TESTING=1
            CYCLE_V2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
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
