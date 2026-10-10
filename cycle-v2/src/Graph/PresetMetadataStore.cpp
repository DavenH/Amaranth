#include "Graph/PresetMetadataStore.h"

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

namespace {

template <typename Edit>
bool saveMetadata(const juce::File& file, juce::String& error, Edit edit) {
    juce::var root;
    if (juce::JSON::parse(file.loadFileAsString(), root).failed()) {
        error = "The preset file is not valid JSON.";
        return false;
    }
    auto* object = root.getDynamicObject();
    auto presentation = object == nullptr
            ? juce::var() : object->getProperty("presetPresentation");
    auto* metadata = presentation.getDynamicObject();
    if (metadata == nullptr) {
        error = "The preset has no editable metadata.";
        return false;
    }
    if (!edit(*metadata, presentation, error)) {
        return false;
    }

    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(juce::JSON::toString(root, true))
            || !temporary.overwriteTargetFileWithTemporary()) {
        error = "The preset file could not be saved.";
        return false;
    }
    error.clear();
    return true;
}

}

juce::StringArray PresetMetadataStore::normalize(juce::StringArray tags) {
    tags.trim();
    tags.removeEmptyStrings();
    tags.removeDuplicates(true);
    return tags;
}

bool PresetMetadataStore::save(
        const juce::File& file,
        juce::StringArray tags,
        juce::String& error) {
    tags = normalize(std::move(tags));
    return saveMetadata(file, error,
            [&tags](juce::DynamicObject& metadata,
                    const juce::var& presentation,
                    juce::String& saveError) {
                juce::Array<juce::var> encodedTags;
                for (const auto& tag : tags) {
                    encodedTags.add(tag);
                }
                metadata.setProperty("tags", std::move(encodedTags));
                if (PresetPresentationCodec::readMetadataJSON(presentation)
                        .presentation.tags != tags) {
                    saveError = "The tags could not be encoded.";
                    return false;
                }
                return true;
            });
}

bool PresetMetadataStore::saveTitle(
        const juce::File& file,
        const juce::String& title,
        juce::String& error) {
    const auto normalized = title.trim();
    if (normalized.isEmpty()) {
        error = "Enter a title.";
        return false;
    }
    return saveMetadata(file, error,
            [&normalized](juce::DynamicObject& metadata,
                    const juce::var& presentation,
                    juce::String& saveError) {
                metadata.setProperty("title", normalized);
                if (PresetPresentationCodec::readMetadataJSON(presentation)
                        .presentation.title != normalized) {
                    saveError = "The title could not be encoded.";
                    return false;
                }
                return true;
            });
}

}
