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
            PortDomain domain, int note, float displayGain = 1.f);
    void setCamera(const ZoomRect& camera);
    ZoomRect camera();
    bool renderOpenGL(float scaleFactor);
    void releaseOpenGLResources();
    juce::var automationDiagnostics() const;
    void paint(juce::Graphics& graphics) override;
    void resized() override;

    juce::String referenceId;
    std::function<void(const ZoomRect&)> onCameraChanged;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

}
