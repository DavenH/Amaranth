#include "PreviewProcessorFactories.h"

#include "../Nodes/Effects/EffectSignalProcessors.h"

#include <Algo/ConvReverb.h>
#include <Algo/FFT.h>
#include <Audio/CycleDsp/EqualizerCore.h>
#include <Util/NumberUtils.h>

#include <cmath>

namespace CycleV2 {

namespace {

class ReverbSpectrogramPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::ReverbSpectrogram; }

    void render(PreviewProcessContext& context) override {
        const auto configuration = context.configuration != nullptr
                ? std::dynamic_pointer_cast<const ReverbConfiguration>(
                        context.configuration->value)
                : nullptr;
        if (configuration == nullptr || context.pointCount == 0
                || configuration->kernels[0].empty()
                || configuration->kernels[1].empty()) {
            context.primary.clear();
            context.secondary.clear();
            return;
        }

        std::vector<float> dirac(1, 1.f);
        std::vector<float> left(configuration->kernels[0].size());
        std::vector<float> right(configuration->kernels[1].size());
        ConvReverb::basicConvolve(
                { dirac.data(), (int) dirac.size() },
                { const_cast<float*>(configuration->kernels[0].data()),
                        (int) configuration->kernels[0].size() },
                { left.data(), (int) left.size() });
        ConvReverb::basicConvolve(
                { dirac.data(), (int) dirac.size() },
                { const_cast<float*>(configuration->kernels[1].data()),
                        (int) configuration->kernels[1].size() },
                { right.data(), (int) right.size() });

        Buffer<float> response(left.data(), (int) left.size());
        response.add({ right.data(), (int) right.size() });
        const float direct = jmax(0.5f, configuration->width);
        const float cross = jmin(0.5f, 1.f - configuration->width);
        response.mul(configuration->enabled ? 0.5f * (direct + cross) : 0.f);

        constexpr int fftSize = 2048;
        constexpr size_t spectralRowCount = fftSize / 2 + 1;
        constexpr unsigned minimumKernelLog2 = 12;
        const unsigned kernelLog2 = NumberUtils::log2i(
                (unsigned) configuration->kernels[0].size());
        const size_t sizeDetail = 8 * (size_t) (kernelLog2 > minimumKernelLog2
                ? kernelLog2 - minimumKernelLog2
                : 0);
        const size_t columnCount = std::max<size_t>(16, context.pointCount + sizeDetail);
        const size_t rowCount = spectralRowCount;
        context.primary.assign(columnCount * rowCount, 0.f);
        context.secondary.clear();
        context.gridColumns = columnCount;
        context.gridRows = rowCount;
        context.domain = PortDomain::SpectralMagnitudeSignal;

        std::vector<float> windowStorage(fftSize);
        Buffer<float> window(windowStorage.data(), fftSize);
        window.ramp(0.f, MathConstants<float>::pi / (float) (fftSize - 1)).sin().sqr();
        std::vector<float> frameStorage(fftSize);
        Buffer<float> frame(frameStorage.data(), fftSize);
        Transform fft;
        fft.allocate(fftSize, Transform::DivFwdByN, true);
        const size_t maximumStart = response.size() > fftSize
                ? (size_t) response.size() - fftSize
                : 0;

        for (size_t column = 0; column < columnCount; ++column) {
            const size_t start = columnCount > 1
                    ? column * maximumStart / (columnCount - 1)
                    : 0;
            frame.zero();
            const size_t available = std::min<size_t>(fftSize, (size_t) response.size() - start);
            response.section((int) start, (int) available).copyTo(frame.withSize((int) available));
            frame.mul(window);
            fft.forward(frame);
            const Buffer<float> magnitudes = fft.getMagnitudes();
            for (size_t row = 0; row < rowCount; ++row) {
                const size_t bin = rowCount > 1
                        ? row * (size_t) (magnitudes.size() - 1) / (rowCount - 1)
                        : 0;
                context.primary[column * rowCount + row] = magnitudes[(int) bin];
            }
        }

        Buffer<float> surface(context.primary.data(), (int) context.primary.size());
        float maximum {};
        int maximumIndex {};
        surface.getMax(maximum, maximumIndex);
        if (maximum > 0.f) {
            constexpr float maximumWetLevel = 0.25f;
            constexpr float visualizationGain = 36.f;
            surface.mul(visualizationGain * configuration->wetLevel
                            / (maximumWetLevel * maximum))
                    .clip(0.f, 1.f);
        }
    }
};

class EqualizerPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::EqualizerResponse; }

    void render(PreviewProcessContext& context) override {
        const auto configuration = context.configuration != nullptr
                ? std::dynamic_pointer_cast<const EqualizerConfiguration>(
                        context.configuration->value)
                : nullptr;
        if (configuration == nullptr || context.pointCount == 0) {
            context.primary.clear();
            context.secondary.clear();
            return;
        }

        CycleDsp::EqualizerCore core(1);
        for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
            core.configureBand(
                    band,
                    44100.0,
                    configuration->frequencies[(size_t) band],
                    configuration->gains[(size_t) band]);
        }

        context.primary.resize(context.pointCount);
        context.secondary.assign(context.pointCount, 0.5f);
        const double denominator = context.pointCount > 1
                ? (double) context.pointCount - 1.0
                : 1.0;
        double frequency = 40.0;
        const double frequencyRatio = std::pow(400.0, 1.0 / denominator);
        for (size_t index = 0; index < context.pointCount; ++index) {
            context.primary[index] = jlimit(
                    0.f,
                    1.f,
                    core.responseDecibels(frequency) / 60.f + 0.5f);
            frequency *= frequencyRatio;
        }
        context.domain = PortDomain::TimeSignal;
    }
};

void ensurePreview(PreviewProcessContext& context) {
    context.primary.resize(context.pointCount);
    context.secondary.resize(context.pointCount);
}

class EnvelopePreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::Envelope; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            const bool attack = i < context.pointCount / 4;
            const bool sustain = i < context.pointCount / 2;
            context.primary[i] = attack ? 1.f : (sustain ? 0.8f : 0.3f);
            context.secondary[i] = sustain ? 0.4f : 0.f;
        }
    }
};

class ImpulsePreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::ImpulseResponse; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = i == 0 ? 1.f : 1.f / (float) (i + 1);
            context.secondary[i] = 0.f;
        }
    }
};

class TransferPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::Waveshaper; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);
        if (context.pointCount == 0) {
            return;
        }

        const float denominator = context.pointCount > 1
                ? (float) (context.pointCount - 1)
                : 1.f;

        for (size_t i = 0; i < context.pointCount; ++i) {
            const float x = (float) i / denominator;
            context.primary[i] = x * x;
            context.secondary[i] = x;
        }
    }
};

}

std::unique_ptr<NodePreviewProcessor> createEnvelopePreviewProcessor() {
    return std::make_unique<EnvelopePreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createImpulseResponsePreviewProcessor() {
    return std::make_unique<ImpulsePreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createWaveshaperPreviewProcessor() {
    return std::make_unique<TransferPreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createEqualizerPreviewProcessor() {
    return std::make_unique<EqualizerPreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createReverbSpectrogramPreviewProcessor() {
    return std::make_unique<ReverbSpectrogramPreviewProcessor>();
}

}
