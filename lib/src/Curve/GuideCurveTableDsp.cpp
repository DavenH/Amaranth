#include "GuideCurveTableDsp.h"

namespace {

constexpr int tableModulo = GuideCurveProvider::tableSize - 1;

int phaseOffsetFor(
        const GuideCurveTableParameters& parameters,
        const GuideCurveProvider::NoiseContext& context) {
    return (context.phaseOffset & (tableModulo - GuideCurveProvider::tableSize / 2))
            * parameters.phaseOffsetLevel;
}

void rotatePhase(
        Buffer<float> destination,
        Buffer<float> phaseScratch,
        const GuideCurveTableParameters& parameters,
        const GuideCurveProvider::NoiseContext& context) {
    if (parameters.phaseOffsetLevel <= 0.f || destination.empty()) {
        return;
    }

    const int phaseOffset = phaseOffsetFor(parameters, context);
    destination.withPhase(
            phaseOffset % destination.size(),
            phaseScratch.withSize(destination.size()));
}

void addNoise(
        Buffer<float> destination,
        Buffer<float> noise,
        const GuideCurveTableParameters& parameters,
        const GuideCurveProvider::NoiseContext& context) {
    if (parameters.noiseLevel <= 0.f) {
        return;
    }

    const int noiseOffset = (parameters.seed + context.noiseSeed) & tableModulo;
    const int firstLength = jmin(destination.size(), noise.size() - noiseOffset);
    destination.withSize(firstLength).addProduct(
            noise.section(noiseOffset, firstLength),
            parameters.noiseLevel);

    if (destination.size() > firstLength) {
        destination.offset(firstLength).addProduct(
                noise.withSize(destination.size() - firstLength),
                parameters.noiseLevel);
    }
}

void addVerticalOffset(
        Buffer<float> destination,
        Buffer<float> noise,
        const GuideCurveTableParameters& parameters,
        const GuideCurveProvider::NoiseContext& context) {
    if (parameters.verticalOffsetLevel <= 0.f) {
        return;
    }

    const int offset = (parameters.seed + context.vertOffset) & tableModulo;
    destination.add(parameters.verticalOffsetLevel * noise[offset]);
}

}

void GuideCurveTableDsp::initializeNoise(Buffer<float> noise) {
    uint32_t seed = 0x47554944u;
    noise.rand(seed).sub(0.5f);
}

int GuideCurveTableDsp::stableSeed(int guideIndex) {
    const uint32_t mixed = ((uint32_t) guideIndex + 1u) * 0x9e3779b9u;
    return (int) (mixed % (uint32_t) GuideCurveProvider::tableSize);
}

float GuideCurveTableDsp::tableValue(
        Buffer<Float32> table,
        Buffer<float> noise,
        const GuideCurveTableParameters& parameters,
        float progress,
        const GuideCurveProvider::NoiseContext& context) {
    const int tableIndex = (int) (progress * (float) tableModulo);
    const int sampleIndex = (tableIndex + phaseOffsetFor(parameters, context)) & tableModulo;
    const int noiseIndex = (context.noiseSeed + parameters.seed) & tableModulo;

    return table[sampleIndex]
            + parameters.noiseLevel * noise[noiseIndex]
            + parameters.verticalOffsetLevel * noise[context.vertOffset];
}

void GuideCurveTableDsp::sampleDownAddNoise(
        Buffer<Float32> table,
        Buffer<float> noise,
        Buffer<float> phaseScratch,
        const GuideCurveTableParameters& parameters,
        Buffer<float> destination,
        const GuideCurveProvider::NoiseContext& context) {
    destination.downsampleFrom(table);
    rotatePhase(destination, phaseScratch, parameters, context);
    addNoise(destination, noise, parameters, context);
    addVerticalOffset(destination, noise, parameters, context);
}
