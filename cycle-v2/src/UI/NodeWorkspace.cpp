#include "UI/NodeWorkspace.h"

namespace CycleV2 {

namespace {

var rectangleToVar(Rectangle<float> bounds) {
    auto* object = new DynamicObject();
    object->setProperty("x", bounds.getX());
    object->setProperty("y", bounds.getY());
    object->setProperty("width", bounds.getWidth());
    object->setProperty("height", bounds.getHeight());
    return var(object);
}

var pointerTarget(const String& id, const String& kind, Rectangle<float> bounds) {
    auto* object = new DynamicObject();
    object->setProperty("id", id);
    object->setProperty("kind", kind);
    object->setProperty("bounds", rectangleToVar(bounds));
    return var(object);
}

}

NodeWorkspace::NodeWorkspace(StandaloneAudioEngine& engine) :
        audioEngine(engine)
    ,   keyboard(keyboardState, engine) {
    setOpaque(true);
    addAndMakeVisible(canvas);
    canvas.addAndMakeVisible(keyboard);
    canvas.setOverlayOcclusionChangedCallback([this] {
        layoutPerformanceKeyboard();
    });
    startTimerHz(30);
    timerCallback();
}

NodeWorkspace::~NodeWorkspace() {
    stopTimer();
    canvas.setOverlayOcclusionChangedCallback({});
    keyboard.releaseAllNotes();
}

bool NodeWorkspace::saveGraphToFile(const File& file) {
    return canvas.saveGraphToFile(file);
}

bool NodeWorkspace::loadGraphFromFile(const File& file) {
    keyboard.releaseAllNotes();
    if (!canvas.loadGraphFromFile(file)) {
        return false;
    }
    layoutPerformanceKeyboard();
    return true;
}

var NodeWorkspace::exportAutomationState() const {
    var state = canvas.exportAutomationState();
    if (auto* object = state.getDynamicObject()) {
        object->setProperty("performance", performanceStateForAutomation());
        object->setProperty("liveOutputMeter", outputMeterStateForAutomation());
    }
    return state;
}

String NodeWorkspace::exportGraphJson() const {
    return canvas.exportGraphJson();
}

bool NodeWorkspace::openNodeEditorForAutomation(const String& nodeId) {
    return canvas.openNodeEditorForAutomation(nodeId);
}

bool NodeWorkspace::addNodeForAutomation(const String& kind, Point<float> position, String& nodeId) {
    return canvas.addNodeForAutomation(kind, position, nodeId);
}

bool NodeWorkspace::moveNodeForAutomation(const String& nodeId, Point<float> position) {
    return canvas.moveNodeForAutomation(nodeId, position);
}

bool NodeWorkspace::connectPortsForAutomation(
        const String& sourceNodeId,
        const String& sourcePortId,
        const String& destNodeId,
        const String& destPortId) {
    return canvas.connectPortsForAutomation(sourceNodeId, sourcePortId, destNodeId, destPortId);
}

bool NodeWorkspace::deleteNodeForAutomation(const String& nodeId) {
    return canvas.deleteNodeForAutomation(nodeId);
}

bool NodeWorkspace::deleteEdgeForAutomation(int edgeIndex) {
    return canvas.deleteEdgeForAutomation(edgeIndex);
}

bool NodeWorkspace::deleteGuideCurveForAutomation(const String& guideId) {
    return canvas.deleteGuideCurveForAutomation(guideId);
}

bool NodeWorkspace::undoForAutomation() {
    return canvas.undoForAutomation();
}

bool NodeWorkspace::setGuideParameterForAutomation(
        const String& guideId,
        const String& parameterId,
        const String& value) {
    return canvas.setGuideParameterForAutomation(guideId, parameterId, value);
}

bool NodeWorkspace::setNodeParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        const String& value) {
    return canvas.setNodeParameterForAutomation(nodeId, parameterId, label, value);
}

bool NodeWorkspace::setMorphSliderForAutomation(const String& nodeId, const String& axis, float value) {
    return canvas.setMorphSliderForAutomation(nodeId, axis, value);
}

bool NodeWorkspace::setPrimaryAxisForAutomation(const String& nodeId, const String& axis) {
    return canvas.setPrimaryAxisForAutomation(nodeId, axis);
}

bool NodeWorkspace::toggleLinkForAutomation(const String& nodeId, const String& axis) {
    return canvas.toggleLinkForAutomation(nodeId, axis);
}

bool NodeWorkspace::selectVertexForAutomation(const String& nodeId, int vertexIndex) {
    return canvas.selectVertexForAutomation(nodeId, vertexIndex);
}

bool NodeWorkspace::setVertexParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        float value) {
    return canvas.setVertexParameterForAutomation(nodeId, parameterId, value);
}

bool NodeWorkspace::getNodeParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        String& value) const {
    return canvas.getNodeParameterForAutomation(nodeId, parameterId, value);
}

var NodeWorkspace::inspectNodeControlsForAutomation(const String& nodeId) const {
    return canvas.inspectNodeControlsForAutomation(nodeId);
}

var NodeWorkspace::inspectPointerTargetsForAutomation() const {
    var result = canvas.inspectPointerTargetsForAutomation();
    auto* resultObject = result.getDynamicObject();
    if (resultObject == nullptr) {
        return result;
    }
    Array<var>* targets = resultObject->getProperty("targets").getArray();
    if (targets == nullptr) {
        return result;
    }
    if (!keyboard.isVisible()) {
        return result;
    }

    const Rectangle<float> keyboardBounds = keyboard.getBounds().toFloat();
    targets->add(pointerTarget(
            "PerformanceKeyboard.OctaveDown",
            "performanceOctave",
            keyboard.octaveDownBounds().translated(
                    keyboardBounds.getX(),
                    keyboardBounds.getY())));
    targets->add(pointerTarget(
            "PerformanceKeyboard.OctaveUp",
            "performanceOctave",
            keyboard.octaveUpBounds().translated(
                    keyboardBounds.getX(),
                    keyboardBounds.getY())));
    for (int note = keyboard.baseNote(); note <= keyboard.baseNote() + 12; ++note) {
        targets->add(pointerTarget(
                "PerformanceKeyboard.Note" + String(note),
                "performanceKey",
                keyboard.noteBounds(note).translated(
                        keyboardBounds.getX(),
                        keyboardBounds.getY())));
    }
    return result;
}

var NodeWorkspace::inspectOpenGLDiagnosticsForAutomation() const {
    return canvas.inspectOpenGLDiagnosticsForAutomation();
}

var NodeWorkspace::inspectCanvasPerformanceForAutomation() const {
    return canvas.inspectPerformanceMetricsForAutomation();
}

void NodeWorkspace::resetCanvasPerformanceForAutomation() {
    canvas.resetPerformanceMetricsForAutomation();
}

void NodeWorkspace::requestCanvasOpenGLFrameForAutomation() {
    canvas.requestOpenGLFrameForAutomation();
}

var NodeWorkspace::captureAudioForAutomation(size_t frameCount) const {
    return canvas.captureAudioForAutomation(frameCount);
}

var NodeWorkspace::performanceStateForAutomation() const {
    const auto status = audioEngine.status();
    const Rectangle<float> whiteKey = keyboard.noteBounds(keyboard.baseNote());
    const Rectangle<float> octaveButton = keyboard.octaveDownBounds();
    const float whiteKeyAspect = whiteKey.getWidth() > 0.f
            ? whiteKey.getHeight() / whiteKey.getWidth()
            : 0.f;
    auto* object = new DynamicObject();
    object->setProperty("visible", keyboard.isVisible());
    object->setProperty("baseNote", keyboard.baseNote());
    object->setProperty("highestNote", keyboard.baseNote() + 12);
    object->setProperty("baseNoteLabel", keyboard.baseNoteLabel());
    object->setProperty("highestNoteLabel", keyboard.highestNoteLabel());
    object->setProperty("heldNote", keyboard.heldNote());
    object->setProperty("heldVelocity", keyboard.heldVelocity());
    object->setProperty("audioDeviceReady", status.deviceReady);
    object->setProperty("deviceName", status.deviceName);
    object->setProperty("deviceError", status.error);
    object->setProperty("sampleRate", status.sampleRate);
    object->setProperty("blockSize", status.blockSize);
    object->setProperty("callbackCount", (int64) status.renderer.callbackCount);
    object->setProperty("graphRevision", (int64) status.renderer.graphRevision);
    object->setProperty("activeVoiceCount", (int) status.renderer.activeVoiceCount);
    object->setProperty("droppedMidiEvents", (int) status.renderer.droppedMidiEvents);
    object->setProperty("peak", status.renderer.peak);
    object->setProperty("rms", status.renderer.rms);
    object->setProperty("leftPeak", status.renderer.leftPeak);
    object->setProperty("rightPeak", status.renderer.rightPeak);
    object->setProperty(
            "hostedByCanvas",
            keyboard.getParentComponent() == &canvas);
    object->setProperty("docked", true);
    object->setProperty("screenX", keyboard.getX());
    object->setProperty("screenY", keyboard.getY());
    object->setProperty("screenWidth", keyboard.getWidth());
    object->setProperty("screenHeight", keyboard.getHeight());
    object->setProperty("whiteKeyWidth", whiteKey.getWidth());
    object->setProperty("whiteKeyHeight", whiteKey.getHeight());
    object->setProperty("whiteKeyAspect", whiteKeyAspect);
    object->setProperty("octaveButtonWidth", octaveButton.getWidth());
    object->setProperty("octaveButtonHeight", octaveButton.getHeight());
    object->setProperty(
            "occludedByExpandedEditor",
            performanceOccludedByExpandedEditor);
    return var(object);
}

var NodeWorkspace::outputMeterStateForAutomation() const {
    const auto levels = canvas.realtimeOutputMeterLevels();
    auto* object = new DynamicObject();
    object->setProperty("live", levels.has_value());
    object->setProperty("leftAmplitude", levels.has_value() ? levels->left : 0.f);
    object->setProperty("rightAmplitude", levels.has_value() ? levels->right : 0.f);
    object->setProperty(
            "leftDisplay",
            levels.has_value()
                    ? OutputMeterPresentation::displayLevelForAmplitude(levels->left)
                    : 0.f);
    object->setProperty(
            "rightDisplay",
            levels.has_value()
                    ? OutputMeterPresentation::displayLevelForAmplitude(levels->right)
                    : 0.f);
    return var(object);
}

bool NodeWorkspace::performancePointerDownForAutomation(
        int noteNumber,
        float velocity) {
    if (noteNumber < keyboard.baseNote() || noteNumber > keyboard.baseNote() + 12) {
        return false;
    }
    if (keyboard.heldNote() >= 0) {
        keyboardState.noteOff(1, keyboard.heldNote(), velocity);
    }
    keyboardState.noteOn(1, noteNumber, jlimit(0.05f, 1.f, velocity));
    return true;
}

bool NodeWorkspace::performancePointerDragForAutomation(
        int noteNumber,
        float velocity) {
    if (keyboard.heldNote() == noteNumber) {
        return true;
    }
    return performancePointerDownForAutomation(noteNumber, velocity);
}

bool NodeWorkspace::performancePointerUpForAutomation() {
    if (keyboard.heldNote() < 0) {
        return false;
    }
    keyboardState.noteOff(1, keyboard.heldNote(), 0.f);
    return true;
}

StandaloneAudioEngine::LiveCapture NodeWorkspace::captureLiveAudioForAutomation(
        int durationMs) {
    StandaloneAudioEngine::LiveCapture capture = audioEngine.captureLiveAudio(durationMs);
    updateOutputMeter(audioEngine.status());
    return capture;
}

void NodeWorkspace::resized() {
    canvas.setBounds(getLocalBounds());
    layoutPerformanceKeyboard();
}

void NodeWorkspace::timerCallback() {
    const auto status = audioEngine.status();
    updateOutputMeter(status);
    if (previousDeviceReady && !status.deviceReady) {
        keyboard.releaseAllNotes();
    }
    previousDeviceReady = status.deviceReady;
    layoutPerformanceKeyboard();

    GraphExecutionPlan plan;
    uint64_t revision {};
    if (!canvas.copyAudioPlan(plan, revision)) {
        return;
    }
    if (revision == publishedPlanRevision
            && status.preparationRevision == publishedDevicePreparationRevision) {
        return;
    }
    if (publishedPlanRevision != 0 && revision != publishedPlanRevision) {
        keyboard.releaseAllNotes();
    }
    if (audioEngine.publishGraph(std::move(plan), revision)) {
        publishedPlanRevision = revision;
        publishedDevicePreparationRevision = status.preparationRevision;
    }
}

void NodeWorkspace::updateOutputMeter(const StandaloneAudioEngine::Status& status) {
    std::optional<OutputMeterLevels> meterLevels;
    if (status.deviceReady) {
        meterLevels = OutputMeterLevels {
                status.renderer.leftPeak,
                status.renderer.rightPeak
        };
    }
    canvas.setRealtimeOutputMeterLevels(meterLevels);
}

void NodeWorkspace::layoutPerformanceKeyboard() {
    if (canvas.getWidth() <= 0 || canvas.getHeight() <= 0) {
        return;
    }
    const Rectangle<int> screenBounds = canvas.performanceKeyboardDockBounds();
    const Rectangle<float> expandedBounds = canvas.expandedEditorBoundsForOverlay();
    const bool occluded = !expandedBounds.isEmpty()
            && expandedBounds.intersects(screenBounds.toFloat());

    if (occluded && keyboard.isVisible()) {
        keyboard.releaseAllNotes();
    }
    keyboard.setBounds(screenBounds);
    keyboard.setVisible(!occluded);
    performanceOccludedByExpandedEditor = occluded;
}

}
