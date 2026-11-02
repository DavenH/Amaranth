#pragma once

#include <JuceHeader.h>
#include <UI/Panels/ZoomRect.h>

#include <functional>
#include <memory>
#include <vector>

#include "UI/WaveformColourStyles.h"

class PitchedSample;

namespace CycleV2 {

struct ReferenceWaveformResolution;

class ReferenceWaveformView final : public juce::Component,
                                    private juce::AsyncUpdater,
                                    private juce::ComponentMovementWatcher {
public:
    ReferenceWaveformView();
    ~ReferenceWaveformView() override;
    void setSample(const PitchedSample* sample);
    void setSignal(std::shared_ptr<const std::vector<float>> samples);
    void setViewport(const ZoomRect& viewport);
    juce::var diagnostics() const;
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;
    std::function<void(juce::Graphics&)> onBackground;
    std::function<void(juce::Graphics&)> onOverlay;

private:
    struct Source;
    class RenderJob;
    void componentVisibilityChanged() override;
    void componentMovedOrResized(bool, bool) override {}
    void componentPeerChanged() override {}
    void clearPresentedImage();
    void requestImage();
    void handleAsyncUpdate() override;
    void publish(uint64_t revision, juce::Image image, juce::Image heatField, juce::Image softwareImage,
            ZoomRect viewport, ReferenceWaveformResolution resolution, double renderMs);

    const PitchedSample* sampleIdentity {};
    std::shared_ptr<const std::vector<float>> signalIdentity;
    std::shared_ptr<const Source> source;
    std::shared_ptr<const Source> imageSource;
    WaveformColourStyles::Style colourStyle { WaveformColourStyles::defaultStyle };
    ZoomRect viewport;
    ZoomRect imageViewport;
    juce::Image image;
    juce::Image heatField;
    juce::Image softwareImage;
    juce::Image softwareCoverage;
    juce::Image softwareHeat;
    juce::ThreadPool worker { 1 };
    uint64_t revision {};
    uint64_t publishedRevision {};
    int buildCount {};
    int sourceBuildCount {};
    int publicationCount {};
    int blankPaintCount {};
    int retainedImagePaintCount {};
    int gpuPaintCount {};
    int gpuHeatPaintCount {};
    double paintMs {};
    double gpuHeatPaintTotalMs {};
    double gpuHeatPaintMaxMs {};
    float physicalScale { 1.f };
    float imageScale { 1.f };
    double renderMs {};
    bool line {};
    bool filled {};
    size_t crossings {};
};

}
