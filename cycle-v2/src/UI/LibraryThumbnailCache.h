#pragma once

#include <JuceHeader.h>

#include <functional>
#include <map>

namespace CycleV2 {

class LibraryThumbnailCache final {
public:
    using Loader = std::function<juce::Image(const juce::File&)>;

    explicit LibraryThumbnailCache(Loader loader, size_t maximumEntries = 0);
    ~LibraryThumbnailCache();
    void setReadyCallback(std::function<void()> callback);
    juce::Image imageFor(const juce::File& file, juce::int64 modificationTime);
    juce::Image cachedImageFor(const juce::File& file, juce::int64 modificationTime);
    size_t cachedImageCount() const;
    size_t pendingCount() const;
    uint64_t loadCount() const { return loads; }

private:
    class LoadJob;
    struct Entry {
        juce::int64 modificationTime {};
        juce::Image image;
        bool pending {};
        uint64_t lastUse {};
    };

    void publish(const juce::String& key, juce::int64 modificationTime, juce::Image image);
    void trim();

    Loader loader;
    size_t maximumEntries {};
    std::map<juce::String, Entry> entries;
    juce::ThreadPool worker { 2 };
    std::function<void()> readyCallback;
    uint64_t uses {};
    uint64_t loads {};

    JUCE_DECLARE_WEAK_REFERENCEABLE(LibraryThumbnailCache)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryThumbnailCache)
};

}
