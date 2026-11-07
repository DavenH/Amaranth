#include "UI/PerformanceWaveformPreview.h"

#include "Runtime/GraphPresentationSnapshot.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

class PerformanceWaveformPreview::CaptureJob final : public juce::ThreadPoolJob {
public:
    CaptureJob(PerformanceWaveformPreview& view, const GraphPresentationSnapshot& source) :
            ThreadPoolJob("Performance waveform")
        ,   owner(&view)
        ,   revision(view.revision)
        ,   plan(source.compileResult.plan)
        ,   captures(source.waveformCaptures)
        ,   controllers(source.previewControllerValues)
        ,   midiNote(source.previewMidiNote)
        ,   releaseDuration(source.previewResult.defaultOutput.has_value()
                    ? source.previewResult.defaultOutput->releaseDuration : 0.) {}

    JobStatus runJob() override {
        if (shouldExit() || !plan.defaultOutputProbe.has_value()) {
            return jobHasFinished;
        }
        const auto result = captures->capture(plan,
                {{ *plan.defaultOutputProbe, releaseDuration, 1. }}, midiNote, controllers);
        if (!shouldExit()) {
            juce::MessageManager::callAsync([owner = owner, revision = revision,
                    samples = result.front(), span = (float) (1. + releaseDuration)] {
                if (owner != nullptr) {
                    owner->publish(revision, samples, span);
                }
            });
        }
        return jobHasFinished;
    }

private:
    juce::Component::SafePointer<PerformanceWaveformPreview> owner;
    uint64_t revision;
    GraphExecutionPlan plan;
    std::shared_ptr<SignalProbeWaveformCapture> captures;
    std::array<int, 128> controllers;
    int midiNote;
    double releaseDuration;
};

PerformanceWaveformPreview::PerformanceWaveformPreview() {
    setName("PerformanceKeyboard.Waveform");
    setTitle("Voice waveform and playback positions");
    addAndMakeVisible(waveform);
    addAndMakeVisible(overlay);
    waveform.setColourStyle(WaveformColourStyles::Style::Blues);
    waveform.onBackground = [](juce::Graphics& graphics) {
        graphics.fillAll(CanvasChromePalette::dockSurface);
    };
    waveform.setWantsKeyboardFocus(false);
    waveform.setInterceptsMouseClicks(false, false);
    overlay.setInterceptsMouseClicks(false, false);
}

PerformanceWaveformPreview::~PerformanceWaveformPreview() {
    worker.removeAllJobs(true, 2000);
}

void PerformanceWaveformPreview::setSource(const GraphPresentationSnapshot& source) {
    if (!isShowing() || (sourceIdentity == source.waveformCaptures
            && midiNote == source.previewMidiNote && controllers == source.previewControllerValues)) {
        return;
    }
    sourceIdentity = source.waveformCaptures;
    midiNote = source.previewMidiNote;
    controllers = source.previewControllerValues;
    ++revision;
    worker.removeAllJobs(true, 0);
    if (!source.compileResult.succeeded() || !source.compileResult.plan.defaultOutputProbe.has_value()) {
        waveform.setSignal({});
        return;
    }
    ++captureRequests;
    worker.addJob(new CaptureJob(*this, source), true);
}

void PerformanceWaveformPreview::publish(uint64_t completed,
        std::shared_ptr<const std::vector<float>> samples, float span) {
    if (completed != revision) {
        return;
    }
    ++publications;
    publishedRevision = completed;
    timeSpan = span;
    waveform.setSignal(std::move(samples));
    overlay.setFrame(cursors, timeSpan);
}

void PerformanceWaveformPreview::setPlaybackCursors(const PlaybackCursorFrame& frame) {
    cursors = frame;
    overlay.setFrame(frame, timeSpan);
}

void PerformanceWaveformPreview::VoiceOverlay::setFrame(const PlaybackCursorFrame& frame, float span) {
    std::array<float, PlaybackCursorFrame::capacity> next {};
    const auto nextCount = jmin(frame.timelineCount, next.size());
    for (size_t index = 0; index < nextCount; ++index) {
        const auto& voice = frame.timeline[index];
        const float time = voice.releaseTime >= 0.f
                ? 1.f + voice.time - voice.releaseTime : jmin(1.f, voice.time);
        next[index] = jlimit(0.f, 1.f, time / jmax(1.f, span));
    }
    if (positions != next || count != nextCount) {
        positions = next;
        count = nextCount;
        repaint();
    }
}

void PerformanceWaveformPreview::VoiceOverlay::paint(juce::Graphics& graphics) {
    graphics.setColour(juce::Colour(0xffa9d9ff).withAlpha(0.8f));
    for (size_t index = 0; index < count; ++index) {
        const float x = positions[index] * (getWidth() - 2.f) + 1.f;
        graphics.drawLine(x, 1.f, x, getHeight() - 1.f, 1.f);
        graphics.fillRect(x - 1.5f, 1.f, 3.f, 2.f);
    }
}

void PerformanceWaveformPreview::resized() {
    waveform.setBounds(getLocalBounds());
    overlay.setBounds(getLocalBounds());
}

juce::var PerformanceWaveformPreview::diagnostics() const {
    auto result = waveform.diagnostics();
    auto* object = result.getDynamicObject();
    object->setProperty("ready", (bool) object->getProperty("ready") && publishedRevision == revision);
    object->setProperty("background", "toolbar");
    object->setProperty("captureRequests", captureRequests);
    object->setProperty("capturePublications", publications);
    object->setProperty("cursorCount", (int) overlay.count);
    object->setProperty("visible", isShowing());
    juce::Array<juce::var> positions;
    for (size_t index = 0; index < overlay.count; ++index) {
        positions.add(overlay.positions[index]);
    }
    object->setProperty("cursorPositions", positions);
    return result;
}

}
