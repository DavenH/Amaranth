#include "App/LibraryFavorites.h"

namespace CycleV2 {

namespace {

constexpr const char* propertyKey = "libraryFavorites";

}

LibraryFavorites::LibraryFavorites(
        juce::PropertiesFile& settings,
        juce::File factoryDirectory) :
        properties(settings)
    ,   factoryPresetDirectory(std::move(factoryDirectory)) {
    const juce::var saved = juce::JSON::parse(properties.getValue(propertyKey));
    if (const auto* array = saved.getArray()) {
        for (const auto& item : *array) {
            if (item.isString()) {
                keys.addIfNotAlreadyThere(item.toString());
            }
        }
    }
}

bool LibraryFavorites::isPresetFavorite(const juce::File& file) const {
    return contains(presetKey(file));
}

bool LibraryFavorites::isPatternFavorite(const juce::String& id) const {
    return contains("pattern:" + id);
}

bool LibraryFavorites::togglePreset(const juce::File& file) {
    return toggle(presetKey(file));
}

bool LibraryFavorites::togglePattern(const juce::String& id) {
    return toggle("pattern:" + id);
}

juce::String LibraryFavorites::presetKey(const juce::File& file) const {
    if (file.isAChildOf(factoryPresetDirectory)) {
        return "factory-preset:" + file.getRelativePathFrom(factoryPresetDirectory)
                .replaceCharacter('\\', '/').toLowerCase();
    }
    return "user-preset:" + file.getFullPathName();
}

bool LibraryFavorites::contains(const juce::String& key) const {
    return keys.contains(key);
}

bool LibraryFavorites::toggle(const juce::String& key) {
    const int index = keys.indexOf(key);
    if (index >= 0) {
        keys.remove(index);
    } else {
        keys.add(key);
    }
    juce::Array<juce::var> saved;
    for (const auto& value : keys) {
        saved.add(value);
    }
    properties.setValue(propertyKey, juce::JSON::toString(juce::var(saved), false));
    properties.saveIfNeeded();
    return index < 0;
}

}
