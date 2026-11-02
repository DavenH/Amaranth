#pragma once

#include <JuceHeader.h>
#include <Array/StereoBuffer.h>
#include <UI/Panels/ZoomRect.h>
#include <vector>

#include "UI/WaveformColourStyles.h"

namespace CycleV2 {

enum class WaveformPalette { Monochrome, CrossingHeat };

struct ReferenceWaveformResolution {
    size_t crossings {};
    bool useLine {};
    bool useFill {};
};

ReferenceWaveformResolution referenceWaveformResolution(const std::vector<int>& crossings,
        int start, int end, int pixelWidth, float physicalScale = 1.f);

struct ReferenceWaveformLineStyle {
    float strokeWidth { 1.f };
    float fillAlpha {};
    // Pack stroke coverage in RGB and combined stroke/fill coverage in alpha for heat composition.
    bool encodeFill {};
};

// Presentation-only mono sum, scaled to fill the waveform's signed display range.
juce::AudioBuffer<float> referenceWaveformPreview(const StereoBuffer& audio);
std::vector<float> referenceWaveformDensity(Buffer<float> mono, int width, int height,
        float y = 0.f, float span = 1.f);
juce::Image referenceWaveformHeatmap(const StereoBuffer& audio, int width, int height);
juce::Image referenceWaveformHeatmap(Buffer<float> normalizedMono, int width, int height,
        float y = 0.f, float span = 1.f);
juce::Image signalWaveformHeatmap(const std::vector<float>& mono, int width, int height);
// Full-source statistics remain fixed while the viewport and trace resolution change.
std::vector<float> signalWaveformHeatDensity(Buffer<float> normalizedMono);
juce::Image signalWaveformHeatField(const std::vector<float>& normalizedDensity, WaveformColourStyles::Style style);
juce::Image signalWaveformHeatField(Buffer<float> normalizedMono,
        WaveformColourStyles::Style style = WaveformColourStyles::current());
juce::Image recolourSignalWaveform(const juce::Image& geometry, const juce::Image& heatField,
        const ZoomRect& viewport, bool constantFill = false);
void paintWaveformImage(juce::Graphics& graphics, const juce::Image& image,
        const ZoomRect& coverageViewport, const ZoomRect& viewport, juce::Rectangle<float> bounds);
juce::Image signalWaveformCoverage(Buffer<float> visibleMono, int width, int height,
        const ZoomRect& viewport, bool useLine, juce::Range<float> sampleRange = {},
        ReferenceWaveformLineStyle style = {});
juce::Image renderSignalWaveform(Buffer<float> visibleMono, int width, int height,
        const ZoomRect& viewport, const juce::Image& heatField, bool useLine,
        juce::Range<float> sampleRange = {});
juce::Image referenceWaveformLine(Buffer<float> normalizedMono, int width, int height,
        float y = 0.f, float span = 1.f, juce::Range<float> sampleRange = {},
        ReferenceWaveformLineStyle style = {});
juce::Image loadReferenceWaveformHeatmap(const juce::File& file);

}
