#include "Nodes/Guide/GuideHeatmapSampler.h"

#include "Nodes/Guide/GuideHeatmapAsset.h"

namespace CycleV2 {

namespace {

float catmullRom(float p0, float p1, float p2, float p3, float amount) {
    const float first = p2 - p0;
    const float second = 2.f * p0 - 5.f * p1 + 4.f * p2 - p3;
    const float third = 3.f * (p1 - p2) + p3 - p0;
    return p1 + 0.5f * amount * (first + amount * (second + amount * third));
}

}

float GuideHeatmapSampler::sampleBicubic(
        const GuideHeatmapAsset& heatmap,
        float x,
        float y) {
    if (heatmap.width() < 1 || heatmap.height() < 1) {
        return 0.f;
    }
    const float pixelX = jlimit(0.f, 1.f, x) * (float) (heatmap.width() - 1);
    const float pixelY = (1.f - jlimit(0.f, 1.f, y)) * (float) (heatmap.height() - 1);
    const int baseX = (int) pixelX;
    const int baseY = (int) pixelY;
    const float amountX = pixelX - (float) baseX;
    const float amountY = pixelY - (float) baseY;
    float rows[4] {};
    for (int row = 0; row < 4; ++row) {
        const int sourceY = baseY + row - 1;
        rows[row] = catmullRom(
                heatmap.intensityAt(baseX - 1, sourceY),
                heatmap.intensityAt(baseX, sourceY),
                heatmap.intensityAt(baseX + 1, sourceY),
                heatmap.intensityAt(baseX + 2, sourceY),
                amountX);
    }
    return jlimit(0.f, 1.f, catmullRom(
            rows[0], rows[1], rows[2], rows[3], amountY));
}

bool GuideHeatmapSampler::samplePath(
        const GuideHeatmapAsset& heatmap,
        Buffer<float> path,
        Buffer<float> destination) {
    if (path.empty() || destination.size() != path.size()) {
        return false;
    }
    const float denominator = path.size() > 1 ? (float) (path.size() - 1) : 1.f;
    for (int index = 0; index < path.size(); ++index) {
        destination[index] = sampleBicubic(
                heatmap,
                (float) index / denominator,
                path[index]);
    }
    return true;
}

}
