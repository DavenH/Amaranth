#include "GuideCurveSnapshotProvider.h"

#include <algorithm>
#include <cstdint>

#include "../../Graph/NodeParameterMap.h"
#include "../Effect2D/CurveNodeModels.h"
#include "../Effect2D/FlatCurvePreparation.h"

namespace CycleV2 {

namespace {

constexpr float kGuidePadding = 0.05f;

}

GuideCurveSnapshotProvider::GuideCurveSnapshotProvider() :
        noise        (tableSize)
    ,   phaseScratch (tableSize) {
    GuideCurveTableDsp::initializeNoise(
            Buffer<float>(noise.data(), (int) noise.size()));
}

bool GuideCurveSnapshotProvider::addGuide(const Node& node) {
    if (node.kind != NodeKind::GuideCurve) {
        return false;
    }

    GuideSnapshot snapshot;
    snapshot.table.resize(tableSize);
    const NodeParameterMap parameters(node);
    snapshot.parameters.noiseLevel = parameters.floatValue("noise", 0.f);
    snapshot.parameters.verticalOffsetLevel = parameters.floatValue("dcOffset", 0.f);
    snapshot.parameters.phaseOffsetLevel = parameters.floatValue("phase", 0.f);
    snapshot.parameters.seed = stableSeed((int) guides.size());

    const auto typedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    const FlatCurveModel* curve = typedModel != nullptr ? typedModel->flatCurve() : nullptr;
    snapshot.density = curve != nullptr ? (int) curve->getVertices().size() : 0;

    if (!parameters.boolValue("enabled", true)) {
        Buffer<float>(snapshot.table.data(), (int) snapshot.table.size()).zero();
        guides.push_back(std::move(snapshot));
        return true;
    }

    FlatCurvePreparation preparation(
            "CycleV2GuideSnapshot",
            NodeKind::GuideCurve,
            node.parameters,
            node.model,
            FXRasterizer::Unipolar);
    if (!preparation.prepare()) {
        return false;
    }

    const float interval = (1.f - 2.f * kGuidePadding) / (float) (tableSize - 1);
    preparation.sampler().sampleWithInterval(
            Buffer<float>(snapshot.table.data(), (int) snapshot.table.size()),
            interval,
            kGuidePadding);
    Buffer<float>(snapshot.table.data(), (int) snapshot.table.size()).add(-0.5f);
    guides.push_back(std::move(snapshot));
    return true;
}

bool GuideCurveSnapshotProvider::addGuide(const GuideCurveResource& resource) {
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

    const std::vector<NodeParameter> parameters {
        { "enabled", "Enabled", resource.enabled ? "1" : "0" },
        { "noise", "Noise", String(resource.noise) },
        { "dcOffset", "DC Offset", String(resource.dcOffset) },
        { "phase", "Phase", String(resource.phase) }
    };
    FlatCurvePreparation preparation(
            "CycleV2GuideSnapshot",
            NodeKind::GuideCurve,
            parameters,
            resource.model,
            FXRasterizer::Unipolar);
    if (!preparation.prepare()) {
        return false;
    }

    const float interval = (1.f - 2.f * kGuidePadding) / (float) (tableSize - 1);
    preparation.sampler().sampleWithInterval(
            Buffer<float>(snapshot.table.data(), (int) snapshot.table.size()),
            interval,
            kGuidePadding);
    Buffer<float>(snapshot.table.data(), (int) snapshot.table.size()).add(-0.5f);
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

int GuideCurveSnapshotProvider::stableSeed(int guideIndex) {
    return GuideCurveTableDsp::stableSeed(guideIndex);
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
