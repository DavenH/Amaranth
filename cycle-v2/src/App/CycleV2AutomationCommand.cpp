#include "App/CycleV2AutomationCommand.h"

namespace CycleV2 {

std::optional<CycleV2AutomationCommand> automationCommandForName(
        const juce::String& name) {
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
            { "exportGraph", Command::ExportGraph },
            { "openGraph", Command::OpenGraph },
            { "saveGraph", Command::SaveGraph },
            { "listMenuItems", Command::ListMenuItems },
            { "listMenus", Command::ListMenuItems },
            { "invokeMenuItem", Command::InvokeMenuItem },
            { "listPaletteItems", Command::ListPaletteItems },
            { "invokePaletteItem", Command::InvokePaletteItem },
            { "captureAudio", Command::CaptureAudio },
            { "captureLiveAudio", Command::CaptureLiveAudio },
            { "openNodeEditor", Command::OpenNodeEditor },
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
            { "setNodeParameter", Command::SetNodeParameter },
            { "setGuideParameter", Command::SetGuideParameter },
            { "inspectNodeControls", Command::InspectNodeControls },
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
            { "quit", Command::Quit }
    };
    for (const auto& command : commands) {
        if (name == command.first) {
            return command.second;
        }
    }
    return std::nullopt;
}

}
