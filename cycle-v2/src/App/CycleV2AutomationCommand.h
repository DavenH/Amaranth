#pragma once

#include <JuceHeader.h>

#include <optional>

namespace CycleV2 {

enum class CycleV2AutomationCommand {
    SnapshotState,
    InspectTargets,
    InspectPointerTargets,
    InspectPointerCursor,
    InspectOpenGLDiagnostics,
    InspectCanvasPerformance,
    ResetCanvasPerformance,
    InspectAudioPerformance,
    ResetAudioPerformance,
    SendMidi,
    RequestCanvasOpenGLFrame,
    ExportGraph,
    OpenGraph,
    SaveGraph,
    GeneratePresetPreview,
    ListMenuItems,
    InvokeMenuItem,
    ListPaletteItems,
    InvokePaletteItem,
    CaptureAudio,
    CaptureLiveAudio,
    OpenNodeEditor,
    AddNode,
    MoveNode,
    ConnectPorts,
    DeleteNode,
    DeleteEdge,
    DeleteGuideCurve,
    LoadGuideHeatmap,
    ClearGuideHeatmap,
    Undo,
    SetNodeParameter,
    SetGuideParameter,
    InspectNodeControls,
    SetMorphSlider,
    SetPrimaryAxis,
    ToggleLink,
    SelectVertex,
    SetVertexParameter,
    Pointer,
    Key,
    Screenshot,
    AssertState,
    AssertNodeParameter,
    ListAssertionPaths,
    WaitForIdle,
    Quit
};

std::optional<CycleV2AutomationCommand> automationCommandForName(
        const juce::String& name);

}
