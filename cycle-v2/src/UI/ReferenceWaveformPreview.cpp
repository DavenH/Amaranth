#include "UI/ReferenceWaveformPreview.h"

#include <Algo/Resampling.h>
#include <Algo/PeakCounter.h>
#include <Audio/PitchedSample.h>
#include <Array/ScopedAlloc.h>
#include <Binary/Gradients.h>
#include <algorithm>
#include <array>

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

namespace {

void normalizeDensity(std::vector<float>& density) {
    Buffer<float> values(density.data(), (int) density.size());
    values.add(1.f).ln();
    const float peak = values.max();
    if (peak > 0.f) {
        values.mul(1.f / peak);
    }
}

const std::array<juce::PixelARGB, 512>& signalHeatColours() {
    static const auto colours = [] {
        const auto gradient = juce::PNGImageFormat::loadFrom(Gradients::magenta_png, Gradients::magenta_pngSize);
        std::array<juce::PixelARGB, 512> result;
        for (int index = 0; index < (int) result.size(); ++index) {
            // Keep even isolated traces above the dark canvas while retaining the dense pale highlight.
            const float position = juce::jmap((float) index / (result.size() - 1), 0.45f, 1.f);
            const int x = juce::roundToInt(position * (gradient.getWidth() - 1));
            result[(size_t) index] = gradient.getPixelAt(x, 0).getPixelARGB();
        }
        return result;
    }();
    return colours;
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

void paintWaveformHeat(juce::Image& image, const juce::Image& heat, const ZoomRect& viewport,
        const std::vector<float>& coverage) {
    const int width = image.getWidth();
    const int height = image.getHeight();
    std::vector<HeatPosition> columns((size_t) width);
    for (int x = 0; x < width; ++x) {
        columns[(size_t) x] = heatPosition(viewport.x + viewport.w * x / juce::jmax(1, width - 1),
                heat.getWidth());
    }
    std::vector<juce::PixelARGB> heatRow((size_t) heat.getWidth());
    const auto* columnLookup = columns.data();
    const auto* counts = coverage.data();
    const bool hasCoverage = !coverage.empty();
    auto* blendedRow = heatRow.data();
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::readWrite);
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
            const int alpha = hasCoverage ? (counts[(size_t) x * (size_t) height + (size_t) y] > 0.f ? 255 : 0)
                    : row[x].getAlpha();
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
        int start, int end, int pixelWidth) {
    const auto first = std::lower_bound(crossings.begin(), crossings.end(), start);
    const auto last = std::lower_bound(first, crossings.end(), end);
    const auto count = (size_t) std::distance(first, last);
    return { count, pixelWidth > 0 && count <= (size_t) pixelWidth };
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
    const float peak = juce::jmax(mono.max(), -mono.min());
    if (peak > 0.f) {
        mono.mul(1.f / peak);
    }
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

juce::Image signalWaveformHeatmap(const std::vector<float>& mono, int width, int height) {
    std::vector<float> normalized = mono;
    Buffer<float> samples(normalized.data(), (int) normalized.size());
    if (!samples.empty()) {
        const float peak = juce::jmax(samples.max(), -samples.min());
        if (peak > 0.f) {
            samples.mul(1.f / peak);
        }
    }
    if (samples.empty() || (samples.max() == 0.f && samples.min() == 0.f)) {
        return referenceWaveformHeatmap(samples, width, height);
    }
    const auto resolution = referenceWaveformResolution(
            PeakCounter::findZeroCrossings(samples, 0), 0, samples.size(), width);
    return renderSignalWaveform(samples, width, height, ZoomRect(), signalWaveformHeatField(samples),
            resolution.useLine);
}

juce::Image signalWaveformHeatField(Buffer<float> mono) {
    // A source-space grid preserves overlap richness and colour meaning at every zoom level.
    constexpr int columns = 768;
    constexpr int rows = 384;
    auto density = referenceWaveformDensity(mono, columns, rows);
    if (density.empty()) {
        return {};
    }
    normalizeDensity(density);
    const auto& colours = signalHeatColours();
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

juce::Image renderSignalWaveform(Buffer<float> mono, int width, int height,
        const ZoomRect& viewport, const juce::Image& heatField, bool useLine,
        juce::Range<float> sampleRange) {
    if (width < 1 || height < 2 || viewport.h <= 0.f || !heatField.isValid()) {
        return {};
    }
    if (useLine) {
        auto image = referenceWaveformLine(mono, width, height, viewport.y, viewport.h, sampleRange);
        if (image.isValid()) {
            paintWaveformHeat(image, heatField, viewport, {});
        }
        return image;
    }
    juce::Image image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    if (!mono.empty() && (mono.max() != 0.f || mono.min() != 0.f)) {
        const auto density = referenceWaveformDensity(mono, width, height, viewport.y, viewport.h);
        paintWaveformHeat(image, heatField, viewport, density);
    }
    return image;
}

juce::Image referenceWaveformLine(Buffer<float> mono, int width, int height,
        float y, float span, juce::Range<float> sampleRange) {
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
    graphics.setColour(CanvasChromePalette::text);
    graphics.strokePath(trace, juce::PathStrokeType(1.f));
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
