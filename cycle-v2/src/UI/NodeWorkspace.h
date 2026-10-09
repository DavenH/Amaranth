#pragma once

#include <JuceHeader.h>
#include <optional>

#include "App/StandaloneAudioEngine.h"
#include "UI/NodeCanvas.h"
#include "UI/PerformanceKeyboard.h"
#include "Graph/PatternLibrary.h"

namespace CycleV2 {

using namespace juce;

class NodeWorkspace :
        public Component
    ,   private Timer {
public:
    explicit NodeWorkspace(StandaloneAudioEngine& audioEngine);
    ~NodeWorkspace() override;

    bool saveGraphToFile(const File& file);
    bool loadGraphFromFile(const File& file);
    bool capturePresetPreviewForAutomation(
            PresetPreviewView view,
            PresetPreviewImage& image,
            String& errorMessage) const;
    bool savePresetPreviewForAutomation(
            PresetPreviewImage image,
            const File& destination,
            String& errorMessage);
    bool isGraphDirty() const { return canvas.isGraphDirty(); }
    const File& graphFile() const { return canvas.graphFile(); }
    ProbeRefreshMode probeRefreshMode() const { return canvas.probeRefreshMode(); }
    void setProbeRefreshMode(ProbeRefreshMode mode) { canvas.setProbeRefreshMode(mode); }
    ScalarSurfaceTimeStyle timeSurfaceStyle() const { return canvas.timeSurfaceStyle(); }
    void setTimeSurfaceStyle(ScalarSurfaceTimeStyle style) {
        canvas.setTimeSurfaceStyle(style);
    }
    std::optional<ScalarSurfaceTimeStyle> bipolarSpectralSurfaceStyle() const {
        return canvas.bipolarSpectralSurfaceStyle();
    }
    void setBipolarSpectralSurfaceStyle(std::optional<ScalarSurfaceTimeStyle> style) {
        canvas.setBipolarSpectralSurfaceStyle(style);
    }
    void setGraphDocumentStateChangedCallback(std::function<void()> callback) {
        canvas.setGraphDocumentStateChangedCallback(std::move(callback));
    }
    void configurePresetSidebar(
            std::vector<File> directories,
            InlinePresetBrowser::OpenCallback openCallback,
            InlinePresetBrowser::ActionCallback browseCallback,
            InlinePresetBrowser::ActionCallback createCallback,
            LibraryFavorites* favorites = nullptr);
    void refreshPresetSidebarFavorites();
    void refreshPresetSidebarIndex();
    void refreshPresetSidebarRecord(const File& file);
    void setCurrentPresetTags(StringArray tags);
    void configurePatternLibrary(File factoryDirectory, File userDirectory);
    var exportAutomationState() const;
    String exportGraphJson() const;
    NodeCanvas& getCanvas() { return canvas; }
    PerformanceKeyboardPanel& performanceKeyboardForAutomation() { return keyboard; }
    bool openNodeEditorForAutomation(const String& nodeId);
    bool addNodeForAutomation(const String& kind, Point<float> position, String& nodeId);
    bool moveNodeForAutomation(const String& nodeId, Point<float> position);
    bool connectPortsForAutomation(
            const String& sourceNodeId,
            const String& sourcePortId,
            const String& destNodeId,
            const String& destPortId);
    bool deleteNodeForAutomation(const String& nodeId);
    bool deleteEdgeForAutomation(int edgeIndex);
    bool deleteGuideCurveForAutomation(const String& guideId);
    bool loadGuideHeatmapForAutomation(const String& guideId, const File& file);
    bool clearGuideHeatmapForAutomation(const String& guideId);
    bool undoForAutomation();
    bool setGuideParameterForAutomation(
            const String& guideId,
            const String& parameterId,
            const String& value);
    bool setNodeParameterForAutomation(
            const String& nodeId,
            const String& parameterId,
            const String& label,
            const String& value);
    bool setMorphSliderForAutomation(const String& nodeId, const String& axis, float value);
    bool setPrimaryAxisForAutomation(const String& nodeId, const String& axis);
    bool toggleLinkForAutomation(const String& nodeId, const String& axis);
    bool selectVertexForAutomation(const String& nodeId, int vertexIndex);
    bool setVertexParameterForAutomation(const String& nodeId, const String& parameterId, float value);
    bool getNodeParameterForAutomation(const String& nodeId, const String& parameterId, String& value) const;
    var inspectNodeControlsForAutomation(const String& nodeId) const;
    var inspectPointerTargetsForAutomation() const;
    var inspectOpenGLDiagnosticsForAutomation() const;
    var inspectCanvasPerformanceForAutomation() const;
    void resetCanvasPerformanceForAutomation();
    var inspectAudioPerformanceForAutomation() const;
    void resetAudioPerformanceForAutomation();
    void requestCanvasOpenGLFrameForAutomation();
    void recreateCanvasOpenGLContextForAutomation();
    var captureAudioForAutomation(size_t frameCount) const;
    bool copyAudioPlanForAutomation(GraphExecutionPlan& plan, uint64_t& revision) const;
    var performanceStateForAutomation() const;
    bool performancePointerDownForAutomation(int noteNumber, float velocity);
    bool performancePointerDragForAutomation(int noteNumber, float velocity);
    bool performancePointerUpForAutomation();
    bool performanceSelectPreviewNoteForAutomation(int noteNumber);
    bool performanceSetModWheelForAutomation(int value);
    bool performanceBeginModWheelGestureForAutomation(int value);
    bool performanceUpdateModWheelGestureForAutomation(int value);
    bool performanceEndModWheelGestureForAutomation();
    bool togglePreviewPlayback();
    bool togglePreviewPlaybackForAutomation();
    bool enqueueMidiForAutomation(const juce::MidiMessage& message);
    StandaloneAudioEngine::LiveCapture captureLiveAudioForAutomation(int durationMs);

    void resized() override;

private:
    std::optional<PresetMidiSequence> resolvedPresetSequence() const;
    void refreshPatternSidebar();
    void selectPattern(const String& id);
    void createPattern(const String& name, const StringArray& tags);
    void editPattern(const String& id);
    void renamePattern(const String& id, const String& name);
    void deletePattern(const String& id);
    void saveEditedSequence(PresetMidiSequence sequence, const String& sourceId);
    void showPatternSaveError();
    void timerCallback() override;
    void drainRecordedMidi();
    bool publishAudioPlan(
            const StandaloneAudioEngine::Status& status,
            bool forcePublication);
    void layoutPerformanceKeyboard();
    void updateOutputMeter(const StandaloneAudioEngine::Status& status);
    var outputMeterStateForAutomation() const;

    StandaloneAudioEngine& audioEngine;
    NodeCanvas canvas;
    MidiKeyboardState keyboardState;
    PerformanceKeyboardPanel keyboard;
    std::unique_ptr<PatternLibrary> patternLibrary;
    bool performanceOccludedByExpandedEditor {};
    uint64_t publishedPlanRevision {};
    uint64_t publishedDevicePreparationRevision {};
    uint64_t audioPlanCopyCount {};
    bool previousDeviceReady {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeWorkspace)
};

}
