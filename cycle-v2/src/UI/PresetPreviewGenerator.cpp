#include "UI/PresetPreviewGenerator.h"

#include "Runtime/DefaultOutputPreview.h"
#include "UI/CanvasChromePalette.h"
#include "UI/NodePreviewRenderer.h"

namespace CycleV2 {

namespace {

Image renderPreview(
        const GraphPreviewResult::SignalProbePreview& preview,
        int width,
        int height) {
    NodePreviewResult renderResult {
            "preset-output-preview",
            PreviewModuleRole::SignalSpy,
            preview.values,
            {},
            preview.gridColumns,
            preview.gridRows,
            preview.domain,
            preview.frequencySampling,
            preview.frequencyMidiNote
    };
    const Image heatmap = NodePreviewRenderer::createRuntimeHeatmapImage(renderResult);
    if (!heatmap.isValid()) {
        return {};
    }

    Image result(Image::RGB, width, height, true);
    Graphics graphics(result);
    graphics.fillAll(CanvasChromePalette::insetBackground);
    graphics.setImageResamplingQuality(Graphics::highResamplingQuality);
    graphics.drawImage(heatmap, result.getBounds().toFloat());

    graphics.setColour(CanvasChromePalette::border.withAlpha(0.24f));
    for (int column = 1; column < 8; ++column) {
        const float x = (float) width * (float) column / 8.f;
        graphics.drawVerticalLine(roundToInt(x), 0.f, (float) height);
    }
    for (int row = 1; row < 4; ++row) {
        const float y = (float) height * (float) row / 4.f;
        graphics.drawHorizontalLine(roundToInt(y), 0.f, (float) width);
    }
    return result;
}

}

GraphPreviewResult::SignalProbePreview PresetPreviewGenerator::forView(
        const GraphPreviewResult::SignalProbePreview& timePreview,
        PresetPreviewView view) {
    if (view == PresetPreviewView::Spectrum) {
        return DefaultOutputPreview::spectrum(timePreview);
    }
    return DefaultOutputPreview::normalizedTime(timePreview);
}

PresetPreviewImage PresetPreviewGenerator::encodeJpeg(
        const GraphPreviewResult::SignalProbePreview& timePreview,
        PresetPreviewView view,
        int width,
        int height) {
    const auto preview = forView(timePreview, view);
    const Image image = renderPreview(preview, width, height);
    if (!image.isValid()) {
        return {};
    }

    MemoryOutputStream encoded;
    JPEGImageFormat format;
    format.setQuality(0.86f);
    if (!format.writeImageToStream(image, encoded)) {
        return {};
    }
    return { encoded.getMemoryBlock(), width, height, view };
}

}
