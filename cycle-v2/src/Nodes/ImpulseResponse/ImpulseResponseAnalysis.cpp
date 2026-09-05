#include "Nodes/ImpulseResponse/ImpulseResponseAnalysis.h"

#include "Graph/NodeParameterMap.h"
#include "Nodes/Curve/Panel/FlatCurvePreparation.h"

#include <Algo/FFT.h>
#include <Algo/Oversampler.h>
#include <Array/ScopedAlloc.h>
#include <Audio/CycleDsp/IrModel.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>
#include <Util/Arithmetic.h>

namespace CycleV2 {

namespace {

constexpr int maximumSpectrumRows = 512;
constexpr float frequencyTensionScale = 1000.f;

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

std::optional<ImpulseResponseSource> prepareImpulseResponseSource(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const AudioSampleResource* directResource) {
    const NodeParameterMap parameterMap(parameters);
    const int impulseLength = CycleDsp::irImpulseLength(
            parameterMap.floatValue("size", 0.5f));

    ImpulseResponseSource result;
    result.rawImpulse.resize((size_t) impulseLength);
    result.displayImpulse.resize((size_t) impulseLength);
    Buffer<float> rawImpulseBuffer(result.rawImpulse.data(), impulseLength);
    if (directResource != nullptr) {
        const int copyCount = jmin(impulseLength, (int) directResource->samples.size());
        VecOps::copy(directResource->samples.data(), rawImpulseBuffer.get(), copyCount);
        result.displayImpulse = result.rawImpulse;
    } else {
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
