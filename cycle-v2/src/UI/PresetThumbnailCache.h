#pragma once

#include <JuceHeader.h>

#include <functional>
#include <map>

#include "UI/PresetLibraryIndex.h"

namespace CycleV2 {

class PresetThumbnailCache final {
public:
    using ReadyCallback = std::function<void()>;

    PresetThumbnailCache() = default;
    ~PresetThumbnailCache();

    void setReadyCallback(ReadyCallback callbackToUse);
    juce::Image imageFor(const PresetLibraryRecord& record);
    size_t cachedImageCount() const;
    size_t pendingCount() const;

private:
    class LoadJob;

    struct Entry {
        juce::int64 modificationTime {};
        juce::Image image;
        bool pending {};
    };

    void publish(
            const juce::String& key,
            juce::int64 modificationTime,
            juce::Image image);

    std::map<juce::String, Entry> entries;
    juce::ThreadPool worker { 2 };
    ReadyCallback readyCallback;

    JUCE_DECLARE_WEAK_REFERENCEABLE(PresetThumbnailCache)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetThumbnailCache)
};

}
