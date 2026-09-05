#include "Nodes/Guide/GuideCurvePreparation.h"

#include "Nodes/Curve/Panel/FlatCurvePreparation.h"
#include "Nodes/Guide/GuideHeatmapAsset.h"
#include "Nodes/Guide/GuideHeatmapSampler.h"

namespace CycleV2 {

namespace {

constexpr float kGuidePadding = 0.05f;

}

bool GuideCurvePreparation::prepare(
        const GuideCurveResource& resource,
        const GuideHeatmapAsset* heatmap,
        Buffer<float> path,
        Buffer<float> bipolarOutput) {
    if (path.empty() || path.size() != bipolarOutput.size()) {
        return false;
    }

    FlatCurvePreparation preparation(
            "CycleV2GuidePreparation",
            resource.model,
            FXRasterizer::Unipolar);
    if (!preparation.prepare()) {
        return false;
    }

    const float interval = path.size() > 1
            ? (1.f - 2.f * kGuidePadding) / (float) (path.size() - 1)
            : 0.f;
    preparation.sampler().sampleWithInterval(path, interval, kGuidePadding);
    if (heatmap != nullptr) {
        if (!GuideHeatmapSampler::samplePath(*heatmap, path, bipolarOutput)) {
            return false;
        }
    } else {
        path.copyTo(bipolarOutput);
    }
    bipolarOutput.add(-0.5f);
    return true;
}

}
