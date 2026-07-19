#include "TrimeshRenderProfile.h"

#include <Binary/Gradients.h>

namespace CycleV2 {

namespace {

const Color kSpectralYellow(0.85f, 0.68f, 0.23f, 0.82f);
const Color kSpectralBlue(0.44f, 0.605f, 0.88f, 0.82f);
const Color kPhasePurple(0.70f, 0.52f, 1.0f, 0.84f);
const Color kPhaseOrange(1.0f, 0.48f, 0.18f, 0.78f);
const Color kWaveformGrey(0.86f, 0.86f, 0.94f, 0.74f);

Image& blueGradientImage() {
    static Image image = PNGImageFormat::loadFrom(Gradients::blue_png, Gradients::blue_pngSize);
    return image;
}

Image& burntalumGradientImage() {
    static Image image = PNGImageFormat::loadFrom(Gradients::burntalum_png, Gradients::burntalum_pngSize);
    return image;
}

Colour sampleGradient(Image& gradient, float value) {
    if (!gradient.isValid() || gradient.getWidth() <= 0) {
        return Colour(0xff53657a);
    }

    const int x = jlimit(
            0,
            gradient.getWidth() - 1,
            roundToInt(jlimit(0.f, 1.f, value) * (float) (gradient.getWidth() - 1)));
    return gradient.getPixelAt(x, 0);
}

Color positiveCurveColourFor(bool spectral, bool phase) {
    if (phase) {
        return kPhasePurple;
    }

    if (spectral) {
        return kSpectralYellow;
    }

    return kWaveformGrey;
}

Color negativeCurveColourFor(bool spectral, bool phase, bool bipolar) {
    if (phase) {
        return kPhaseOrange;
    }

    if (spectral) {
        return bipolar ? kSpectralBlue : kSpectralYellow;
    }

    return kWaveformGrey;
}

}

TrimeshRenderProfile TrimeshRenderProfile::fromDomain(PortDomain domain) {
    RenderScalePolicy scalePolicy = RenderScalePolicy::Unipolar;

    if (domain == PortDomain::TimeSignal || domain == PortDomain::SpectralPhaseSignal) {
        scalePolicy = RenderScalePolicy::Bipolar;
    }

    return TrimeshRenderProfile({ domain, scalePolicy, RenderSemanticRole::Generic });
}

TrimeshRenderProfile TrimeshRenderProfile::fromSemantic(NodeRenderSemantic semantic) {
    return TrimeshRenderProfile(semantic);
}

Image TrimeshSurfaceStyle::gradientImage() const {
    const bool spectral = domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal;
    return spectral ? burntalumGradientImage() : blueGradientImage();
}

Colour TrimeshSurfaceStyle::colourForValue(float value) const {
    const float v = jlimit(0.f, 1.f, value);
    const bool spectral = domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal;

    if (domain == PortDomain::SpectralPhaseSignal) {
        const Colour negative = Colour(0xffff7a3d);
        const Colour centre = Colour(0xff120d18);
        const Colour positive = Colour(0xffb887ff);
        const Colour colour = v < 0.5f
                ? negative.interpolatedWith(centre, v * 2.f)
                : centre.interpolatedWith(positive, (v - 0.5f) * 2.f);
        const float distanceFromCentre = v < 0.5f ? 0.5f - v : v - 0.5f;
        return colour.withAlpha(v <= 0.f ? 0.f : jlimit(0.18f, 0.90f, 0.28f + distanceFromCentre * 1.24f));
    }

    if (spectral) {
        const float alpha = jmin(1.f, 25.f * v * v);
        return sampleGradient(burntalumGradientImage(), v).withAlpha(alpha);
    }

    return sampleGradient(blueGradientImage(), v).withAlpha(0.82f);
}

TrimeshRenderProfile::TrimeshRenderProfile(NodeRenderSemantic semantic) :
        domain      (semantic.domain)
    ,   scalePolicy (semantic.scalePolicy) {
    const bool spectral = semantic.domain == PortDomain::SpectralMagnitudeSignal
            || semantic.domain == PortDomain::SpectralPhaseSignal;
    const bool phase = semantic.domain == PortDomain::SpectralPhaseSignal;

    surfaceStyle.domain = domain;
    surfaceStyle.textureUsesAlpha = spectral;

    if (phase) {
        sliceStyle.background = TrimeshSliceBackground::SpectrumPhase;
        sliceStyle.fillColour = Colour(0xff080608).withAlpha(0.58f);
        sliceStyle.minorGridColour = Colour(0xff241b18).withAlpha(0.70f);
        sliceStyle.majorGridColour = Colour(0xff806646).withAlpha(0.34f);
        sliceStyle.panel3DTitle = "3D phase surface";
        sliceStyle.panel2DTitle = "2D phase slice";
    } else if (spectral) {
        sliceStyle.background = TrimeshSliceBackground::SpectrumMagnitude;
        sliceStyle.fillColour = Colour(0xff080608).withAlpha(0.58f);
        sliceStyle.minorGridColour = Colour(0xff241b18).withAlpha(0.70f);
        sliceStyle.majorGridColour = Colour(0xff806646).withAlpha(0.34f);
        sliceStyle.panel3DTitle = "3D magnitude surface";
        sliceStyle.panel2DTitle = "2D magnitude slice";
    } else {
        sliceStyle.background = TrimeshSliceBackground::Waveform;
        sliceStyle.fillColour = Colour(0xff05070a).withAlpha(0.58f);
        sliceStyle.minorGridColour = Colour(0xff1b2430).withAlpha(0.70f);
        sliceStyle.majorGridColour = Colour(0xff546276).withAlpha(0.34f);
        sliceStyle.panel3DTitle = "3D grid heatmap";
        sliceStyle.panel2DTitle = "2D waveshape";
    }

    surfaceStyle.minorGridColour = (spectral ? Colour(0xffd7b166) : Colour(0xffeef5ff)).withAlpha(0.08f);
    surfaceStyle.majorGridColour = (spectral ? Colour(0xffffd68a) : Colour(0xffeef5ff)).withAlpha(0.18f);

    curveStyle.bipolar = scalePolicy == RenderScalePolicy::Bipolar;
    curveStyle.cyclic = !spectral;
    curveStyle.xMinimum = curveStyle.cyclic ? -0.05f : 0.f;
    curveStyle.xMaximum = curveStyle.cyclic ? 1.05f : 1.f;
    curveStyle.positiveColour = positiveCurveColourFor(spectral, phase);
    curveStyle.negativeColour = negativeCurveColourFor(spectral, phase, curveStyle.bipolar);
}

}
