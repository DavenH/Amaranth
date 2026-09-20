#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

#include <Util/Arithmetic.h>
#include <Util/LogRegionMapping.h>

#include <algorithm>
#include <cmath>

namespace CycleV2 {

namespace {

const Color kSpectralYellow(0.85f, 0.68f, 0.23f, 0.82f);
const Color kSpectralBlue(0.44f, 0.605f, 0.88f, 0.82f);
const Color kPhasePurple(0.70f, 0.52f, 1.0f, 0.84f);
const Color kPhaseOrange(1.0f, 0.48f, 0.18f, 0.78f);
const Color kWaveformGrey(0.86f, 0.86f, 0.94f, 0.74f);

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

std::vector<float> logarithmicSourceRows(
        size_t outputRows,
        size_t sourceRowCount,
        int midiNote,
        bool excludeDc) {
    std::vector<float> sourceRows(outputRows);
    LogRegionMapping(midiNote).fillSourceUnits(Buffer<float>(
            sourceRows.data(),
            (int) sourceRows.size()));
    const float firstSourceRow = excludeDc ? 1.f : 0.f;
    Buffer<float>(sourceRows.data(), (int) sourceRows.size())
            .mul((float) (sourceRowCount - 1) - firstSourceRow)
            .add(firstSourceRow)
            .clip(firstSourceRow, (float) (sourceRowCount - 1));
    return sourceRows;
}

void resampleLogarithmicColumn(
        const float* source,
        float* destination,
        size_t sourceRowCount,
        const std::vector<float>& sourceRows) {
    for (size_t row = 0; row < sourceRows.size(); ++row) {
        const float position = sourceRows[row];
        const size_t rowA = (size_t) position;
        const size_t rowB = std::min(rowA + 1, sourceRowCount - 1);
        const float amount = position - (float) rowA;
        destination[row] = source[rowA]
                + amount * (source[rowB] - source[rowA]);
    }
}

std::vector<float> logarithmicRowsToDisplay(
        const std::vector<float>& source,
        size_t columns,
        size_t rows,
        int midiNote,
        bool excludeDc) {
    if (columns == 0 || rows < 2 || source.size() < columns * rows) {
        return source;
    }

    std::vector<float> surface(source.size());
    const std::vector<float> sourceRows = logarithmicSourceRows(
            rows, rows, midiNote, excludeDc);
    for (size_t column = 0; column < columns; ++column) {
        const size_t columnOffset = column * rows;
        resampleLogarithmicColumn(
                source.data() + columnOffset,
                surface.data() + columnOffset,
                rows,
                sourceRows);
    }

    return surface;
}

std::vector<float> pitchColumnsToDisplay(
        const std::vector<float>& source,
        size_t columns,
        size_t rows) {
    if (columns < 2 || rows < 2 || source.size() < columns * rows) {
        return source;
    }

    std::vector<float> surface(source.size());
    std::vector<float> sourceRows;
    const Range<int> midiRange(
            Constants::LowestMidiNote,
            Constants::HighestMidiNote);
    int previousNote = -1;
    size_t harmonicCount = 0;
    for (size_t column = 0; column < columns; ++column) {
        const float x = (float) column / (float) (columns - 1);
        const int note = Arithmetic::getGraphicNoteForValue(x, midiRange);
        if (note != previousNote) {
            harmonicCount = (size_t) LogRegionMapping(note).regionSize();
            sourceRows = logarithmicSourceRows(rows, harmonicCount, note, false);
            previousNote = note;
        }
        const size_t columnOffset = column * rows;
        resampleLogarithmicColumn(
                source.data() + columnOffset,
                surface.data() + columnOffset,
                harmonicCount,
                sourceRows);
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

void mapMagnitudeToDisplay(Buffer<float> values, float tension) {
    values.abs();
    Arithmetic::applyLogMapping(values, tension);
    values.clip(0.f, 1.f);
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

std::vector<float> mapSpectralValuesToDisplay(
        const std::vector<float>& source,
        size_t columns,
        size_t rows,
        PortDomain domain,
        float magnitudeTension) {
    std::vector<float> surface = source;
    if (domain == PortDomain::SpectralPhaseSignal) {
        unwrapPhaseColumns(surface, columns, rows);
    }

    Buffer<float> buffer(surface.data(), (int) surface.size());
    if (domain == PortDomain::SpectralMagnitudeSignal) {
        mapMagnitudeToDisplay(buffer, magnitudeTension);
    } else {
        mapPhaseToDisplay(buffer);
    }
    return surface;
}

std::vector<float> mapSpectralGridToDisplay(
        const std::vector<float>& source,
        size_t columns,
        size_t rows,
        PortDomain domain,
        float magnitudeTension,
        int midiNote) {
    return logarithmicRowsToDisplay(
            mapSpectralValuesToDisplay(
                    source,
                    columns,
                    rows,
                    domain,
                    magnitudeTension),
            columns,
            rows,
            midiNote,
            true);
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
        size_t rows,
        int midiNote) const {
    std::vector<float> surface = source;
    if (surface.empty()) {
        return surface;
    }

    if (domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal) {
        surface = mapTrimeshValuesToDisplay(surface);
        return logarithmicRowsToDisplay(
                surface,
                columns,
                rows,
                midiNote,
                false);
    }

    Buffer<float> buffer(surface.data(), (int) surface.size());
    mapValuesToDisplay(buffer);
    return surface;
}

std::vector<float> TrimeshRenderProfile::mapSpectrum2DGridToDisplay(
        const std::vector<float>& source,
        size_t columns,
        size_t rows,
        int midiNote) const {
    if (source.empty()) {
        return source;
    }
    if (domain != PortDomain::SpectralMagnitudeSignal
            && domain != PortDomain::SpectralPhaseSignal) {
        return mapGridToDisplay(source, columns, rows, midiNote);
    }
    return mapSpectralGridToDisplay(
            source,
            columns,
            rows,
            domain,
            500.f,
            midiNote);
}

std::vector<float> TrimeshRenderProfile::mapTrimeshValuesToDisplay(
        const std::vector<float>& source) const {
    if (source.empty()) {
        return source;
    }
    std::vector<float> surface = source;
    Buffer<float> values(surface.data(), (int) surface.size());
    if (domain == PortDomain::SpectralMagnitudeSignal) {
        values.clip(0.f, 1.f);
    } else if (domain == PortDomain::SpectralPhaseSignal) {
        values.mul(0.5f).add(0.5f).clip(0.f, 1.f);
    } else {
        mapValuesToDisplay(values);
    }
    return surface;
}

std::vector<float> TrimeshRenderProfile::mapPitchColumnsToDisplay(
        const std::vector<float>& source,
        size_t columns,
        size_t rows) const {
    if (!sliceStyle.isSpectral()) {
        return source;
    }
    return pitchColumnsToDisplay(source, columns, rows);
}

float TrimeshRenderProfile::displayFrequencyUnit(float sourceUnit, int midiNote) const {
    return LogRegionMapping(midiNote).displayUnitForSourceUnit(sourceUnit);
}

void TrimeshRenderProfile::mapValuesToDisplay(Buffer<float> values) const {
    if (scalePolicy == RenderScalePolicy::Bipolar) {
        values.mul(0.5f).add(0.5f);
    }
    values.clip(0.f, 1.f);
}

Image TrimeshSurfaceStyle::gradientImage() const {
    return ScalarSurfaceMaterialEvaluator::createGradientImage(surfaceMaterial());
}

Colour TrimeshSurfaceStyle::colourForValue(float value) const {
    return ScalarSurfaceMaterialEvaluator::colourFor(
            value,
            {},
            surfaceMaterial());
}

ScalarSurfaceMaterial TrimeshSurfaceStyle::surfaceMaterial() const {
    if (domain == PortDomain::SpectralPhaseSignal) {
        return ScalarSurfaceMaterial::bipolarPhase();
    }
    if (domain == PortDomain::SpectralMagnitudeSignal) {
        return ScalarSurfaceMaterial::unipolarMagnitude();
    }
    return ScalarSurfaceMaterial::signedAmplitude();
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
    } else if (spectral) {
        sliceStyle.background = TrimeshSliceBackground::SpectrumMagnitude;
        sliceStyle.fillColour = Colour(0xff080608).withAlpha(0.58f);
        sliceStyle.minorGridColour = Colour(0xff241b18).withAlpha(0.70f);
        sliceStyle.majorGridColour = Colour(0xff806646).withAlpha(0.34f);
    } else {
        sliceStyle.background = TrimeshSliceBackground::Waveform;
        sliceStyle.fillColour = Colour(0xff05070a).withAlpha(0.58f);
        sliceStyle.minorGridColour = Colour(0xff1b2430).withAlpha(0.70f);
        sliceStyle.majorGridColour = Colour(0xff546276).withAlpha(0.34f);
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
