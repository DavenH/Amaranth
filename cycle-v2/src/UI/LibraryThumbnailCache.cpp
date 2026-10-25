#include "UI/LibraryThumbnailCache.h"

#include <algorithm>

namespace CycleV2 {

class LibraryThumbnailCache::LoadJob final : public juce::ThreadPoolJob {
public:
    LoadJob(LibraryThumbnailCache& cache, juce::File source, juce::int64 revision) :
            ThreadPoolJob("Library thumbnail")
        ,   owner(&cache)
        ,   file(std::move(source))
        ,   modificationTime(revision)
        ,   loader(cache.loader) {}

    JobStatus runJob() override {
        if (shouldExit()) {
            return jobHasFinished;
        }
        auto image = loader(file);
        if (!shouldExit()) {
            juce::MessageManager::callAsync([safeOwner = owner,
                    key = file.getFullPathName(), revision = modificationTime,
                    image = std::move(image)]() mutable {
                if (safeOwner != nullptr) {
                    safeOwner->publish(key, revision, std::move(image));
                }
            });
        }
        return jobHasFinished;
    }

private:
    juce::WeakReference<LibraryThumbnailCache> owner;
    juce::File file;
    juce::int64 modificationTime {};
    Loader loader;
};

LibraryThumbnailCache::LibraryThumbnailCache(Loader imageLoader, size_t limit) :
        loader(std::move(imageLoader))
    ,   maximumEntries(limit) {}

LibraryThumbnailCache::~LibraryThumbnailCache() {
    worker.removeAllJobs(true, 2000);
}

void LibraryThumbnailCache::setReadyCallback(std::function<void()> callback) {
    readyCallback = std::move(callback);
}

juce::Image LibraryThumbnailCache::imageFor(const juce::File& file, juce::int64 modificationTime) {
    const auto key = file.getFullPathName();
    const auto found = entries.find(key);
    if (found != entries.end() && found->second.modificationTime == modificationTime) {
        found->second.lastUse = ++uses;
        return found->second.image;
    }
    entries.insert_or_assign(key, Entry { modificationTime, {}, true, ++uses });
    ++loads;
    worker.addJob(new LoadJob(*this, file, modificationTime), true);
    trim();
    return {};
}

juce::Image LibraryThumbnailCache::cachedImageFor(const juce::File& file, juce::int64 modificationTime) {
    const auto found = entries.find(file.getFullPathName());
    if (found == entries.end() || found->second.modificationTime != modificationTime) {
        return {};
    }
    found->second.lastUse = ++uses;
    return found->second.image;
}

size_t LibraryThumbnailCache::cachedImageCount() const {
    return (size_t) std::count_if(entries.begin(), entries.end(), [](const auto& pair) {
        return pair.second.image.isValid();
    });
}

size_t LibraryThumbnailCache::pendingCount() const {
    return (size_t) std::count_if(entries.begin(), entries.end(), [](const auto& pair) {
        return pair.second.pending;
    });
}

void LibraryThumbnailCache::publish(const juce::String& key, juce::int64 modificationTime, juce::Image image) {
    const auto found = entries.find(key);
    if (found == entries.end() || found->second.modificationTime != modificationTime) {
        return;
    }
    found->second.image = std::move(image);
    found->second.pending = false;
    trim();
    if (readyCallback) {
        readyCallback();
    }
}

void LibraryThumbnailCache::trim() {
    while (maximumEntries > 0 && entries.size() > maximumEntries) {
        auto oldest = entries.end();
        for (auto entry = entries.begin(); entry != entries.end(); ++entry) {
            if (!entry->second.pending
                    && (oldest == entries.end() || entry->second.lastUse < oldest->second.lastUse)) {
                oldest = entry;
            }
        }
        if (oldest == entries.end()) {
            return;
        }
        entries.erase(oldest);
    }
}

}
