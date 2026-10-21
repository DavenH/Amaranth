#include "App/CycleV2AutomationCommand.h"

namespace CycleV2 {

namespace {

using Command = CycleV2AutomationCommand;
const std::pair<const char*, Command> commands[] {
            { "snapshotState", Command::SnapshotState },
            { "inspectTargets", Command::InspectTargets },
            { "inspectPointerTargets", Command::InspectPointerTargets },
            { "inspectPointerCursor", Command::InspectPointerCursor },
            { "inspectOpenGLDiagnostics", Command::InspectOpenGLDiagnostics },
            { "inspectCanvasPerformance", Command::InspectCanvasPerformance },
            { "resetCanvasPerformance", Command::ResetCanvasPerformance },
            { "inspectAudioPerformance", Command::InspectAudioPerformance },
            { "resetAudioPerformance", Command::ResetAudioPerformance },
            { "sendMidi", Command::SendMidi },
            { "requestCanvasOpenGLFrame", Command::RequestCanvasOpenGLFrame },
            { "recreateCanvasOpenGLContext", Command::RecreateCanvasOpenGLContext },
            { "exportGraph", Command::ExportGraph },
            { "inspectSynthesisCapabilities", Command::InspectSynthesisCapabilities },
            { "convertSynthesisParameter", Command::ConvertSynthesisParameter },
            { "openGraph", Command::OpenGraph },
            { "resizeWindow", Command::ResizeWindow },
            { "saveGraph", Command::SaveGraph },
            { "generatePresetPreview", Command::GeneratePresetPreview },
            { "listMenuItems", Command::ListMenuItems },
            { "listMenus", Command::ListMenuItems },
            { "invokeMenuItem", Command::InvokeMenuItem },
            { "listPaletteItems", Command::ListPaletteItems },
            { "invokePaletteItem", Command::InvokePaletteItem },
            { "captureAudio", Command::CaptureAudio },
            { "exportPatternSchedule", Command::ExportPatternSchedule },
            { "setPresetSequence", Command::SetPresetSequence },
            { "captureLiveAudio", Command::CaptureLiveAudio },
            { "openNodeEditor", Command::OpenNodeEditor },
            { "pinComparisonSpy", Command::PinComparisonSpy },
            { "selectReference", Command::SelectReference },
            { "listReferences", Command::ListReferences },
            { "importReference", Command::ImportReference },
            { "exportReferenceAnalysis", Command::ExportReferenceAnalysis },
            { "openReferencePitchEditor", Command::OpenReferencePitchEditor },
            { "openMeshPopup", Command::OpenNodeEditor },
            { "addNode", Command::AddNode },
            { "moveNode", Command::MoveNode },
            { "connectPorts", Command::ConnectPorts },
            { "connect", Command::ConnectPorts },
            { "deleteNode", Command::DeleteNode },
            { "removeNode", Command::DeleteNode },
            { "deleteEdge", Command::DeleteEdge },
            { "removeEdge", Command::DeleteEdge },
            { "deleteGuideCurve", Command::DeleteGuideCurve },
            { "removeGuideCurve", Command::DeleteGuideCurve },
            { "loadGuideHeatmap", Command::LoadGuideHeatmap },
            { "clearGuideHeatmap", Command::ClearGuideHeatmap },
            { "undo", Command::Undo },
            { "redo", Command::Redo },
            { "applyNodeEdits", Command::ApplyNodeEdits },
            { "setNodeParameter", Command::SetNodeParameter },
            { "setGuideParameter", Command::SetGuideParameter },
            { "inspectNodeControls", Command::InspectNodeControls },
            { "inspectExplanation", Command::InspectExplanation },
            { "setExplanation", Command::SetExplanation },
            { "setGroupInformationCard", Command::SetGroupInformationCard },
            { "openExplanationEditor", Command::OpenExplanationEditor },
            { "setMorphSlider", Command::SetMorphSlider },
            { "setPrimaryAxis", Command::SetPrimaryAxis },
            { "toggleLink", Command::ToggleLink },
            { "selectVertex", Command::SelectVertex },
            { "setVertexParameter", Command::SetVertexParameter },
            { "pointer", Command::Pointer },
            { "key", Command::Key },
            { "screenshot", Command::Screenshot },
            { "assertState", Command::AssertState },
            { "assertNodeParameter", Command::AssertNodeParameter },
            { "listAssertionPaths", Command::ListAssertionPaths },
            { "waitForIdle", Command::WaitForIdle },
            { "ping", Command::Ping },
            { "quit", Command::Quit }
};

}

juce::String automationCommandName(CycleV2AutomationCommand value) {
    for (const auto& command : commands) {
        if (command.second == value) {
            return command.first;
        }
    }
    return {};
}

std::optional<CycleV2AutomationCommand> automationCommandForName(
        const juce::String& name) {
    for (const auto& command : commands) {
        if (name == command.first) {
            return command.second;
        }
    }
    return std::nullopt;
}

}
