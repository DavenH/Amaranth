#include "Nodes/Envelope/Rendering/EnvelopeSurfacePanel.h"
#include "Nodes/Envelope/Rendering/EnvelopeSectionColours.h"

#include <UI/Panels/Texture.h>

namespace CycleV2 {

namespace {

const auto& sectionPalettes() {
    static const auto palettes = [] {
        std::array<std::array<Colour, 512>, 4> result;
        const auto material = ScalarSurfaceMaterial::blueDepth();
        const auto bipolar = ScalarSurfaceMaterial::signedAmplitudeFlat();
        const auto sections = envelopeSectionColours();
        const std::array<float, 3> hues { sections[0].toColour().getHue(),
                sections[1].toColour().getHue(), sections[2].toColour().getHue() };
        for (int index = 0; index < 512; ++index) {
            const auto blue = ScalarSurfaceMaterialEvaluator::baseColourFor((float) index / 511.f, material);
            for (size_t section = 0; section < hues.size(); ++section) {
                result[section][(size_t) index] = blue.withHue(hues[section]);
            }
            result[3][(size_t) index] = ScalarSurfaceMaterialEvaluator::baseColourFor(
                    (float) index / 511.f, bipolar);
        }
        return result;
    }();
    return palettes;
}

}

EnvelopeSurfacePanel::EnvelopeSurfacePanel(SingletonRepo* repo, Panel3D::DataRetriever& source) :
        SingletonAccessor(repo, "CycleV2EnvelopeSurfacePanel")
    ,   TrimeshPanel3D(repo, source) {}

Image EnvelopeSurfacePanel::createColourMap(
        const std::vector<float>& values, int columns, int rows, Range<float> timeRange,
        const std::vector<Rasterization::EnvelopeSurfaceMarkers>& markers, bool bipolar) {
    if (columns < 2 || rows < 2 || values.size() != (size_t) columns * (size_t) rows
            || markers.size() != (size_t) rows) {
        return {};
    }
    const auto& palettes = sectionPalettes();
    Image image(Image::ARGB, columns, rows, false);
    Image::BitmapData pixels(image, Image::BitmapData::writeOnly);
    const float timeStep = timeRange.getLength() / (float) (columns - 1);
    for (int column = 0; column < columns; ++column) {
        const float time = timeRange.getStart() + (float) column * timeStep;
        for (int row = 0; row < rows; ++row) {
            const auto& section = markers[(size_t) row];
            if (time < section.signalStart || time > section.signalEnd) {
                pixels.setPixelColour(column, rows - 1 - row, Colours::transparentBlack);
                continue;
            }
            const size_t palette = bipolar ? 3 : time >= section.releaseStart ? 2
                    : section.loopStart >= 0.f && time >= section.loopStart ? 1 : 0;
            const float value = values[(size_t) column * rows + row];
            const auto colour = palettes[palette][(size_t) roundToInt(jlimit(0.f, 1.f, value) * 511.f)];
            pixels.setPixelColour(column, rows - 1 - row, colour);
        }
    }
    return image;
}

void EnvelopeSurfacePanel::setSurfaceImage(Image image, Range<float> timeRange) {
    // Prepare at double resolution so the existing texture sampler interpolates
    // the flat colour map, as it does other high-resolution panel imagery.
    contentImage = image.rescaled(image.getWidth() * 2, image.getHeight() * 2,
            Graphics::highResamplingQuality);
    // TextureGL consumes literal RGBA bytes rather than JUCE's native ARGB
    // layout. Translate once at the texture boundary, after JUCE resampling.
    Image::BitmapData pixels(contentImage, Image::BitmapData::readWrite);
    for (int row = 0; row < pixels.height; ++row) {
        for (int column = 0; column < pixels.width; ++column) {
            const auto colour = pixels.getPixelColour(column, row);
            auto* destination = pixels.getPixelPointer(column, row);
            destination[0] = colour.getRed();
            destination[1] = colour.getGreen();
            destination[2] = colour.getBlue();
            destination[3] = colour.getAlpha();
        }
    }
    surfaceTimeRange = timeRange;
}

void EnvelopeSurfacePanel::drawCurvesAndSurfaces() {
    auto* renderer = getPanelRenderer();
    if (renderer == nullptr || contentImageTex == nullptr || !contentImage.isValid()) {
        return;
    }
    if (contentImageTex->image != contentImage) {
        contentImageTex->setImage(contentImage, 2.f);
        renderer->updateTexture(contentImageTex);
        ++uploads;
    }
    contentImageTex->imageScale = 2.f;
    contentImageTex->rect = Rectangle<float>::leftTopRightBottom(
            sx(surfaceTimeRange.getStart()), sy(1.f), sx(surfaceTimeRange.getEnd()), sy(0.f));
    renderer->setCurrentColour(1.f, 1.f, 1.f, 1.f);
    renderer->drawTexture(contentImageTex);
}

}
