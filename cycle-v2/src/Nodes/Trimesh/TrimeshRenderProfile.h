#pragma once

#include "../../Graph/NodeGraph.h"

#include <Obj/Color.h>

#include <JuceHeader.h>

namespace CycleV2 {

enum class TrimeshSliceBackground {
    Waveform,
    Spectrum
};

class TrimeshRenderProfile {
public:
    static TrimeshRenderProfile fromDomain(PortDomain domain);

    PortDomain getDomain() const { return domain; }
    TrimeshSliceBackground getSliceBackground() const { return sliceBackground; }
    juce::String panel3DTitle() const;
    juce::String panel2DTitle() const;
    juce::Image gradientImage() const;
    juce::Colour surfaceColour(float value) const;
    Color positiveCurveColour() const;
    Color negativeCurveColour() const;

    bool isSpectral() const { return spectral; }
    bool isPhase() const { return phase; }
    bool curveIsBipolar() const { return curveBipolar; }
    bool surfaceTextureUsesAlpha() const { return spectral; }

private:
    explicit TrimeshRenderProfile(PortDomain domain);

    PortDomain domain { PortDomain::TimeSignal };
    TrimeshSliceBackground sliceBackground { TrimeshSliceBackground::Waveform };
    bool spectral {};
    bool phase {};
    bool curveBipolar { true };
};

}
