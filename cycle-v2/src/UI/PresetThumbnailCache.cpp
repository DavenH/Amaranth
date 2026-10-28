#include "UI/PresetThumbnailCache.h"

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

PresetThumbnailCache::PresetThumbnailCache() : cache(loadThumbnail) {}

void PresetThumbnailCache::setReadyCallback(ReadyCallback callback) {
    cache.setReadyCallback(std::move(callback));
}

juce::Image PresetThumbnailCache::imageFor(const PresetLibraryRecord& record) {
    return cache.imageFor(record.file, record.modificationTime);
}

size_t PresetThumbnailCache::cachedImageCount() const { return cache.cachedImageCount(); }
size_t PresetThumbnailCache::pendingCount() const { return cache.pendingCount(); }

}
