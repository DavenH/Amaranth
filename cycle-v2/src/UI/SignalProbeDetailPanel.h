#pragma once

#include "UI/ComparisonSurfaceView.h"
#include "UI/SignalProbeDetailView.h"
#include "UI/Editors/PropertySegmentedSelector.h"
#include "UI/PanelOverlayIconButton.h"

namespace CycleV2 {

class SignalProbeDetailPanel final : public juce::Component {
public:
    explicit SignalProbeDetailPanel(SignalProbeDetailState& state);
    void sync(PresetPreviewView view, bool timeSignal);
    juce::var waveformDiagnostics() const;
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> pointerTargets() const;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;

    std::function<void(PresetPreviewView)> onModeChanged;
    std::function<void()> onCameraChanged;

private:
    SignalProbeDetailState& state;
    ComparisonSurfaceView waveform;
    PropertySegmentedSelector modes;
    PanelOverlayIconButton zoomAttack;
    PanelOverlayIconButton zoomFull;
};

}
