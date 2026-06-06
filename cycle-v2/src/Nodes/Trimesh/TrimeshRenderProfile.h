#pragma once

#include "../../Graph/GraphRenderSemanticResolver.h"
#include "../../Graph/NodeGraph.h"

#include <Obj/Color.h>

#include <JuceHeader.h>

namespace CycleV2 {

enum class TrimeshSliceBackground {
    Waveform,
    SpectrumMagnitude,
    SpectrumPhase
};

class TrimeshRenderProfile {
public:
    static TrimeshRenderProfile fromDomain(PortDomain domain);
    static TrimeshRenderProfile fromSemantic(NodeRenderSemantic semantic);

    PortDomain getDomain() const { return domain; }
    RenderScalePolicy getScalePolicy() const { return scalePolicy; }
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
    bool curveIsCyclic() const { return curveCyclic; }
    bool surfaceTextureUsesAlpha() const { return spectral; }

private:
    explicit TrimeshRenderProfile(NodeRenderSemantic semantic);

    PortDomain domain { PortDomain::TimeSignal };
    RenderScalePolicy scalePolicy { RenderScalePolicy::Bipolar };
    TrimeshSliceBackground sliceBackground { TrimeshSliceBackground::Waveform };
    bool spectral {};
    bool phase {};
    bool curveBipolar { true };
    bool curveCyclic { true };
};

}
