#include "Graph/PresetPresentation.h"

namespace CycleV2 {

namespace {

constexpr int kPresentationVersion = 1;
constexpr int kMaximumPreviewWidth = 1024;
constexpr int kMaximumPreviewHeight = 1024;
constexpr size_t kMaximumPreviewBytes = 512 * 1024;

bool isValidDimension(int value, int maximum) {
    return value > 0 && value <= maximum;
}

std::optional<PresetPreviewImage> readPreview(const juce::var& value, juce::String& warning) {
    const auto* object = value.getDynamicObject();
    if (object == nullptr
            || object->getProperty("mediaType").toString() != "image/jpeg") {
        warning = "Preset preview must be a JPEG object";
        return std::nullopt;
    }

    PresetPreviewImage preview;
    preview.width = (int) object->getProperty("width");
    preview.height = (int) object->getProperty("height");
    const auto view = presetPreviewViewForId(object->getProperty("view").toString());
    if (!isValidDimension(preview.width, kMaximumPreviewWidth)
            || !isValidDimension(preview.height, kMaximumPreviewHeight)
            || !view.has_value()) {
        warning = "Preset preview dimensions or view are invalid";
        return std::nullopt;
    }
    preview.view = *view;

    juce::MemoryOutputStream decoded;
    const juce::String data = object->getProperty("data").toString();
    if (data.isEmpty() || !juce::Base64::convertFromBase64(decoded, data)
            || decoded.getDataSize() > kMaximumPreviewBytes) {
        warning = "Preset preview data is invalid or too large";
        return std::nullopt;
    }
    preview.jpegData = decoded.getMemoryBlock();

    const juce::Image image = juce::ImageFileFormat::loadFrom(preview.jpegData.getData(),
            preview.jpegData.getSize());
    if (!image.isValid()
            || image.getWidth() != preview.width
            || image.getHeight() != preview.height) {
        warning = "Preset preview JPEG does not match its declared dimensions";
        return std::nullopt;
    }
    return preview;
}

}

bool PresetPreviewImage::isValid() const {
    return width > 0 && height > 0 && !jpegData.isEmpty();
}

bool PresetPresentation::empty() const {
    return author.isEmpty()
            && pack.isEmpty()
            && description.isEmpty()
            && tags.isEmpty()
            && rating == 0
            && !preview.has_value();
}

juce::var PresetPresentationCodec::writeJSON(const PresetPresentation& presentation) {
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("version", kPresentationVersion);
    if (presentation.author.isNotEmpty()) {
        result->setProperty("author", presentation.author);
    }
    if (presentation.pack.isNotEmpty()) {
        result->setProperty("pack", presentation.pack);
    }
    if (presentation.description.isNotEmpty()) {
        result->setProperty("description", presentation.description);
    }
    if (!presentation.tags.isEmpty()) {
        juce::Array<juce::var> tags;
        for (const auto& tag : presentation.tags) {
            tags.add(tag);
        }
        result->setProperty("tags", std::move(tags));
    }
    if (presentation.rating > 0) {
        result->setProperty("rating", juce::jlimit(0, 5, presentation.rating));
    }
    if (presentation.preview.has_value() && presentation.preview->isValid()) {
        const PresetPreviewImage& preview = *presentation.preview;
        auto encoded = std::make_unique<juce::DynamicObject>();
        encoded->setProperty("mediaType", "image/jpeg");
        encoded->setProperty("width", preview.width);
        encoded->setProperty("height", preview.height);
        encoded->setProperty("view", idForPresetPreviewView(preview.view));
        encoded->setProperty("data", juce::Base64::toBase64(
                preview.jpegData.getData(), preview.jpegData.getSize()));
        result->setProperty("preview", juce::var(encoded.release()));
    }
    return juce::var(result.release());
}

PresetPresentationDecodeResult PresetPresentationCodec::readJSON(const juce::var& value) {
    PresetPresentationDecodeResult result;
    if (value.isVoid()) {
        return result;
    }

    const auto* object = value.getDynamicObject();
    if (object == nullptr || (int) object->getProperty("version") != kPresentationVersion) {
        result.warning = "Preset presentation has an unsupported schema";
        return result;
    }

    result.presentation.author = object->getProperty("author").toString();
    result.presentation.pack = object->getProperty("pack").toString();
    result.presentation.description = object->getProperty("description").toString();
    result.presentation.rating = juce::jlimit(0, 5, (int) object->getProperty("rating"));

    const juce::var tagsValue = object->getProperty("tags");
    if (const auto* tags = tagsValue.getArray()) {
        for (const auto& tag : *tags) {
            if (tag.isString() && tag.toString().isNotEmpty()) {
                result.presentation.tags.addIfNotAlreadyThere(tag.toString());
            }
        }
    } else if (!tagsValue.isVoid()) {
        result.warning = "Preset presentation tags must be an array";
    }

    const juce::var previewValue = object->getProperty("preview");
    if (!previewValue.isVoid()) {
        result.presentation.preview = readPreview(previewValue, result.warning);
    }
    return result;
}

juce::String idForPresetPreviewView(PresetPreviewView view) {
    return view == PresetPreviewView::Time ? "time" : "spectrum";
}

std::optional<PresetPreviewView> presetPreviewViewForId(const juce::String& id) {
    if (id == "time") {
        return PresetPreviewView::Time;
    }
    if (id == "spectrum") {
        return PresetPreviewView::Spectrum;
    }
    return std::nullopt;
}

}
