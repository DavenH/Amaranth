#include "Nodes/Guide/GuideCurveSnapshotProvider.h"

#include <algorithm>
#include <cstdint>

#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Guide/GuideCurvePreparation.h"

namespace CycleV2 {

GuideCurveSnapshotProvider::GuideCurveSnapshotProvider() :
        noise        (tableSize)
    ,   phaseScratch (tableSize) {
    GuideCurveTableDsp::initializeNoise(
            Buffer<float>(noise.data(), (int) noise.size()));
}

bool GuideCurveSnapshotProvider::addGuide(
        const GuideCurveResource& resource,
        const GuideHeatmapAsset* heatmap) {
    GuideSnapshot snapshot;
    snapshot.table.resize(tableSize);
    snapshot.parameters.noiseLevel = resource.noise;
    snapshot.parameters.verticalOffsetLevel = resource.dcOffset;
    snapshot.parameters.phaseOffsetLevel = resource.phase;
    snapshot.parameters.seed = stableSeed(resource);

    const auto typedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(resource.model);
    const FlatCurveModel* curve = typedModel != nullptr ? typedModel->flatCurve() : nullptr;
    snapshot.density = curve != nullptr ? (int) curve->getVertices().size() : 0;

    if (!resource.enabled) {
        Buffer<float>(snapshot.table.data(), (int) snapshot.table.size()).zero();
        guides.push_back(std::move(snapshot));
        return true;
    }

    std::vector<float> path(snapshot.table.size());
    Buffer<float> table(snapshot.table.data(), (int) snapshot.table.size());
    if (!GuideCurvePreparation::prepare(
            resource,
            heatmap,
            Buffer<float>(path.data(), (int) path.size()),
            table)) {
        return false;
    }
    guides.push_back(std::move(snapshot));
    return true;
}

float GuideCurveSnapshotProvider::getTableValue(
        int guideIndex,
        float progress,
        const NoiseContext& context) {
    GuideSnapshot* guide = guideAt(guideIndex);
    if (guide == nullptr) {
        return 0.f;
    }

    return GuideCurveTableDsp::tableValue(
            Buffer<Float32>(guide->table.data(), (int) guide->table.size()),
            Buffer<float>(noise.data(), (int) noise.size()),
            guide->parameters,
            progress,
            context);
}

void GuideCurveSnapshotProvider::sampleDownAddNoise(
        int guideIndex,
        Buffer<float> destination,
        const NoiseContext& context) {
    GuideSnapshot* guide = guideAt(guideIndex);
    if (guide == nullptr) {
        destination.zero();
        return;
    }

    GuideCurveTableDsp::sampleDownAddNoise(
            Buffer<Float32>(guide->table.data(), (int) guide->table.size()),
            Buffer<float>(noise.data(), (int) noise.size()),
            Buffer<float>(phaseScratch.data(), (int) phaseScratch.size()),
            guide->parameters,
            destination,
            context);
}

Buffer<Float32> GuideCurveSnapshotProvider::getTable(int guideIndex) {
    GuideSnapshot* guide = guideAt(guideIndex);
    return guide != nullptr
            ? Buffer<Float32>(guide->table.data(), (int) guide->table.size())
            : Buffer<Float32>();
}

int GuideCurveSnapshotProvider::getTableDensity(int guideIndex) {
    GuideSnapshot* guide = guideAt(guideIndex);
    return guide != nullptr ? guide->density : 0;
}

uint32_t GuideCurveSnapshotProvider::visualizationSeed(PortDomain domain) {
    switch (domain) {
        case PortDomain::SpectralPhaseSignal:     return 0x50484153u;
        case PortDomain::SpectralMagnitudeSignal: return 0x53504543u;
        default:                                  return 0x54494d45u;
    }
}

int GuideCurveSnapshotProvider::stableSeed(const GuideCurveResource& resource) {
    return GuideCurveTableDsp::stableSeed(resource.id.hashCode());
}

GuideCurveSnapshotProvider::GuideSnapshot* GuideCurveSnapshotProvider::guideAt(
        int guideIndex) {
    if (!isPositiveAndBelow(guideIndex, (int) guides.size())) {
        return nullptr;
    }

    return &guides[(size_t) guideIndex];
}

}
