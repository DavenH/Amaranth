#include "Nodes/ImpulseResponse/ImpulseResponseAnalysis.h"

#include "Graph/NodeParameterMap.h"
#include "Nodes/Curve/Panel/FlatCurvePreparation.h"

#include <Algo/FFT.h>
#include <Algo/Oversampler.h>
#include <Array/ScopedAlloc.h>
#include <Audio/CycleDsp/IrModel.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>
#include <Util/Arithmetic.h>

#include <cmath>

namespace CycleV2 {

namespace {

constexpr int maximumSpectrumRows = 512;
constexpr float frequencyTensionScale = 1000.f;
constexpr double minimumImpulseSampleRate = 8000.0;
constexpr double maximumImpulseSampleRate = 384000.0;

void reduceMagnitudes(Buffer<float> source, std::vector<float>& destination) {
    const int destinationSize = jmin(source.size(), maximumSpectrumRows);
    destination.resize((size_t) destinationSize);
    Buffer<float> output(destination.data(), destinationSize);
    if (source.size() <= destinationSize) {
        source.copyTo(output);
        return;
    }

    ScopedAlloc<float> scratchMemory(destinationSize);
    Buffer<float> scratch = scratchMemory.place(destinationSize);
    output.zero();
    const int ratio = source.size() / destinationSize;
    for (int offset = 0; offset < ratio; ++offset) {
        scratch.downsampleFrom(source, -1, offset);
        output.add(scratch);
    }
    output.mul(1.f / (float) ratio);
}

}

bool isSupportedImpulseResponseSampleRate(double sampleRate) {
    return std::isfinite(sampleRate)
            && sampleRate >= minimumImpulseSampleRate
            && sampleRate <= maximumImpulseSampleRate;
}

std::vector<float> resampleImpulseResponse(
        const std::vector<float>& samples,
        double sourceSampleRate,
        double destinationSampleRate,
        int destinationSampleCount) {
    const int maximumOutputSamples = (int) std::ceil(
            CycleDsp::irImpulseLength(1.0) * maximumImpulseSampleRate / impulseResponseReferenceSampleRate);
    if (samples.empty() || destinationSampleCount <= 0 || destinationSampleCount > maximumOutputSamples
            || !isSupportedImpulseResponseSampleRate(sourceSampleRate)
            || !isSupportedImpulseResponseSampleRate(destinationSampleRate)) {
        return {};
    }

    std::vector<float> result((size_t) destinationSampleCount);
    if (sourceSampleRate == destinationSampleRate) {
        VecOps::copy(samples.data(), result.data(),
                jmin((int) samples.size(), destinationSampleCount));
        return result;
    }

    auto* inputChannel = const_cast<float*>(samples.data());
    juce::AudioBuffer<float> inputBuffer(&inputChannel, 1, (int) samples.size());
    juce::MemoryAudioSource input(inputBuffer, false);
    juce::ResamplingAudioSource resampler(&input, false, 1);
    resampler.setResamplingRatio(sourceSampleRate / destinationSampleRate);
    resampler.prepareToPlay(destinationSampleCount, destinationSampleRate);
    auto* outputChannel = result.data();
    juce::AudioBuffer<float> outputBuffer(&outputChannel, 1, destinationSampleCount);
    resampler.getNextAudioBlock({ &outputBuffer, 0, destinationSampleCount });
    resampler.releaseResources();
    return result;
}

std::optional<ImpulseResponseSource> prepareImpulseResponseSource(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const AudioSampleResource* directResource) {
    const NodeParameterMap parameterMap(parameters);
    const int impulseLength = CycleDsp::irImpulseLength(
            parameterMap.floatValue("size", 0.5f));

    ImpulseResponseSource result;
    if (directResource != nullptr) {
        result.rawImpulse = resampleImpulseResponse(directResource->samples.vector(),
                directResource->sampleRate, impulseResponseReferenceSampleRate, impulseLength);
        if (result.rawImpulse.empty()) {
            return {};
        }
        result.displayImpulse = result.rawImpulse;
    } else {
        result.rawImpulse.resize((size_t) impulseLength);
        result.displayImpulse.resize((size_t) impulseLength);
        Buffer<float> rawImpulseBuffer(result.rawImpulse.data(), impulseLength);
        std::vector<float> oversampledImpulse((size_t) impulseLength * 2);
        Oversampler oversampler(8);
        oversampler.setOversampleFactor(2);
        oversampler.setMemoryBuffer({
            oversampledImpulse.data(), (int) oversampledImpulse.size()
        });
        FlatCurvePreparation curve(
                "CycleV2IrConfiguration",
                model,
                FXRasterizer::Bipolar);
        if (!curve.prepare()) {
            return {};
        }
        const auto sampler = curve.sampler();
        const double interval = (1.0 - CycleDsp::irDomainPadding)
                / (double) (impulseLength - 1);
        (void) sampler.sampleWithInterval(
                { result.displayImpulse.data(), impulseLength },
                interval,
                (double) CycleDsp::irDomainPadding);
        CycleDsp::rasterizeIrImpulse(
                sampler,
                rawImpulseBuffer,
                oversampler,
                CycleDsp::irDomainPadding);
    }
    return result;
}

ImpulseResponseAnalysis prepareImpulseResponseAnalysis(
        const ImpulseResponseSource& source,
        float highPass) {
    const int impulseLength = (int) source.rawImpulse.size();
    std::vector<float> prefilterLevels((size_t) impulseLength / 2);
    Buffer<float> rawImpulseBuffer(
            const_cast<float*>(source.rawImpulse.data()), impulseLength);

    ImpulseResponseAnalysis result;
    result.filteredImpulse.resize((size_t) impulseLength);
    result.filteredDisplayImpulse.resize((size_t) impulseLength);
    Transform transform;
    transform.allocate(impulseLength, Transform::DivFwdByN, true);
    Buffer<float> levels(prefilterLevels.data(), (int) prefilterLevels.size());
    CycleDsp::buildIrPrefilterLevels(levels, highPass);
    CycleDsp::applyIrFrequencyPrefilter(
            rawImpulseBuffer,
            { result.filteredImpulse.data(), (int) result.filteredImpulse.size() },
            levels,
            highPass > 0.f,
            transform);
    CycleDsp::applyIrFrequencyPrefilter(
            { const_cast<float*>(source.displayImpulse.data()), impulseLength },
            { result.filteredDisplayImpulse.data(),
                    (int) result.filteredDisplayImpulse.size() },
            levels,
            highPass > 0.f,
            transform);

    Buffer<float> magnitudes = transform.getMagnitudes();
    Arithmetic::applyLogMapping(magnitudes, frequencyTensionScale);
    magnitudes.threshLT(0.f);
    const float maximumMagnitude = magnitudes.max();
    if (maximumMagnitude > 0.f) {
        magnitudes.mul(0.99f / maximumMagnitude);
    }
    reduceMagnitudes(magnitudes, result.normalizedMagnitudes);

    result.frequencyRows.resize(result.normalizedMagnitudes.size());
    Buffer<float> frequencyRows(
            result.frequencyRows.data(), (int) result.frequencyRows.size());
    if (frequencyRows.size() > 1) {
        frequencyRows.ramp(0.f, 1.f / (float) (frequencyRows.size() - 1));
        Arithmetic::applyLogMapping(frequencyRows, frequencyTensionScale);
    }
    return result;
}

std::optional<ImpulseResponseAnalysis> prepareImpulseResponseAnalysis(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const AudioSampleResource* directResource) {
    const auto source = prepareImpulseResponseSource(parameters, model, directResource);
    if (!source.has_value()) {
        return {};
    }
    const NodeParameterMap parameterMap(parameters);
    return prepareImpulseResponseAnalysis(
            *source,
            parameterMap.floatValue("highPass", 0.5f));
}

}
