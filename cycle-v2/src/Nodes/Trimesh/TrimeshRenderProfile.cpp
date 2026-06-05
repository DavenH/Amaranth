#include "TrimeshRenderProfile.h"

#include <Binary/Gradients.h>

namespace CycleV2 {

namespace {

const Color kSpectralYellow(0.85f, 0.68f, 0.23f, 0.82f);
const Color kSpectralBlue(0.44f, 0.605f, 0.88f, 0.82f);
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

TrimeshRenderProfile::TrimeshRenderProfile(NodeRenderSemantic semantic) :
        domain      (semantic.domain)
    ,   scalePolicy (semantic.scalePolicy)
    ,   spectral    (semantic.domain == PortDomain::SpectralMagnitudeSignal
                     || semantic.domain == PortDomain::SpectralPhaseSignal)
    ,   phase       (semantic.domain == PortDomain::SpectralPhaseSignal) {
    sliceBackground = spectral ? TrimeshSliceBackground::Spectrum : TrimeshSliceBackground::Waveform;
    curveBipolar = scalePolicy == RenderScalePolicy::Bipolar;
}

String TrimeshRenderProfile::panel3DTitle() const {
    if (phase) {
        return "3D phase surface";
    }

    if (spectral) {
        return "3D magnitude surface";
    }

    return "3D grid heatmap";
}

String TrimeshRenderProfile::panel2DTitle() const {
    if (phase) {
        return "2D phase slice";
    }

    if (spectral) {
        return "2D magnitude slice";
    }

    return "2D waveshape";
}

Image TrimeshRenderProfile::gradientImage() const {
    return spectral ? burntalumGradientImage() : blueGradientImage();
}

Colour TrimeshRenderProfile::surfaceColour(float value) const {
    const float v = jlimit(0.f, 1.f, value);

    if (spectral) {
        const float alpha = jlimit(0.f, 0.92f, (v - 0.02f) / 0.98f * 0.92f);
        return sampleGradient(burntalumGradientImage(), v).withAlpha(alpha);
    }

    return sampleGradient(blueGradientImage(), v).withAlpha(0.82f);
}

Color TrimeshRenderProfile::positiveCurveColour() const {
    if (spectral) {
        return kSpectralYellow;
    }

    return kWaveformGrey;
}

Color TrimeshRenderProfile::negativeCurveColour() const {
    if (spectral) {
        return curveBipolar ? kSpectralBlue : kSpectralYellow;
    }

    return kWaveformGrey;
}

}
