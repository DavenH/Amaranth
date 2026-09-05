#include "Nodes/Guide/GuideHeatmapLoader.h"

namespace CycleV2 {

namespace {

class LoadJob final : public juce::ThreadPoolJob {
public:
    LoadJob(juce::File sourceFile, GuideHeatmapLoader::Completion completion) :
            juce::ThreadPoolJob("Load Guide heatmap")
        ,   file(std::move(sourceFile))
        ,   complete(std::move(completion)) {}

    JobStatus runJob() override {
        GuideHeatmapAssetPtr asset;
        juce::String error;
        const juce::int64 size = file.getSize();
        if (size <= 0 || size > (juce::int64) GuideHeatmapAsset::maximumEncodedBytes) {
            error = "Image must be between 1 byte and 16 MiB";
        } else {
            juce::MemoryBlock data;
            if (!file.loadFileAsData(data)) {
                error = "The image could not be read";
            } else {
                asset = GuideHeatmapAsset::decode(data, file.getFileName(), error);
            }
        }

        juce::MessageManager::callAsync([
                completion = std::move(complete),
                asset = std::move(asset),
                error = std::move(error)]() mutable {
            completion(std::move(asset), std::move(error));
        });
        return jobHasFinished;
    }

private:
    juce::File file;
    GuideHeatmapLoader::Completion complete;
};

}

GuideHeatmapLoader::GuideHeatmapLoader() = default;

GuideHeatmapLoader::~GuideHeatmapLoader() {
    pool.removeAllJobs(true, 2000);
}

void GuideHeatmapLoader::load(juce::File file, Completion completion) {
    pool.addJob(new LoadJob(std::move(file), std::move(completion)), true);
}

}
