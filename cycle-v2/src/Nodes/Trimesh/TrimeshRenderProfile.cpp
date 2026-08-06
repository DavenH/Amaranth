#include "TrimeshRenderProfile.h"

#include <Binary/Gradients.h>
#include <Util/Arithmetic.h>

#include <algorithm>
#include <cmath>

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

std::vector<float> logarithmicRowsWithoutDc(
        const std::vector<float>& source,
        size_t columns,
        size_t rows) {
    if (columns == 0 || rows < 2 || source.size() < columns * rows) {
        return source;
    }

    std::vector<float> surface(source.size());
    std::vector<float> sourceRows(rows);
    const float frequencyTension = (float) rows * 0.5f;
    for (size_t row = 0; row < rows; ++row) {
        const float unit = (float) row / (float) (rows - 1);
        const float sourceUnit = Arithmetic::invLogMapping(
                frequencyTension,
                unit,
                true);
        sourceRows[row] = jlimit(
                1.f,
                (float) (rows - 1),
                1.f + sourceUnit * (float) (rows - 2));
    }

    for (size_t column = 0; column < columns; ++column) {
        const size_t columnOffset = column * rows;
        for (size_t row = 0; row < rows; ++row) {
            const float position = sourceRows[row];
            const size_t rowA = (size_t) position;
            const size_t rowB = std::min(rowA + 1, rows - 1);
            const float amount = position - (float) rowA;
            surface[columnOffset + row] = source[columnOffset + rowA]
                    + amount * (source[columnOffset + rowB] - source[columnOffset + rowA]);
        }
    }

    return surface;
}

void unwrapPhaseColumns(std::vector<float>& surface, size_t columns, size_t rows) {
    if (columns < 2 || rows == 0 || surface.size() < columns * rows) {
        return;
    }

    for (size_t row = 0; row < rows; ++row) {
        float offset = 0.f;
        float previous = surface[row];
        for (size_t column = 1; column < columns; ++column) {
            const size_t index = column * rows + row;
            const float current = surface[index];
            const float delta = current + offset - previous;

            if (delta > MathConstants<float>::pi) {
                offset -= MathConstants<float>::twoPi;
            } else if (delta < -MathConstants<float>::pi) {
                offset += MathConstants<float>::twoPi;
            }

            surface[index] = current + offset;
            previous = surface[index];
        }
    }
}

void mapMagnitudeToDisplay(Buffer<float> values) {
    values.abs()
            .mul(16.f)
            .add(1.f)
            .ln()
            .mul(1.f / 2.833213344f)
            .clip(0.f, 1.f);
}

void mapPhaseToDisplay(Buffer<float> values) {
    float minimum {};
    float maximum {};
    int minimumIndex {};
    int maximumIndex {};
    values.getMin(minimum, minimumIndex);
    values.getMax(maximum, maximumIndex);

    const float realMaximum = jmax(std::abs(minimum), std::abs(maximum));
    if (realMaximum <= 0.f) {
        values.set(0.5f);
        return;
    }

    const float exponent = std::ceil(std::log2(realMaximum) + 0.5f);
    values.mul(std::pow(2.f, -exponent)).add(0.5f).clip(0.f, 1.f);
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

std::vector<float> TrimeshRenderProfile::mapGridToDisplay(
        const std::vector<float>& source,
        size_t columns,
        size_t rows) const {
    std::vector<float> surface = source;
    if (surface.empty()) {
        return surface;
    }

    if (domain == PortDomain::SpectralPhaseSignal) {
        unwrapPhaseColumns(surface, columns, rows);
    }
    if (domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal) {
        surface = logarithmicRowsWithoutDc(surface, columns, rows);
    }

    Buffer<float> buffer(surface.data(), (int) surface.size());
    if (domain == PortDomain::SpectralMagnitudeSignal) {
        mapMagnitudeToDisplay(buffer);
        return surface;
    }
    if (domain == PortDomain::SpectralPhaseSignal) {
        mapPhaseToDisplay(buffer);
        return surface;
    }

    mapValuesToDisplay(buffer);
    return surface;
}

void TrimeshRenderProfile::mapValuesToDisplay(Buffer<float> values) const {
    if (scalePolicy == RenderScalePolicy::Bipolar) {
        values.mul(0.5f).add(0.5f);
    }
    values.clip(0.f, 1.f);
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
