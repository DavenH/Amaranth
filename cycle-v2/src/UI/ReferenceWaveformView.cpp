#include "UI/ReferenceWaveformView.h"

#include <atomic>
#include <cmath>
#include <Algo/PeakCounter.h>
#include <Audio/PitchedSample.h>

#include "UI/CanvasChromePalette.h"
#include "UI/ReferenceWaveformPreview.h"
#include "UI/WaveformHeatRenderer.h"

namespace CycleV2 {

struct ReferenceWaveformView::Source {
    juce::AudioBuffer<float> mono;
    std::vector<int> zeroCrossings;
    WaveformPalette palette { WaveformPalette::Monochrome };
    bool silent {};
    // Only this view's single worker accesses the cache; diagnostics reads the atomic count.
    mutable std::vector<float> density;
    mutable juce::Image heatField;
    mutable WaveformColourStyles::Style heatStyle { WaveformColourStyles::defaultStyle };
    mutable std::atomic<int> colourBuilds {};
    mutable std::atomic<int> geometryBuilds {};
    mutable std::atomic<int> heatBuilds {};

    const juce::Image& coloursFor(WaveformColourStyles::Style style) const {
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

};

class ReferenceWaveformView::RenderJob final : public juce::ThreadPoolJob {
public:
    RenderJob(ReferenceWaveformView& view, int width, int height) :
            ThreadPoolJob("Reference waveform view")
        ,   owner(&view)
        ,   source(view.source)
        ,   cachedGeometry(view.imageSource == view.source
                    && view.imageViewport == view.viewport && view.imageScale == view.physicalScale
                    && view.image.getWidth() == width
                    && view.image.getHeight() == height ? view.image : juce::Image())
        ,   prepareSoftware(view.gpuHeatPaintCount == 0)
        ,   colourStyle(view.colourStyle)
        ,   viewport(view.viewport)
        ,   revision(view.revision)
        ,   physicalScale(view.physicalScale)
        ,   width(width)
        ,   height(height) {}

    JobStatus runJob() override {
        if (shouldExit() || source == nullptr || source->mono.getNumSamples() == 0) {
            return jobHasFinished;
        }
        const double started = juce::Time::getMillisecondCounterHiRes();
        const int length = source->mono.getNumSamples();
        const int start = juce::jlimit(0, length - 1, (int) (viewport.x * length));
        const int end = juce::jlimit(start + 1, length,
                (int) std::ceil((viewport.x + viewport.w) * length));
        auto resolution = referenceWaveformResolution(source->zeroCrossings, start, end, width, physicalScale);
        resolution.useLine = resolution.useLine && !source->silent;
        resolution.useFill = resolution.useFill && resolution.useLine;
        const bool line = resolution.useLine;
        const ReferenceWaveformLineStyle style {
                resolution.useFill ? 1.5f * physicalScale : 1.f, resolution.useFill ? 0.2f : 0.f,
                resolution.useFill && source->palette == WaveformPalette::CrossingHeat };
        const int sourceEnd = line ? juce::jmin(length, end + 1) : end;
        Buffer<float> audio(const_cast<float*>(source->mono.getReadPointer(0)) + start, sourceEnd - start);
        const juce::Range<float> sampleRange(viewport.x * length - start,
                (viewport.x + viewport.w) * length - start);
        juce::Image image;
        juce::Image heatField;
        if (source->palette == WaveformPalette::CrossingHeat) {
            heatField = source->coloursFor(colourStyle);
            if (cachedGeometry.isValid()) {
                image = cachedGeometry;
            } else {
                image = signalWaveformCoverage(audio, width, height, viewport, line, sampleRange, style);
                ++source->geometryBuilds;
            }
        } else {
            image = line ? referenceWaveformLine(audio, width, height, viewport.y, viewport.h, sampleRange, style)
                    : referenceWaveformHeatmap(audio, width, height, viewport.y, viewport.h);
        }
        const auto software = prepareSoftware && heatField.isValid()
                ? recolourSignalWaveform(image, heatField, viewport, resolution.useFill) : juce::Image();
        const double renderMs = juce::Time::getMillisecondCounterHiRes() - started;
        if (!shouldExit()) {
            juce::MessageManager::callAsync([owner = owner, revision = revision,
                    image = std::move(image), heatField = std::move(heatField), software,
                    viewport = viewport, resolution, renderMs]() mutable {
                if (owner != nullptr) {
                    owner->publish(revision, std::move(image), std::move(heatField), software, viewport, resolution, renderMs);
                }
            });
        }
        return jobHasFinished;
    }

private:
    juce::Component::SafePointer<ReferenceWaveformView> owner;
    std::shared_ptr<const Source> source;
    juce::Image cachedGeometry;
    bool prepareSoftware {};
    WaveformColourStyles::Style colourStyle;
    ZoomRect viewport;
    uint64_t revision {};
    float physicalScale { 1.f };
    int width {};
    int height {};
};

ReferenceWaveformView::ReferenceWaveformView() :
        ComponentMovementWatcher(this) {
    setOpaque(true);
    setWantsKeyboardFocus(true);
    setTitle("Reference waveform");
}

ReferenceWaveformView::~ReferenceWaveformView() {
    cancelPendingUpdate();
    worker.removeAllJobs(true, 2000);
}

void ReferenceWaveformView::setSample(const PitchedSample* sample) {
    if (sampleIdentity == sample && signalIdentity == nullptr) {
        return;
    }
    sampleIdentity = sample;
    setTitle("Reference waveform");
    signalIdentity.reset();
    source.reset();
    if (sample != nullptr) {
        auto prepared = std::make_shared<Source>();
        prepared->mono = referenceWaveformPreview(sample->audio);
        prepared->zeroCrossings = PeakCounter::findZeroCrossings(Buffer<float>(prepared->mono, 0), 0);
        source = std::move(prepared);
        ++sourceBuildCount;
    }
    if (source == nullptr) {
        clearPresentedImage();
    }
    requestImage();
}

void ReferenceWaveformView::setSignal(std::shared_ptr<const std::vector<float>> samples) {
    if (signalIdentity == samples && sampleIdentity == nullptr) {
        return;
    }
    colourStyle = WaveformColourStyles::current();
    signalIdentity = std::move(samples);
    setTitle("Signal waveform");
    sampleIdentity = nullptr;
    source.reset();
    if (signalIdentity != nullptr && !signalIdentity->empty()) {
        auto prepared = std::make_shared<Source>();
        prepared->mono.setSize(1, (int) signalIdentity->size());
        Buffer<float> mono(prepared->mono, 0);
        Buffer<float>(const_cast<float*>(signalIdentity->data()), mono.size()).copyTo(mono);
        const float peak = juce::jmax(mono.max(), -mono.min());
        if (peak > 0.f) {
            mono.mul(1.f / peak);
        }
        prepared->zeroCrossings = PeakCounter::findZeroCrossings(mono, 0);
        prepared->silent = peak == 0.f;
        prepared->palette = WaveformPalette::CrossingHeat;
        source = std::move(prepared);
        ++sourceBuildCount;
    }
    if (source == nullptr) {
        clearPresentedImage();
    }
    requestImage();
    repaint();
}

void ReferenceWaveformView::setViewport(const ZoomRect& next) {
    if (viewport == next) {
        return;
    }
    viewport = next;
    requestImage();
    repaint();
}

void ReferenceWaveformView::clearPresentedImage() {
    image = {};
    heatField = {};
    softwareImage = {};
    softwareCoverage = {};
    softwareHeat = {};
    imageSource.reset();
}

void ReferenceWaveformView::requestImage() {
    ++revision;
    if (isShowing() && source != nullptr && getWidth() > 0 && getHeight() > 1) {
        triggerAsyncUpdate();
    }
}

void ReferenceWaveformView::handleAsyncUpdate() {
    worker.removeAllJobs(true, 0);
    if (!isShowing() || source == nullptr || getWidth() < 1 || getHeight() < 2) {
        return;
    }
    ++buildCount;
    worker.addJob(new RenderJob(*this, juce::roundToInt(getWidth() * physicalScale),
            juce::roundToInt(getHeight() * physicalScale)), true);
}

void ReferenceWaveformView::publish(uint64_t completed, juce::Image rendered, juce::Image heat, juce::Image software,
        ZoomRect view, ReferenceWaveformResolution resolution, double elapsedMs) {
    if (completed != revision) {
        return;
    }
    imageSource = source;
    ++publicationCount;
    image = std::move(rendered);
    heatField = std::move(heat);
    softwareImage = std::move(software);
    softwareCoverage = softwareImage.isValid() ? image : juce::Image();
    softwareHeat = softwareImage.isValid() ? heatField : juce::Image();
    imageViewport = view;
    imageScale = physicalScale;
    line = resolution.useLine;
    filled = resolution.useFill;
    crossings = resolution.crossings;
    renderMs = elapsedMs;
    publishedRevision = completed;
    repaint();
}

void ReferenceWaveformView::paint(juce::Graphics& graphics) {
    const double started = juce::Time::getMillisecondCounterHiRes();
    if (source != nullptr && !image.isValid()) {
        ++blankPaintCount;
    } else if (image.isValid() && publishedRevision != revision) {
        ++retainedImagePaintCount;
    }
    if (juce::OpenGLContext::getCurrentContext() != nullptr) {
        ++gpuPaintCount;
    }
    graphics.fillAll(juce::Colours::black);
    if (onBackground) {
        onBackground(graphics);
    }
    if (signalIdentity != nullptr && colourStyle != WaveformColourStyles::current()) {
        colourStyle = WaveformColourStyles::current();
        requestImage();
    }
    const float scale = graphics.getInternalContext().getPhysicalPixelScaleFactor();
    // Software snapshots must not change an on-screen GL view's render resolution.
    if (physicalScale != scale && (juce::OpenGLContext::getCurrentContext() != nullptr || gpuPaintCount == 0)) {
        physicalScale = scale;
        requestImage();
    }
    bool paintedOnGpu {};
    if (auto* context = juce::OpenGLContext::getCurrentContext()) {
        if (auto* target = context->getTargetComponent()) {
            const auto deviceBounds = target->getLocalArea(this, getLocalBounds()).toFloat()
                    * (float) context->getRenderingScale();
            paintedOnGpu = paintWaveformHeatOpenGL(graphics, image, heatField, imageViewport,
                    viewport, getLocalBounds().toFloat(), deviceBounds, filled);
        }
    }
    if (paintedOnGpu) {
        ++gpuHeatPaintCount;
    } else {
        if (image.isValid() && heatField.isValid()
                && (softwareCoverage != image || softwareHeat != heatField)) {
            softwareImage = recolourSignalWaveform(image, heatField, imageViewport, filled);
            softwareCoverage = image;
            softwareHeat = heatField;
        }
        paintWaveformImage(graphics, heatField.isValid() ? softwareImage : image,
                imageViewport, viewport, getLocalBounds().toFloat());
    }
    if (onOverlay) {
        onOverlay(graphics);
    }
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawRect(getLocalBounds());
    paintMs = juce::Time::getMillisecondCounterHiRes() - started;
    if (paintedOnGpu) {
        gpuHeatPaintTotalMs += paintMs;
        gpuHeatPaintMaxMs = juce::jmax(gpuHeatPaintMaxMs, paintMs);
    }
}

void ReferenceWaveformView::resized() { requestImage(); }
void ReferenceWaveformView::componentVisibilityChanged() {
    if (isShowing()) {
        if (!image.isValid() || publishedRevision != revision) {
            triggerAsyncUpdate();
        }
    } else {
        cancelPendingUpdate();
        worker.removeAllJobs(true, 0);
    }
}

void ReferenceWaveformView::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    if (onWheel) {
        onWheel(event, wheel);
    }
}

juce::var ReferenceWaveformView::diagnostics() const {
    auto value = std::make_unique<juce::DynamicObject>();
    value->setProperty("ready", image.isValid() && publishedRevision == revision);
    value->setProperty("hasImage", image.isValid());
    value->setProperty("publications", publicationCount);
    value->setProperty("blankPaints", blankPaintCount);
    value->setProperty("retainedImagePaints", retainedImagePaintCount);
    value->setProperty("mode", line ? "line" : "density");
    value->setProperty("filled", filled);
    value->setProperty("crossings", (juce::int64) crossings);
    value->setProperty("renderMs", renderMs);
    value->setProperty("paintMs", paintMs);
    value->setProperty("gpuPaintCount", gpuPaintCount);
    value->setProperty("gpuHeatPaintCount", gpuHeatPaintCount);
    value->setProperty("gpuHeatPaintTotalMs", gpuHeatPaintTotalMs);
    value->setProperty("gpuHeatPaintMaxMs", gpuHeatPaintMaxMs);
    value->setProperty("background", "black");
    value->setProperty("pixelWidth", image.getWidth());
    value->setProperty("sourceBuilds", sourceBuildCount);
    value->setProperty("heatBuilds", source != nullptr ? source->heatBuilds.load() : 0);
    value->setProperty("colourBuilds", source != nullptr ? source->colourBuilds.load() : 0);
    value->setProperty("geometryBuilds", source != nullptr ? source->geometryBuilds.load() : 0);
    value->setProperty("colourStyle", WaveformColourStyles::id(colourStyle));
    value->setProperty("imageBuilds", buildCount);
    value->setProperty("x", viewport.x);
    value->setProperty("w", viewport.w);
    value->setProperty("y", viewport.y);
    value->setProperty("h", viewport.h);
    return { value.release() };
}

}
