#pragma once

#include <JuceHeader.h>

#include "App/StandaloneAudioEngine.h"
#include "UI/NodeCanvas.h"
#include "UI/PerformanceKeyboard.h"

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
    var exportAutomationState() const;
    String exportGraphJson() const;
    NodeCanvas& getCanvas() { return canvas; }
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
    void requestCanvasOpenGLFrameForAutomation();
    var captureAudioForAutomation(size_t frameCount) const;
    var performanceStateForAutomation() const;
    bool performancePointerDownForAutomation(int noteNumber, float velocity);
    bool performancePointerDragForAutomation(int noteNumber, float velocity);
    bool performancePointerUpForAutomation();
    StandaloneAudioEngine::LiveCapture captureLiveAudioForAutomation(int durationMs);

    void resized() override;

private:
    void timerCallback() override;
    void layoutPerformanceKeyboard();
    void updateOutputMeter(const StandaloneAudioEngine::Status& status);
    var outputMeterStateForAutomation() const;

    StandaloneAudioEngine& audioEngine;
    NodeCanvas canvas;
    MidiKeyboardState keyboardState;
    PerformanceKeyboardPanel keyboard;
    bool performanceOccludedByExpandedEditor {};
    uint64_t publishedPlanRevision {};
    uint64_t publishedDevicePreparationRevision {};
    bool previousDeviceReady {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeWorkspace)
};

}
