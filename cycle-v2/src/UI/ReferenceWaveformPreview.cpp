#include "UI/ReferenceWaveformPreview.h"

#include <Algo/Resampling.h>
#include <Algo/PeakCounter.h>
#include <Audio/PitchedSample.h>
#include <Array/ScopedAlloc.h>
#include <algorithm>
#include <array>

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

namespace {

constexpr int kHeatRows = 384;

float normalizeWaveform(Buffer<float> samples) {
    const float peak = samples.empty() ? 0.f : juce::jmax(samples.max(), -samples.min());
    if (peak > 0.f) {
        samples.mul(1.f / peak);
    }
    return peak;
}

void normalizeDensity(std::vector<float>& density) {
    Buffer<float> values(density.data(), (int) density.size());
    values.add(1.f).ln();
    const float peak = values.max();
    if (peak > 0.f) {
        values.mul(1.f / peak);
    }
}

std::array<juce::PixelARGB, 512> signalHeatColours(WaveformColourStyles::Style style) {
    const auto gradient = WaveformColourStyles::gradient(style);
    std::array<juce::PixelARGB, 512> result;
    for (int index = 0; index < (int) result.size(); ++index) {
        result[(size_t) index] = gradient.getPixelAt(index, 0).withAlpha(1.f).getPixelARGB();
    }
    return result;
}

struct HeatPosition {
    int index {};
    unsigned int fraction {};
};

HeatPosition heatPosition(float unit, int size) {
    const float position = juce::jlimit(0.f, (float) (size - 1), unit * (size - 1));
    const int index = juce::jmin(size - 2, (int) position);
    return { index, (unsigned int) juce::roundToInt((position - index) * 256.f) };
}

void paintWaveformHeat(juce::Image& image, const juce::Image& heat, const ZoomRect& viewport, bool filled) {
    const int width = image.getWidth();
    const int height = image.getHeight();
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::readWrite);
    if (filled) {
        const auto neutral = CanvasChromePalette::text.getPixelARGB();
        for (int y = 0; y < height; ++y) {
            auto* row = reinterpret_cast<juce::PixelARGB*>(pixels.getLinePointer(y));
            for (int x = 0; x < width; ++x) {
                const int alpha = row[x].getAlpha();
                const int fill = alpha - row[x].getRed();
                auto colour = neutral;
                colour.multiplyAlpha(alpha);
                colour.setARGB((juce::uint8) juce::jmin(255,
                                alpha + WaveformColourStyles::fillGridBacking * fill),
                        colour.getRed(), colour.getGreen(), colour.getBlue());
                row[x] = colour;
            }
        }
        return;
    }
    std::vector<HeatPosition> columns((size_t) width);
    for (int x = 0; x < width; ++x) {
        columns[(size_t) x] = heatPosition(viewport.x + viewport.w * x / juce::jmax(1, width - 1),
                heat.getWidth());
    }
    std::vector<juce::PixelARGB> heatRow((size_t) heat.getWidth());
    const auto* columnLookup = columns.data();
    auto* blendedRow = heatRow.data();
    juce::Image::BitmapData colours(heat, juce::Image::BitmapData::readOnly);
    for (int y = 0; y < height; ++y) {
        const auto position = heatPosition(viewport.y + viewport.h * y / (height - 1), heat.getHeight());
        const auto* upper = reinterpret_cast<const juce::PixelARGB*>(colours.getLinePointer(position.index));
        const auto* lower = reinterpret_cast<const juce::PixelARGB*>(colours.getLinePointer(position.index + 1));
        for (int x = 0; x < heat.getWidth(); ++x) {
            blendedRow[x] = upper[x];
            blendedRow[x].tween(lower[x], position.fraction);
        }
        auto* row = reinterpret_cast<juce::PixelARGB*>(pixels.getLinePointer(y));
        for (int x = 0; x < width; ++x) {
            const int alpha = row[x].getAlpha();
            if (alpha > 0) {
                const auto column = columnLookup[x];
                auto colour = blendedRow[column.index];
                colour.tween(blendedRow[column.index + 1], column.fraction);
                if (alpha < 255) {
                    colour.multiplyAlpha(alpha);
                }
                row[x] = colour;
            }
        }
    }
}

}

ReferenceWaveformResolution referenceWaveformResolution(const std::vector<int>& crossings,
        int start, int end, int pixelWidth, float physicalScale) {
    const auto first = std::lower_bound(crossings.begin(), crossings.end(), start);
    const auto last = std::lower_bound(first, crossings.end(), end);
    const auto count = (size_t) std::distance(first, last);
    const bool useLine = pixelWidth > 0 && count <= (size_t) pixelWidth;
    const bool useFill = useLine && physicalScale > 0.f
            && count <= (size_t) ((float) pixelWidth / (2.f * physicalScale));
    return { count, useLine, useFill };
}

juce::AudioBuffer<float> referenceWaveformPreview(const StereoBuffer& audio) {
    juce::AudioBuffer<float> result(1, audio.left.size());
    if (audio.left.empty()) {
        return result;
    }
    Buffer<float> mono(result, 0);
    audio.left.copyTo(mono);
    if (audio.numChannels > 1) {
        mono.add(audio.right);
    }
    normalizeWaveform(mono);
    return result;
}

std::vector<float> referenceWaveformDensity(Buffer<float> mono, int width, int height,
        float y, float span) {
    if (mono.empty() || width < 1 || height < 2 || span <= 0.f) {
        return {};
    }
    const int stride = height + 1;
    std::vector<float> differences((size_t) stride, 0.f);
    std::vector<float> density((size_t) width * (size_t) height, 0.f);
    const float sampleStep = (float) (mono.size() - 1) / (float) width;
    const float yScale = (float) (height - 1) / span;
    for (int x = 0; x < width; ++x) {
        std::fill(differences.begin(), differences.end(), 0.f);
        auto* column = differences.data();
        const float start = (float) x * sampleStep;
        const float end = juce::jmin((float) (mono.size() - 1), (float) (x + 1) * sampleStep);
        float previous = Resampling::lerpC(mono, start / (float) mono.size());
        const auto accumulate = [&](float next) {
            const float top = 0.5f * (1.f - juce::jmax(previous, next));
            const float bottom = 0.5f * (1.f - juce::jmin(previous, next));
            if (bottom < y || top > y + span) {
                previous = next;
                return;
            }
            const int first = juce::jlimit(0, height - 1,
                    (int) ((top - y) * yScale));
            const int last = juce::jlimit(0, height - 1,
                    (int) ((bottom - y) * yScale));
            column[first] += 1.f;
            column[last + 1] -= 1.f;
            previous = next;
        };
        for (int index = (int) start + 1; index <= (int) end; ++index) {
            accumulate(mono[index]);
        }
        if (end > (float) (int) end || start == end) {
            accumulate(Resampling::lerpC(mono, end / (float) mono.size()));
        }
        float count = 0.f;
        for (int y = 0; y < height; ++y) {
            count += column[y];
            density[(size_t) x * (size_t) height + (size_t) y] = count;
        }
    }
    return density;
}

juce::Image referenceWaveformHeatmap(const StereoBuffer& audio, int width, int height) {
    auto preview = referenceWaveformPreview(audio);
    return referenceWaveformHeatmap(Buffer<float>(preview, 0), width, height);
}

juce::Image referenceWaveformHeatmap(Buffer<float> mono, int width, int height,
        float y, float span) {
    if (width < 1 || height < 2) {
        return {};
    }
    juce::Image image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    if (mono.empty() || (mono.max() == 0.f && mono.min() == 0.f)) {
        return image;
    }
    auto density = referenceWaveformDensity(mono, width, height, y, span);
    if (density.empty()) {
        return image;
    }
    normalizeDensity(density);
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < height; ++y) {
        auto* row = reinterpret_cast<juce::PixelARGB*>(pixels.getLinePointer(y));
        for (int x = 0; x < width; ++x) {
            const float value = density[(size_t) x * (size_t) height + (size_t) y];
            if (value > 0.f) {
                row[x] = CanvasChromePalette::text.withAlpha(0.15f + 0.85f * value).getPixelARGB();
            }
        }
    }
    return image;
}

WaveformPreviewSource::WaveformPreviewSource(Buffer<float> samples) :
        mono(1, samples.size()) {
    Buffer<float> normalized(mono, 0);
    samples.copyTo(normalized);
    silent = normalizeWaveform(normalized) == 0.f;
    zeroCrossings = PeakCounter::findZeroCrossings(normalized, 0);
}

WaveformPreviewSource::WaveformPreviewSource(const StereoBuffer& audio) :
        mono(referenceWaveformPreview(audio))
    ,   palette(WaveformPalette::Monochrome) {
    Buffer<float> samples(mono, 0);
    silent = samples.empty() || (samples.max() == 0.f && samples.min() == 0.f);
    zeroCrossings = PeakCounter::findZeroCrossings(samples, 0);
}

const juce::Image& WaveformPreviewSource::coloursFor(WaveformColourStyles::Style style) const {
    if (density.empty()) {
        density = signalWaveformHeatDensity(Buffer<float>(
                const_cast<float*>(mono.getReadPointer(0)), mono.getNumSamples()));
        ++heatBuilds;
    }
    if (!heatField.isValid() || heatStyle != style) {
        heatField = signalWaveformHeatField(density, style);
        heatStyle = style;
        ++colourBuilds;
    }
    return heatField;
}

juce::Image signalWaveformHeatmap(const std::vector<float>& mono, int width, int height) {
    const WaveformPreviewSource source(Buffer<float>(const_cast<float*>(mono.data()), (int) mono.size()));
    return signalWaveformHeatmap(source, width, height);
}

juce::Image signalWaveformHeatmap(const WaveformPreviewSource& source, int width, int height,
        WaveformColourStyles::Style style) {
    const auto geometry = signalWaveformCoverage(source, width, height);
    return source.silent || !geometry.isValid() ? geometry
            : recolourSignalWaveform(geometry, source.coloursFor(style), ZoomRect());
}

juce::Image signalWaveformCoverage(const WaveformPreviewSource& source, int width, int height) {
    Buffer<float> samples(const_cast<float*>(source.mono.getReadPointer(0)), source.mono.getNumSamples());
    if (source.silent) {
        return referenceWaveformHeatmap(samples, width, height);
    }
    const auto resolution = referenceWaveformResolution(source.zeroCrossings, 0, samples.size(), width);
    ++source.geometryBuilds;
    return signalWaveformCoverage(samples, width, height, ZoomRect(), resolution.useLine);
}

std::vector<float> signalWaveformHeatDensity(Buffer<float> mono) {
    auto density = referenceWaveformDensity(mono, waveformHeatColumns, kHeatRows);
    if (!density.empty()) {
        normalizeDensity(density);
    }
    return density;
}

juce::Image signalWaveformHeatField(Buffer<float> mono, WaveformColourStyles::Style style) {
    return signalWaveformHeatField(signalWaveformHeatDensity(mono), style);
}

juce::Image signalWaveformHeatField(const std::vector<float>& density, WaveformColourStyles::Style style) {
    constexpr int columns = waveformHeatColumns;
    constexpr int rows = kHeatRows;
    if (density.size() != (size_t) columns * rows) {
        return {};
    }
    const auto colours = signalHeatColours(style);
    juce::Image heat(juce::Image::ARGB, columns, rows, false, juce::SoftwareImageType());
    juce::Image::BitmapData pixels(heat, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < rows; ++y) {
        auto* row = reinterpret_cast<juce::PixelARGB*>(pixels.getLinePointer(y));
        for (int x = 0; x < columns; ++x) {
            const float value = density[(size_t) x * rows + (size_t) y];
            row[x] = colours[(size_t) juce::jlimit(0, 511, juce::roundToInt(value * 511.f))];
        }
    }
    return heat;
}

void paintWaveformImage(juce::Graphics& graphics, const juce::Image& image,
        const ZoomRect& imageViewport, const ZoomRect& viewport, juce::Rectangle<float> bounds) {
    if (!image.isValid() || viewport.w <= 0.f || viewport.h <= 0.f) {
        return;
    }
    const juce::Rectangle<float> imageBounds {
            bounds.getX() + (imageViewport.x - viewport.x) / viewport.w * bounds.getWidth(),
            bounds.getY() + (imageViewport.y - viewport.y) / viewport.h * bounds.getHeight(),
            imageViewport.w / viewport.w * bounds.getWidth(),
            imageViewport.h / viewport.h * bounds.getHeight() };
    juce::Graphics::ScopedSaveState saved(graphics);
    graphics.reduceClipRegion(bounds.toNearestInt());
    graphics.drawImage(image, imageBounds, juce::RectanglePlacement::stretchToFit);
}

juce::Image recolourSignalWaveform(const juce::Image& geometry, const juce::Image& heatField,
        const ZoomRect& viewport, bool filled) {
    auto image = geometry.createCopy();
    paintWaveformHeat(image, heatField, viewport, filled);
    return image;
}

juce::Image renderSignalWaveform(Buffer<float> mono, int width, int height,
        const ZoomRect& viewport, const juce::Image& heatField, bool useLine,
        juce::Range<float> sampleRange) {
    if (!heatField.isValid()) {
        return {};
    }
    auto image = signalWaveformCoverage(mono, width, height, viewport, useLine, sampleRange);
    if (image.isValid()) {
        paintWaveformHeat(image, heatField, viewport, false);
    }
    return image;
}

juce::Image signalWaveformCoverage(Buffer<float> mono, int width, int height,
        const ZoomRect& viewport, bool useLine, juce::Range<float> sampleRange, ReferenceWaveformLineStyle style) {
    if (width < 1 || height < 2 || viewport.h <= 0.f) {
        return {};
    }
    if (useLine) {
        return referenceWaveformLine(mono, width, height, viewport.y, viewport.h, sampleRange, style);
    }
    juce::Image image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    if (!mono.empty() && (mono.max() != 0.f || mono.min() != 0.f)) {
        const auto density = referenceWaveformDensity(mono, width, height, viewport.y, viewport.h);
        juce::Image::BitmapData pixels(image, juce::Image::BitmapData::writeOnly);
        const auto white = juce::Colours::white.getPixelARGB();
        for (int y = 0; y < height; ++y) {
            auto* row = reinterpret_cast<juce::PixelARGB*>(pixels.getLinePointer(y));
            for (int x = 0; x < width; ++x) {
                if (density[(size_t) x * (size_t) height + (size_t) y] > 0.f) {
                    row[x] = white;
                }
            }
        }
    }
    return image;
}

juce::Image referenceWaveformLine(Buffer<float> mono, int width, int height,
        float y, float span, juce::Range<float> sampleRange, ReferenceWaveformLineStyle style) {
    if (mono.empty() || width < 1 || height < 2 || span <= 0.f) {
        return {};
    }
    juce::Image image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    ScopedAlloc<float> memory(mono.size() * 2);
    auto horizontal = memory.place(mono.size());
    auto vertical = memory.place(mono.size());
    if (sampleRange.isEmpty()) {
        sampleRange = { 0.f, (float) juce::jmax(1, mono.size() - 1) };
    }
    const float xScale = (float) (width - 1) / sampleRange.getLength();
    horizontal.ramp(-sampleRange.getStart() * xScale, xScale);
    mono.copyTo(vertical);
    vertical.mul(-0.5f).add(0.5f - y).mul((float) (height - 1) / span);
    juce::Path trace;
    trace.preallocateSpace(mono.size() * 3);
    trace.startNewSubPath(horizontal[0], vertical[0]);
    for (int index = 1; index < mono.size(); ++index) {
        trace.lineTo(horizontal[index], vertical[index]);
    }
    if (sampleRange.getEnd() > (float) (mono.size() - 1)) {
        trace.lineTo((float) (width - 1), vertical.back());
    }
    juce::Graphics graphics(image);
    if (style.fillAlpha > 0.f) {
        const float baseline = (0.5f - y) * (float) (height - 1) / span;
        auto area = trace;
        area.lineTo(trace.getCurrentPosition().x, baseline);
        area.lineTo(horizontal[0], baseline);
        area.closeSubPath();
        graphics.setColour((style.encodeFill ? juce::Colours::black : CanvasChromePalette::text)
                .withAlpha(style.fillAlpha));
        graphics.fillPath(area);
    }
    graphics.setColour(style.encodeFill ? juce::Colours::white : CanvasChromePalette::text);
    graphics.strokePath(trace, juce::PathStrokeType(style.strokeWidth));
    return image;
}

juce::Image loadReferenceWaveformHeatmap(const juce::File& file) {
    PitchedSample sample;
    if (sample.load(file.getFullPathName(), 5.0) < 0) {
        return {};
    }
    return referenceWaveformHeatmap(sample.audio, 512, 96);
}

}
