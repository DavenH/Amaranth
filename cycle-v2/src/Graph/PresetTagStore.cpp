#include "Graph/PresetTagStore.h"

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

juce::StringArray PresetTagStore::normalize(juce::StringArray tags) {
    tags.trim();
    tags.removeEmptyStrings();
    tags.removeDuplicates(true);
    return tags;
}

bool PresetTagStore::save(
        const juce::File& file,
        juce::StringArray tags,
        juce::String& error) {
    tags = normalize(std::move(tags));
    if (tags.isEmpty()) {
        error = "Enter at least one tag.";
        return false;
    }

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

    juce::Array<juce::var> encodedTags;
    for (const auto& tag : tags) {
        encodedTags.add(tag);
    }
    metadata->setProperty("tags", std::move(encodedTags));
    if (PresetPresentationCodec::readMetadataJSON(presentation).presentation.tags
            != tags) {
        error = "The tags could not be encoded.";
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
