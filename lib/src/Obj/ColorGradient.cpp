#include "ColorGradient.h"

using std::vector;

ColorGradient::ColorGradient() : pixelStride(3) {
}

void ColorGradient::read(Image& image, bool softerAlpha, bool isTransparent) {
    jassert(! image.isNull());

    pixelStride = (isTransparent ? 4 : 3);

    pixels.resize(image.getWidth() * pixelStride);
    floatPixels.resize(image.getWidth() * pixelStride);

    bool isWindows = false;

  #ifndef JUCE_WINDOWS
    isWindows = false;
  #endif

    int pos = 0, fpos = 0;

    for (int i = 0; i < image.getWidth(); ++i) {
        Colour colour(image.getPixelAt(i, 0));

        float r = colour.getFloatRed();
        float g = colour.getFloatGreen();
        float b = colour.getFloatBlue();

        float alpha = i / float(image.getWidth());
        alpha = softerAlpha ? squashSoft(alpha) : squash(alpha);

        colours.emplace_back(r, g, b, alpha);

        if (isTransparent || isWindows) {
            floatPixels[fpos++] = b * alpha;
            floatPixels[fpos++] = g * alpha;
            floatPixels[fpos++] = r * alpha;
        } else {
            floatPixels[fpos++] = r * alpha;
            floatPixels[fpos++] = g * alpha;
            floatPixels[fpos++] = b * alpha;
        }

        if (isTransparent) {
            floatPixels[fpos++] = alpha;
        }

        pixels[pos++] = colour.getRed();
        pixels[pos++] = colour.getGreen();
        pixels[pos++] = colour.getBlue();

        if(isTransparent) {
            pixels[pos++] = std::numeric_limits<unsigned char>::max() * alpha;
        }
    }
}

void ColorGradient::multiplyAlpha(float alpha) {
    jassert(pixelStride == 4);

    if(pixelStride != 4) {
        return;
    }

    for (auto& colour : colours)
        colour.v[3] = jlimit<float>(0, 1.f, alpha * colour.v[3]);

    uint8 maxu8 = std::numeric_limits<unsigned char>::max();

    for (int i = 0; i < (int) pixels.size() / 4; ++i) {
        uint8& px = pixels[(i + 1) * pixelStride - 1];
        px = jlimit<uint8>(0, maxu8, (uint8) alpha * px);

        float& fpx = floatPixels[(i + 1) * pixelStride - 1];
        fpx = jlimit<float>(0, 1.f, alpha * fpx);
    }
}
