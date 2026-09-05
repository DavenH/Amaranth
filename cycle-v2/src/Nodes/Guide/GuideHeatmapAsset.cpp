#include <array>
#include <cstdint>
#include <cstring>

#include "Nodes/Guide/GuideHeatmapAsset.h"

namespace CycleV2 {

namespace {

constexpr float kDisplayGain = 3.2f;

constexpr std::array<uint32_t, 64> kRoundConstants {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

uint32_t rotateRight(uint32_t value, int amount) {
    return (value >> amount) | (value << (32 - amount));
}

void processBlock(const uint8_t* block, std::array<uint32_t, 8>& state) {
    std::array<uint32_t, 64> words {};
    for (int index = 0; index < 16; ++index) {
        const uint8_t* source = block + index * 4;
        words[(size_t) index] = ((uint32_t) source[0] << 24u)
                | ((uint32_t) source[1] << 16u)
                | ((uint32_t) source[2] << 8u)
                | (uint32_t) source[3];
    }
    for (int index = 16; index < 64; ++index) {
        const uint32_t previous = words[(size_t) index - 15];
        const uint32_t recent = words[(size_t) index - 2];
        const uint32_t sigma0 = rotateRight(previous, 7) ^ rotateRight(previous, 18) ^ (previous >> 3u);
        const uint32_t sigma1 = rotateRight(recent, 17) ^ rotateRight(recent, 19) ^ (recent >> 10u);
        words[(size_t) index] = words[(size_t) index - 16] + sigma0
                + words[(size_t) index - 7] + sigma1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];
    for (int index = 0; index < 64; ++index) {
        const uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
        const uint32_t choice = (e & f) ^ (~e & g);
        const uint32_t first = h + sum1 + choice + kRoundConstants[(size_t) index] + words[(size_t) index];
        const uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t second = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + second;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

juce::String sha256(const juce::MemoryBlock& data) {
    std::array<uint32_t, 8> state {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    const auto* bytes = static_cast<const uint8_t*>(data.getData());
    const size_t completeBytes = data.getSize() & ~size_t(63);
    for (size_t offset = 0; offset < completeBytes; offset += 64) {
        processBlock(bytes + offset, state);
    }

    std::array<uint8_t, 128> tail {};
    const size_t remaining = data.getSize() - completeBytes;
    if (remaining > 0) {
        std::memcpy(tail.data(), bytes + completeBytes, remaining);
    }
    tail[remaining] = 0x80u;
    const size_t tailSize = remaining < 56 ? 64 : 128;
    const uint64_t bitCount = (uint64_t) data.getSize() * 8u;
    for (int index = 0; index < 8; ++index) {
        tail[tailSize - 1u - (size_t) index] = (uint8_t) (bitCount >> (index * 8));
    }
    processBlock(tail.data(), state);
    if (tailSize == 128) {
        processBlock(tail.data() + 64, state);
    }

    juce::String result;
    for (uint32_t word : state) {
        result << juce::String::toHexString((juce::int64) word).paddedLeft('0', 8);
    }
    return result;
}

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
            const juce::uint8 displayValue = (juce::uint8) juce::jmin(
                    255,
                    juce::roundToInt(kDisplayGain * (float) value));
            scalarPixels.setPixelColour(
                    x,
                    y,
                    juce::Colour(displayValue, displayValue, displayValue));
        }
    }
    error.clear();
    return std::shared_ptr<const GuideHeatmapAsset>(new GuideHeatmapAsset(
            "sha256:" + sha256(data),
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
