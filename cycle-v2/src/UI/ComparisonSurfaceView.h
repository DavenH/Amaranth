#pragma once

#include <JuceHeader.h>
#include <UI/Panels/ZoomRect.h>

#include <functional>
#include <memory>
#include <vector>

namespace CycleV2 {

enum class PortDomain;

class ComparisonSurfaceView final : public juce::Component {
public:
    ComparisonSurfaceView();
    ~ComparisonSurfaceView() override;

    void setGrid(const std::vector<float>& values, size_t columns, size_t rows,
            PortDomain domain, int note, float displayGain = 1.f, bool includesDc = true,
            double sampleRate = 44100.0, const std::vector<float>& positions = {}, size_t maximumRows = 0);
    void setCamera(const ZoomRect& camera);
    ZoomRect camera();
    void setZoomBarsVisible(bool visible);
    juce::Rectangle<int> plotBounds() const;
    juce::Rectangle<int> zoomBounds(bool horizontal) const;
    bool renderOpenGL(float scaleFactor);
    void releaseOpenGLResources();
    juce::var automationDiagnostics() const;
    void paint(juce::Graphics& graphics) override;
    void resized() override;

    juce::String referenceId;
    std::function<void(const ZoomRect&)> onCameraChanged;
    std::function<void()> onClicked;
    std::function<void()> onDoubleClicked;
    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

}
