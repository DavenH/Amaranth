#pragma once

#include "UI/ReferenceWaveformView.h"
#include "Runtime/PlaybackCursor.h"

namespace CycleV2 {

struct GraphPresentationSnapshot;
class SignalProbeWaveformCapture;

// Cached representative audition with independent, audio-clocked voice overlays.
class PerformanceWaveformPreview final : public juce::Component {
public:
    PerformanceWaveformPreview();
    ~PerformanceWaveformPreview() override;
    void setSource(const GraphPresentationSnapshot& source);
    void setPlaybackCursors(const PlaybackCursorFrame& frame);
    juce::var diagnostics() const;
    void resized() override;

private:
    class CaptureJob;
    class VoiceOverlay final : public juce::Component {
    public:
        void setFrame(const PlaybackCursorFrame& frame, float span);
        void paint(juce::Graphics& graphics) override;
        std::array<float, PlaybackCursorFrame::capacity> positions {};
        size_t count {};
    };
    void publish(uint64_t revision, std::shared_ptr<const std::vector<float>> samples, float span);

    ReferenceWaveformView waveform;
    VoiceOverlay overlay;
    juce::ThreadPool worker { 1 };
    std::shared_ptr<SignalProbeWaveformCapture> sourceIdentity;
    std::array<int, 128> controllers {};
    int midiNote { -1 };
    uint64_t revision {};
    uint64_t publishedRevision {};
    int captureRequests {};
    int publications {};
    float timeSpan { 1.f };
    PlaybackCursorFrame cursors;
};

}
