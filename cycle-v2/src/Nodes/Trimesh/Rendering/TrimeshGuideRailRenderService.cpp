#include "Nodes/Trimesh/Rendering/TrimeshGuideRailRenderService.h"

#include "Nodes/Guide/GuideCurveSnapshotProvider.h"

#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>

namespace CycleV2 {

std::vector<std::vector<float>> TrimeshGuideRailRenderService::prepareTables(
        GuideCurveSnapshotProvider& provider,
        uint32_t visualizationSeed) {
    Rasterization::GuideCurveOffsetSeeds offsetSeeds;
    offsetSeeds.derive(
            provider.size(),
            GuideCurveProvider::tableSize,
            Rasterization::GuideCurveSeed::visualization(visualizationSeed));

    std::vector<std::vector<float>> tables((size_t) provider.size());
    for (int guideIndex = 0; guideIndex < provider.size(); ++guideIndex) {
        auto& table = tables[(size_t) guideIndex];
        table.resize(GuideCurveProvider::tableSize);
        GuideCurveProvider::NoiseContext noise;
        noise.noiseSeed = (int) (visualizationSeed % GuideCurveProvider::tableSize);
        noise.phaseOffset = offsetSeeds.phaseAt(guideIndex);
        noise.vertOffset = offsetSeeds.verticalAt(guideIndex);
        provider.sampleDownAddNoise(
                guideIndex,
                Buffer<float>(table.data(), (int) table.size()),
                noise);
    }
    return tables;
}

}
