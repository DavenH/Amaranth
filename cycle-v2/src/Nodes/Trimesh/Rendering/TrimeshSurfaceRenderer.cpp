#include <algorithm>

#include <Array/VecOps.h>
#include <App/AppConstants.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegions.h>

#include "Nodes/Trimesh/Rendering/TrimeshSurfaceRenderer.h"

namespace CycleV2 {

namespace {

Image createSteppedMagnitudeImage(
        const TrimeshRenderData& renderData,
        const TrimeshRenderProfile& profile,
        bool opaque,
        int displayHeight) {
    const std::vector<float>& values = renderData.linearFrequencySurface;
    if (values.size() < (size_t) renderData.columns * (size_t) renderData.rows) {
        return {};
    }

    const int width = renderData.columns;
    const int height = jmax(2, displayHeight > 0 ? displayHeight : renderData.rows);
    const Image gradient = profile.getSurfaceStyle().gradientImage();
    if (!gradient.isValid()) {
        return {};
    }

    Image image(opaque ? Image::RGB : Image::ARGB, width, height, true);
    Image::BitmapData bitmap(image, Image::BitmapData::writeOnly);
    const Range<int> midiRange(Constants::LowestMidiNote, Constants::HighestMidiNote);
    std::vector<int> bandAtPixel((size_t) height, -1);
    std::vector<Colour> bandColours;
    std::vector<float> scaledValues((size_t) renderData.rows);
    std::vector<int> gradientIndices((size_t) renderData.rows);
    const int gradientWidth = gradient.getWidth();
    int previousNote = -1;
    int bandCount = 0;

    for (int column = 0; column < width; ++column) {
        const int midiNote = renderData.pitchSpansColumns
                ? Arithmetic::getGraphicNoteForValue(
                        (float) column / (float) (width - 1), midiRange)
                : renderData.midiNote;
        if (midiNote != previousNote) {
            std::fill(bandAtPixel.begin(), bandAtPixel.end(), -1);
            const Buffer<float> ramp = LogRegions::getDefaultRegion(midiNote);
            bandCount = jmin(renderData.rows, ramp.size()) - 1;
            bandColours.resize((size_t) bandCount);
            const float displayHeightScale = (float) height / (1.f - ramp.front());
            for (int band = 0; band < bandCount; ++band) {
                const int top = jlimit(0, height,
                        roundToInt((1.f - ramp[band + 1]) * displayHeightScale));
                const int bottom = jlimit(0, height,
                        roundToInt((1.f - ramp[band]) * displayHeightScale));
                for (int pixel = top; pixel < bottom; ++pixel) {
                    bandAtPixel[(size_t) pixel] = band;
                }
            }
            previousNote = midiNote;
        }

        const size_t columnOffset = (size_t) column * (size_t) renderData.rows;
        std::copy_n(values.data() + columnOffset, bandCount, scaledValues.data());
        Buffer<float> indicesAsFloat(scaledValues.data(), bandCount);
        indicesAsFloat.clip(0.f, (float) (gradientWidth - 1) / (float) gradientWidth)
                .mul((float) gradientWidth);
        VecOps::roundDown(indicesAsFloat, Buffer<int>(gradientIndices.data(), bandCount));
        for (int band = 0; band < bandCount; ++band) {
            bandColours[(size_t) band] = gradient.getPixelAt(
                    gradientIndices[(size_t) band], 0);
        }
        for (int pixel = 0; pixel < height; ++pixel) {
            const int band = bandAtPixel[(size_t) pixel];
            if (band >= 0) {
                bitmap.setPixelColour(column, pixel, bandColours[(size_t) band]);
            }
        }
    }
    return image;
}

}

Colour TrimeshSurfaceRenderer::colourForProfile(float value, const TrimeshRenderProfile& profile) {
    return profile.getSurfaceStyle().colourForValue(value);
}

Image TrimeshSurfaceRenderer::createTrimeshImage(
        const TrimeshRenderData& renderData,
        const TrimeshRenderProfile& profile,
        float surfaceAspectRatio,
        int displayHeight,
        bool opaque) {
    if (!renderData.canDrawSurface()) {
        return {};
    }
    if (profile.getDomain() == PortDomain::SpectralMagnitudeSignal) {
        return createSteppedMagnitudeImage(renderData, profile, opaque, displayHeight);
    }
    return createHeatmapImage(renderData, profile, opaque, surfaceAspectRatio);
}

Image TrimeshSurfaceRenderer::createHeatmapImage(
        const TrimeshRenderData& renderData,
        const TrimeshRenderProfile& profile,
        bool opaque,
        float surfaceAspectRatio) {
    if (!renderData.canDrawSurface()) {
        return {};
    }

    const std::vector<float> pitchSurface = renderData.pitchSpansColumns
            ? profile.mapPitchColumnsToDisplay(
                    renderData.surface,
                    (size_t) renderData.columns,
                    (size_t) renderData.rows)
            : std::vector<float>();
    const std::vector<float>& surface = pitchSurface.empty()
            ? renderData.surface
            : pitchSurface;

    return ScalarSurfaceMaterialEvaluator::createImage(
            surface.data(),
            (int) surface.size(),
            renderData.columns,
            renderData.rows,
            profile.getSurfaceStyle().surfaceMaterial(),
            opaque,
            surfaceAspectRatio);
}

void TrimeshSurfaceRenderer::drawHeatmap(
        Graphics& g,
        Rectangle<float> area,
        const TrimeshRenderData& renderData,
        const TrimeshRenderProfile& profile,
        bool drawGrid) {
    if (!renderData.canDrawSurface()) {
        return;
    }

    const Rectangle<float> surface = area.reduced(area.getWidth() * 0.025f, area.getHeight() * 0.06f);
    const Image heatmap = createTrimeshImage(
            renderData,
            profile,
            surface.getWidth() / jmax(1.f, surface.getHeight()),
            roundToInt(surface.getHeight()));
    if (!heatmap.isValid()) {
        return;
    }
    g.setImageResamplingQuality(Graphics::highResamplingQuality);
    g.drawImage(heatmap, surface);

    if (!drawGrid) {
        return;
    }

    const float cellWidth = surface.getWidth() / (float) renderData.columns;
    const float cellHeight = surface.getHeight() / (float) renderData.rows;

    const auto& surfaceStyle = profile.getSurfaceStyle();

    g.setColour(surfaceStyle.minorGridColour);
    const int minorHorizontalStep = jmax(1, renderData.rows / 16);
    for (int row = 0; row <= renderData.rows; row += minorHorizontalStep) {
        const float y = surface.getY() + (float) row * cellHeight;
        g.drawHorizontalLine(roundToInt(y), surface.getX(), surface.getRight());
    }

    const int minorVerticalStep = jmax(1, renderData.columns / 24);
    for (int column = 0; column <= renderData.columns; column += minorVerticalStep) {
        const float x = surface.getX() + (float) column * cellWidth;
        g.drawVerticalLine(roundToInt(x), surface.getY(), surface.getBottom());
    }

    g.setColour(surfaceStyle.majorGridColour);
    const int horizontalStep = jmax(1, renderData.rows / 4);
    for (int row = 0; row <= renderData.rows; row += horizontalStep) {
        const float y = surface.getY() + (float) row * cellHeight;
        g.drawHorizontalLine(roundToInt(y), surface.getX(), surface.getRight());
    }

    const int verticalStep = jmax(1, renderData.columns / 8);
    for (int column = 0; column <= renderData.columns; column += verticalStep) {
        const float x = surface.getX() + (float) column * cellWidth;
        g.drawVerticalLine(roundToInt(x), surface.getY(), surface.getBottom());
    }
}

}
