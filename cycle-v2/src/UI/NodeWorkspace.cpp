#include "NodeWorkspace.h"

namespace CycleV2 {

namespace {

constexpr int performanceKeyboardWidth = 520;
constexpr int performanceKeyboardHeight = 92;
constexpr int performanceStatusHeight = 24;
constexpr int performanceButtonWidth = 32;
constexpr int performanceMargin = 18;
constexpr int performanceStripHeight = performanceKeyboardHeight
        + performanceStatusHeight
        + performanceMargin;

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
    addAndMakeVisible(keyboard);
    addAndMakeVisible(octaveDown);
    addAndMakeVisible(octaveUp);
    addAndMakeVisible(audioStatus);

    octaveDown.setTooltip("Lower keyboard by one octave");
    octaveUp.setTooltip("Raise keyboard by one octave");
    octaveDown.onClick = [this] { keyboard.shiftOctave(-1); };
    octaveUp.onClick = [this] { keyboard.shiftOctave(1); };
    audioStatus.setJustificationType(Justification::centred);
    audioStatus.setInterceptsMouseClicks(false, false);
    startTimerHz(30);
    timerCallback();
}

NodeWorkspace::~NodeWorkspace() {
    stopTimer();
    keyboard.releaseAllNotes();
}

bool NodeWorkspace::saveGraphToFile(const File& file) {
    return canvas.saveGraphToFile(file);
}

bool NodeWorkspace::loadGraphFromFile(const File& file) {
    keyboard.releaseAllNotes();
    return canvas.loadGraphFromFile(file);
}

var NodeWorkspace::exportAutomationState() const {
    var state = canvas.exportAutomationState();
    if (auto* object = state.getDynamicObject()) {
        object->setProperty("performance", performanceStateForAutomation());
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

    const Rectangle<float> keyboardBounds = keyboard.getBounds().toFloat();
    targets->add(pointerTarget(
            "PerformanceKeyboard.OctaveDown",
            "performanceOctave",
            octaveDown.getBounds().toFloat()));
    targets->add(pointerTarget(
            "PerformanceKeyboard.OctaveUp",
            "performanceOctave",
            octaveUp.getBounds().toFloat()));
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

var NodeWorkspace::captureAudioForAutomation(size_t frameCount) const {
    return canvas.captureAudioForAutomation(frameCount);
}

var NodeWorkspace::performanceStateForAutomation() const {
    const auto status = audioEngine.status();
    auto* object = new DynamicObject();
    object->setProperty("visible", keyboard.isVisible());
    object->setProperty("baseNote", keyboard.baseNote());
    object->setProperty("highestNote", keyboard.baseNote() + 12);
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
    object->setProperty(
            "clearOfOpenGLCanvas",
            canvas.getBottom() <= keyboard.getY());
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
    return audioEngine.captureLiveAudio(durationMs);
}

void NodeWorkspace::paint(Graphics& graphics) {
    graphics.fillAll(Colour(0xff101318));
    graphics.setColour(Colour(0xff354050));
    graphics.fillRect(
            0,
            jmax(0, getHeight() - performanceStripHeight),
            getWidth(),
            1);
}

void NodeWorkspace::resized() {
    Rectangle<int> canvasBounds = getLocalBounds();
    Rectangle<int> performanceStrip = canvasBounds.removeFromBottom(performanceStripHeight);
    performanceStrip.removeFromBottom(performanceMargin);
    canvas.setBounds(canvasBounds);

    const int totalWidth = performanceKeyboardWidth + performanceButtonWidth * 2 + 12;
    Rectangle<int> strip(
            (getWidth() - totalWidth) / 2,
            performanceStrip.getY(),
            totalWidth,
            performanceStrip.getHeight());
    audioStatus.setBounds(strip.removeFromTop(performanceStatusHeight));
    octaveDown.setBounds(strip.removeFromLeft(performanceButtonWidth).reduced(2));
    octaveUp.setBounds(strip.removeFromRight(performanceButtonWidth).reduced(2));
    strip.reduce(6, 0);
    keyboard.setBounds(strip);
}

void NodeWorkspace::timerCallback() {
    const auto status = audioEngine.status();
    if (previousDeviceReady && !status.deviceReady) {
        keyboard.releaseAllNotes();
    }
    previousDeviceReady = status.deviceReady;
    const String nextStatus = !status.deviceReady
            ? "Audio device unavailable"
            : status.renderer.graphRevision == 0
                    ? "Preparing audio"
                    : "Audio ready";
    if (audioStatus.getText() != nextStatus) {
        audioStatus.setText(nextStatus, dontSendNotification);
    }

    GraphExecutionPlan plan;
    uint64_t revision {};
    if (!canvas.copyAudioPlan(plan, revision)) {
        if (status.deviceReady) {
            audioStatus.setText("Graph cannot play", dontSendNotification);
        }
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
        if (status.deviceReady) {
            audioStatus.setText("Audio ready", dontSendNotification);
        }
    }
}

}
