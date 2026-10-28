#pragma once

#include <JuceHeader.h>
#include <UI/Panels/ZoomRect.h>

#include <functional>
#include <memory>

class PitchedSample;

namespace CycleV2 {

class ReferenceWaveformView final : public juce::Component,
                                    private juce::AsyncUpdater,
                                    private juce::ComponentMovementWatcher {
public:
    ReferenceWaveformView();
    ~ReferenceWaveformView() override;
    void setSample(const PitchedSample* sample);
    void setViewport(const ZoomRect& viewport);
    juce::var diagnostics() const;
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;

private:
    struct Source;
    class RenderJob;
    void componentVisibilityChanged() override;
    void componentMovedOrResized(bool, bool) override {}
    void componentPeerChanged() override {}
    void requestImage();
    void handleAsyncUpdate() override;
    void publish(uint64_t revision, juce::Image image, ZoomRect viewport, bool line, size_t crossings);

    const PitchedSample* sampleIdentity {};
    std::shared_ptr<const Source> source;
    ZoomRect viewport;
    ZoomRect imageViewport;
    juce::Image image;
    juce::ThreadPool worker { 1 };
    uint64_t revision {};
    uint64_t publishedRevision {};
    int buildCount {};
    int sourceBuildCount {};
    float physicalScale { 1.f };
    bool line {};
    size_t crossings {};
};

}
