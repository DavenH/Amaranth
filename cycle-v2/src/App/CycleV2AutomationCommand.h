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
    RecreateCanvasOpenGLContext,
    ExportGraph,
    InspectSynthesisCapabilities,
    ConvertSynthesisParameter,
    OpenGraph,
    ResizeWindow,
    SaveGraph,
    GeneratePresetPreview,
    ListMenuItems,
    InvokeMenuItem,
    ListPaletteItems,
    InvokePaletteItem,
    CaptureAudio,
    ExportPatternSchedule,
    CaptureLiveAudio,
    OpenNodeEditor,
    PinComparisonSpy,
    SelectReference,
    ListReferences,
    ImportReference,
    ExportReferenceAnalysis,
    OpenReferencePitchEditor,
    AddNode,
    MoveNode,
    ConnectPorts,
    DeleteNode,
    DeleteEdge,
    DeleteGuideCurve,
    LoadGuideHeatmap,
    ClearGuideHeatmap,
    Undo,
    Redo,
    ApplyNodeEdits,
    SetNodeParameter,
    SetGuideParameter,
    InspectNodeControls,
    InspectExplanation,
    SetExplanation,
    OpenExplanationEditor,
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
    Ping,
    Quit
};

juce::String automationCommandName(CycleV2AutomationCommand command);

std::optional<CycleV2AutomationCommand> automationCommandForName(
        const juce::String& name);

}
