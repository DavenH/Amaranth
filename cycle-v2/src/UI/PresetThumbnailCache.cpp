#include "UI/PresetThumbnailCache.h"

#include <algorithm>

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

namespace {

juce::Image loadThumbnail(const juce::File& file) {
    juce::var root;
    if (juce::JSON::parse(file.loadFileAsString(), root).failed()) {
        return {};
    }
    const auto* object = root.getDynamicObject();
    if (object == nullptr) {
        return {};
    }

    const auto decoded = PresetPresentationCodec::readJSON(
            object->getProperty("presetPresentation"));
    if (!decoded.presentation.preview.has_value()) {
        return {};
    }
    const auto& jpeg = decoded.presentation.preview->jpegData;
    return juce::ImageFileFormat::loadFrom(jpeg.getData(), jpeg.getSize());
}

}

class PresetThumbnailCache::LoadJob final : public juce::ThreadPoolJob {
public:
    LoadJob(
            PresetThumbnailCache& ownerToUse,
            juce::File fileToLoad,
            juce::int64 modificationTimeToUse) :
            ThreadPoolJob("Cycle V2 preset thumbnail")
        ,   owner(&ownerToUse)
        ,   file(std::move(fileToLoad))
        ,   key(file.getFullPathName())
        ,   modificationTime(modificationTimeToUse) {
    }

    JobStatus runJob() override {
        juce::Image image = loadThumbnail(file);
        const juce::WeakReference<PresetThumbnailCache> safeOwner = owner;
        juce::MessageManager::callAsync([
                safeOwner,
                keyToPublish = key,
                modificationTimeToPublish = modificationTime,
                imageToPublish = std::move(image)]() mutable {
            if (safeOwner != nullptr) {
                safeOwner->publish(
                        keyToPublish,
                        modificationTimeToPublish,
                        std::move(imageToPublish));
            }
        });
        return jobHasFinished;
    }

private:
    juce::WeakReference<PresetThumbnailCache> owner;
    juce::File file;
    juce::String key;
    juce::int64 modificationTime {};
};

PresetThumbnailCache::~PresetThumbnailCache() {
    worker.removeAllJobs(true, 2000);
}

void PresetThumbnailCache::setReadyCallback(ReadyCallback callbackToUse) {
    readyCallback = std::move(callbackToUse);
}

juce::Image PresetThumbnailCache::imageFor(const PresetLibraryRecord& record) {
    const juce::String key = record.file.getFullPathName();
    auto found = entries.find(key);
    if (found != entries.end()
            && found->second.modificationTime == record.modificationTime) {
        return found->second.image;
    }

    entries.insert_or_assign(key, Entry { record.modificationTime, {}, true });
    worker.addJob(new LoadJob(*this, record.file, record.modificationTime), true);
    return {};
}

size_t PresetThumbnailCache::cachedImageCount() const {
    return (size_t) std::count_if(entries.begin(), entries.end(), [](const auto& pair) {
        return pair.second.image.isValid();
    });
}

size_t PresetThumbnailCache::pendingCount() const {
    return (size_t) std::count_if(entries.begin(), entries.end(), [](const auto& pair) {
        return pair.second.pending;
    });
}

void PresetThumbnailCache::publish(
        const juce::String& key,
        juce::int64 modificationTime,
        juce::Image image) {
    const auto found = entries.find(key);
    if (found == entries.end()
            || found->second.modificationTime != modificationTime) {
        return;
    }
    found->second.image = std::move(image);
    found->second.pending = false;
    if (readyCallback) {
        readyCallback();
    }
}

}
