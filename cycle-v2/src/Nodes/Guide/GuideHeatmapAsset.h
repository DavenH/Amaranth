#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

namespace CycleV2 {

class GuideHeatmapAsset {
public:
    static constexpr size_t maximumEncodedBytes = 16u * 1024u * 1024u;
    static constexpr int maximumDimension = 4096;
    static constexpr juce::int64 maximumPixels = 8ll * 1024ll * 1024ll;

    static std::shared_ptr<const GuideHeatmapAsset> decode(
            const juce::MemoryBlock& encoded,
            juce::String filename,
            juce::String& error);

    float intensityAt(int x, int y) const;

    const juce::String& id() const { return assetId; }
    const juce::String& filename() const { return sourceFilename; }
    const juce::String& mediaType() const { return sourceMediaType; }
    const juce::MemoryBlock& encodedData() const { return encoded; }
    const juce::Image& image() const { return decodedImage; }
    int width() const { return decodedImage.getWidth(); }
    int height() const { return decodedImage.getHeight(); }

private:
    GuideHeatmapAsset(
            juce::String id,
            juce::String filename,
            juce::String mediaType,
            juce::MemoryBlock data,
            juce::Image image,
            std::vector<juce::uint8> intensity);

    juce::String assetId;
    juce::String sourceFilename;
    juce::String sourceMediaType;
    juce::MemoryBlock encoded;
    juce::Image decodedImage;
    std::vector<juce::uint8> intensity;
};

using GuideHeatmapAssetPtr = std::shared_ptr<const GuideHeatmapAsset>;

}
