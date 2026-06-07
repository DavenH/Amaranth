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

struct TrimeshSurfaceStyle {
    PortDomain domain { PortDomain::TimeSignal };
    juce::Colour minorGridColour;
    juce::Colour majorGridColour;
    bool textureUsesAlpha {};

    juce::Image gradientImage() const;
    juce::Colour colourForValue(float value) const;
};

struct TrimeshCurveStyle {
    Color positiveColour;
    Color negativeColour;
    float xMinimum {};
    float xMaximum { 1.f };
    bool bipolar { true };
    bool cyclic { true };
};

struct TrimeshSliceStyle {
    TrimeshSliceBackground background { TrimeshSliceBackground::Waveform };
    juce::Colour fillColour;
    juce::Colour minorGridColour;
    juce::Colour majorGridColour;
    juce::String panel3DTitle;
    juce::String panel2DTitle;

    bool isSpectral() const { return background != TrimeshSliceBackground::Waveform; }
};

class TrimeshRenderProfile {
public:
    static TrimeshRenderProfile fromDomain(PortDomain domain);
    static TrimeshRenderProfile fromSemantic(NodeRenderSemantic semantic);

    PortDomain getDomain() const { return domain; }
    RenderScalePolicy getScalePolicy() const { return scalePolicy; }
    const TrimeshSurfaceStyle& getSurfaceStyle() const { return surfaceStyle; }
    const TrimeshCurveStyle& getCurveStyle() const { return curveStyle; }
    const TrimeshSliceStyle& getSliceStyle() const { return sliceStyle; }

private:
    explicit TrimeshRenderProfile(NodeRenderSemantic semantic);

    PortDomain domain { PortDomain::TimeSignal };
    RenderScalePolicy scalePolicy { RenderScalePolicy::Bipolar };
    TrimeshSurfaceStyle surfaceStyle;
    TrimeshCurveStyle curveStyle;
    TrimeshSliceStyle sliceStyle;
};

}
