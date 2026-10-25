#pragma once

#include <JuceHeader.h>

#include <functional>

#include "UI/PresetLibraryIndex.h"
#include "UI/LibraryThumbnailCache.h"

namespace CycleV2 {

class PresetThumbnailCache final {
public:
    using ReadyCallback = std::function<void()>;

    PresetThumbnailCache();

    void setReadyCallback(ReadyCallback callbackToUse);
    juce::Image imageFor(const PresetLibraryRecord& record);
    size_t cachedImageCount() const;
    size_t pendingCount() const;

private:
    LibraryThumbnailCache cache;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetThumbnailCache)
};

}
