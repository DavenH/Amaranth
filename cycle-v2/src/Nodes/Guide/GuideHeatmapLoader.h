#pragma once

#include <JuceHeader.h>

#include <functional>

#include "Nodes/Guide/GuideHeatmapAsset.h"

namespace CycleV2 {

class GuideHeatmapLoader {
public:
    using Completion = std::function<void(GuideHeatmapAssetPtr, juce::String)>;

    GuideHeatmapLoader();
    ~GuideHeatmapLoader();

    void load(juce::File file, Completion completion);

private:
    juce::ThreadPool pool { 1 };
};

}
