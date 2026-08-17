#pragma once

#include <Curve/GuideCurveTableDsp.h>

#include <cstdint>
#include <vector>

#include "../../Graph/NodeGraph.h"

namespace CycleV2 {

class GuideCurveSnapshotProvider final : public GuideCurveProvider {
public:
    GuideCurveSnapshotProvider();

    bool addGuide(const Node& node);
    bool addGuide(const GuideCurveResource& resource);

    float getTableValue(
            int guideIndex,
            float progress,
            const NoiseContext& context) override;
    void sampleDownAddNoise(
            int guideIndex,
            Buffer<float> destination,
            const NoiseContext& context) override;
    Buffer<Float32> getTable(int guideIndex) override;
    int getTableDensity(int guideIndex) override;

    int size() const { return (int) guides.size(); }
    static uint32_t visualizationSeed(PortDomain domain);

private:
    struct GuideSnapshot {
        std::vector<float> table;
        GuideCurveTableParameters parameters;
        int density {};
    };

    static int stableSeed(int guideIndex);
    static int stableSeed(const GuideCurveResource& resource);
    GuideSnapshot* guideAt(int guideIndex);

    std::vector<float> noise;
    std::vector<float> phaseScratch;
    std::vector<GuideSnapshot> guides;
};

}
