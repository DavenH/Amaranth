#include "UI/ReferenceWaveformView.h"

#include <atomic>
#include <cmath>
#include <Algo/PeakCounter.h>
#include <Audio/PitchedSample.h>

#include "UI/CanvasChromePalette.h"
#include "UI/ReferenceWaveformPreview.h"

namespace CycleV2 {

struct ReferenceWaveformView::Source {
    juce::AudioBuffer<float> mono;
    std::vector<int> zeroCrossings;
    WaveformPalette palette { WaveformPalette::Monochrome };
    bool silent {};
    // Only this view's single worker accesses the cache; diagnostics reads the atomic count.
    mutable juce::Image heatField;
    mutable std::atomic<int> heatBuilds {};
};

class ReferenceWaveformView::RenderJob final : public juce::ThreadPoolJob {
public:
    RenderJob(ReferenceWaveformView& view, int width, int height) :
            ThreadPoolJob("Reference waveform view")
        ,   owner(&view)
        ,   source(view.source)
        ,   viewport(view.viewport)
        ,   revision(view.revision)
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
        const auto resolution = referenceWaveformResolution(source->zeroCrossings, start, end, width);
        const auto crossings = resolution.crossings;
        const bool line = resolution.useLine && !source->silent;
        const int sourceEnd = line ? juce::jmin(length, end + 1) : end;
        Buffer<float> audio(const_cast<float*>(source->mono.getReadPointer(0)) + start, sourceEnd - start);
        const juce::Range<float> sampleRange(viewport.x * length - start,
                (viewport.x + viewport.w) * length - start);
        juce::Image image;
        if (source->palette == WaveformPalette::CrossingHeat) {
            if (!source->heatField.isValid()) {
                source->heatField = signalWaveformHeatField(Buffer<float>(
                        const_cast<float*>(source->mono.getReadPointer(0)), length));
                ++source->heatBuilds;
            }
            image = renderSignalWaveform(audio, width, height, viewport, source->heatField, line, sampleRange);
        } else {
            image = line ? referenceWaveformLine(audio, width, height, viewport.y, viewport.h, sampleRange)
                    : referenceWaveformHeatmap(audio, width, height, viewport.y, viewport.h);
        }
        const double renderMs = juce::Time::getMillisecondCounterHiRes() - started;
        if (!shouldExit()) {
            juce::MessageManager::callAsync([owner = owner, revision = revision,
                    image = std::move(image), viewport = viewport, line, crossings, renderMs]() mutable {
                if (owner != nullptr) {
                    owner->publish(revision, std::move(image), viewport, line, crossings, renderMs);
                }
            });
        }
        return jobHasFinished;
    }

private:
    juce::Component::SafePointer<ReferenceWaveformView> owner;
    std::shared_ptr<const Source> source;
    ZoomRect viewport;
    uint64_t revision {};
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
    image = {};
    source.reset();
    if (sample != nullptr) {
        auto prepared = std::make_shared<Source>();
        prepared->mono = referenceWaveformPreview(sample->audio);
        prepared->zeroCrossings = PeakCounter::findZeroCrossings(Buffer<float>(prepared->mono, 0), 0);
        source = std::move(prepared);
        ++sourceBuildCount;
    }
    requestImage();
}

void ReferenceWaveformView::setSignal(std::shared_ptr<const std::vector<float>> samples) {
    if (signalIdentity == samples && sampleIdentity == nullptr) {
        return;
    }
    signalIdentity = std::move(samples);
    setTitle("Signal waveform");
    sampleIdentity = nullptr;
    image = {};
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

void ReferenceWaveformView::publish(uint64_t completed, juce::Image rendered,
        ZoomRect view, bool useLine, size_t count, double elapsedMs) {
    if (completed != revision) {
        return;
    }
    image = std::move(rendered);
    imageViewport = view;
    line = useLine;
    crossings = count;
    renderMs = elapsedMs;
    publishedRevision = completed;
    repaint();
}

void ReferenceWaveformView::paint(juce::Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::surface);
    const float scale = graphics.getInternalContext().getPhysicalPixelScaleFactor();
    if (physicalScale != scale) {
        physicalScale = scale;
        requestImage();
    }
    if (image.isValid() && viewport.w > 0.f && viewport.h > 0.f) {
        graphics.drawImage(image, {
                (imageViewport.x - viewport.x) / viewport.w * getWidth(),
                (imageViewport.y - viewport.y) / viewport.h * getHeight(),
                imageViewport.w / viewport.w * getWidth(),
                imageViewport.h / viewport.h * getHeight() }, juce::RectanglePlacement::stretchToFit);
    }
    if (onOverlay) {
        onOverlay(graphics);
    }
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawRect(getLocalBounds());
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
    value->setProperty("mode", line ? "line" : "density");
    value->setProperty("crossings", (juce::int64) crossings);
    value->setProperty("renderMs", renderMs);
    value->setProperty("pixelWidth", image.getWidth());
    value->setProperty("sourceBuilds", sourceBuildCount);
    value->setProperty("heatBuilds", source != nullptr ? source->heatBuilds.load() : 0);
    value->setProperty("imageBuilds", buildCount);
    value->setProperty("x", viewport.x);
    value->setProperty("w", viewport.w);
    value->setProperty("y", viewport.y);
    value->setProperty("h", viewport.h);
    return { value.release() };
}

}
