#include "Nodes/Guide/GuideHeatmapAsset.h"

namespace CycleV2 {

namespace {

juce::String mediaTypeFor(const juce::MemoryBlock& data) {
    const auto* bytes = static_cast<const uint8_t*>(data.getData());
    if (data.getSize() >= 8
            && bytes[0] == 0x89u && bytes[1] == 0x50u && bytes[2] == 0x4eu && bytes[3] == 0x47u
            && bytes[4] == 0x0du && bytes[5] == 0x0au && bytes[6] == 0x1au && bytes[7] == 0x0au) {
        return "image/png";
    }
    if (data.getSize() >= 3 && bytes[0] == 0xffu && bytes[1] == 0xd8u && bytes[2] == 0xffu) {
        return "image/jpeg";
    }
    return {};
}

}

GuideHeatmapAsset::GuideHeatmapAsset(
        juce::String id,
        juce::String filename,
        juce::String mediaType,
        juce::MemoryBlock data,
        juce::Image image,
        std::vector<juce::uint8> values) :
        assetId        (std::move(id))
    ,   sourceFilename (std::move(filename))
    ,   sourceMediaType(std::move(mediaType))
    ,   encoded        (std::move(data))
    ,   decodedImage   (std::move(image))
    ,   intensity      (std::move(values)) {}

std::shared_ptr<const GuideHeatmapAsset> GuideHeatmapAsset::decode(
        const juce::MemoryBlock& data,
        juce::String filename,
        juce::String& error) {
    if (data.getSize() == 0 || data.getSize() > maximumEncodedBytes) {
        error = "Image must be between 1 byte and 16 MiB";
        return nullptr;
    }
    const juce::String mediaType = mediaTypeFor(data);
    if (mediaType.isEmpty()) {
        error = "Only PNG and JPEG images are supported";
        return nullptr;
    }
    juce::Image image = juce::ImageFileFormat::loadFrom(data.getData(), data.getSize());
    const int width = image.getWidth();
    const int height = image.getHeight();
    if (image.isNull()) {
        error = "The image could not be decoded";
        return nullptr;
    }
    if (width > maximumDimension || height > maximumDimension
            || (juce::int64) width * (juce::int64) height > maximumPixels) {
        error = "Image dimensions exceed the Guide heatmap limit";
        return nullptr;
    }

    std::vector<juce::uint8> intensity((size_t) width * (size_t) height);
    juce::Image scalarImage(juce::Image::ARGB, width, height, false);
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData scalarPixels(scalarImage, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const juce::Colour colour = pixels.getPixelColour(x, y);
            const float luminance = 0.2126f * (float) colour.getRed()
                    + 0.7152f * (float) colour.getGreen()
                    + 0.0722f * (float) colour.getBlue();
            const juce::uint8 value = (juce::uint8) juce::roundToInt(
                    luminance * colour.getFloatAlpha());
            intensity[(size_t) y * (size_t) width + (size_t) x] = value;
            scalarPixels.setPixelColour(
                    x,
                    y,
                    juce::Colour(value, value, value));
        }
    }
    error.clear();
    return std::shared_ptr<const GuideHeatmapAsset>(new GuideHeatmapAsset(
            "sha256:" + juce::SHA256(data).toHexString(),
            filename,
            mediaType,
            data,
            std::move(scalarImage),
            std::move(intensity)));
}

float GuideHeatmapAsset::intensityAt(int x, int y) const {
    if (decodedImage.isNull()) {
        return 0.f;
    }
    x = juce::jlimit(0, width() - 1, x);
    y = juce::jlimit(0, height() - 1, y);
    return (float) intensity[(size_t) y * (size_t) width() + (size_t) x] / 255.f;
}

}
