#include <algorithm>
#include <memory>

#include "Graph/PatternLibrary.h"

namespace CycleV2 {

namespace {

bool validId(const juce::String& id) {
    if (id.isEmpty() || id.length() > 100) {
        return false;
    }
    for (auto character : id) {
        if (!juce::CharacterFunctions::isLetterOrDigit(character)
                && character != '-' && character != '_') {
            return false;
        }
    }
    return true;
}

juce::String legacyFactoryId(const juce::String& id) {
    const std::pair<const char*, const char*> aliases[] {
        { "deep-pocket", "bass" }, { "acid-turn", "bass" },
        { "walking-blue", "bass" }, { "dub-space", "bass" },
        { "octave-engine", "bass" }, { "skyward-arc", "lead" },
        { "minor-hook", "lead" }, { "wide-legato", "lead" },
        { "trance-ladder", "lead" }, { "blues-answer", "lead" },
        { "suspended-cloud", "pad" }, { "open-horizon", "pad" },
        { "minor-drift", "pad" }, { "fifth-drone", "pad" },
        { "luminous-rise", "pad" }, { "swing-comp", "keys" },
        { "jazz-run", "keys" }, { "neo-soul-keys", "keys" },
        { "glass-arpeggio", "keys" }, { "offbeat-organ", "keys" },
        { "vibraphone-drops", "keys" }, { "sax-blue-hour", "sustained" },
        { "sax-night-walk", "sustained" }, { "brass-fanfare", "sustained" },
        { "flute-current", "sustained" }, { "strings-dialogue", "sustained" },
        { "hand-drum-dialogue", "rhythm" },
        { "metallic-sparks", "rhythm" }, { "tom-steps", "rhythm" }
    };
    for (const auto& [oldName, tag] : aliases) {
        if (id == "factory-" + juce::String(oldName)) {
            return "factory-basic-" + juce::String(tag);
        }
    }
    return id;
}

std::optional<PatternRecord> readPattern(const juce::File& file, bool factory) {
    const juce::var value = juce::JSON::parse(file);
    const auto* object = value.getDynamicObject();
    if (object == nullptr || (int) object->getProperty("version") != 1) {
        return std::nullopt;
    }
    PatternRecord result;
    result.id = object->getProperty("id").toString();
    result.name = object->getProperty("name").toString();
    result.file = file;
    result.factory = factory;
    result.tag = object->getProperty("tag").toString();
    const auto tagsValue = object->getProperty("tags");
    if (const auto* tags = tagsValue.getArray()) {
        for (const auto& tag : *tags) {
            const auto value = tag.toString().trim();
            if (value.isNotEmpty()) {
                result.tags.addIfNotAlreadyThere(value);
            }
        }
    }
    if (result.tag.isNotEmpty() && !result.tags.contains(result.tag, true)) {
        result.tags.insert(0, result.tag);
    }
    if (!validId(result.id) || result.name.isEmpty()) {
        return std::nullopt;
    }
    const auto sequence = PresetPresentationCodec::readSequenceJSON(
            object->getProperty("sequence"));
    if (!sequence.has_value()) {
        return std::nullopt;
    }
    result.sequence = *sequence;
    return result;
}

juce::var writePattern(const PatternRecord& pattern) {
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("version", 1);
    object->setProperty("id", pattern.id);
    object->setProperty("name", pattern.name);
    object->setProperty("tag", pattern.tag);
    juce::Array<juce::var> tags;
    for (const auto& tag : pattern.tags) {
        tags.add(tag);
    }
    object->setProperty("tags", std::move(tags));
    object->setProperty("sequence",
            PresetPresentationCodec::writeSequenceJSON(pattern.sequence));
    return juce::var(object.release());
}

}

PatternLibrary::PatternLibrary(
        juce::File factoryDirectoryToUse,
        juce::File userDirectoryToUse) :
        factoryDirectory(std::move(factoryDirectoryToUse))
    ,   userDirectory(std::move(userDirectoryToUse)) {
    reload();
}

void PatternLibrary::reload() {
    patterns.clear();
    readDirectory(factoryDirectory, true);
    readDirectory(userDirectory, false);
    std::sort(patterns.begin(), patterns.end(),
            [](const PatternRecord& first, const PatternRecord& second) {
                return first.name.compareIgnoreCase(second.name) < 0;
            });
}

void PatternLibrary::readDirectory(const juce::File& directory, bool factory) {
    for (const auto& file : directory.findChildFiles(
            juce::File::findFiles, false, "*.cyclepattern")) {
        const auto record = readPattern(file, factory);
        if (record.has_value() && find(record->id) == nullptr) {
            patterns.push_back(*record);
        }
    }
}

const PatternRecord* PatternLibrary::find(const juce::String& id) const {
    const auto canonicalId = legacyFactoryId(id);
    const auto found = std::find_if(patterns.begin(), patterns.end(),
            [&canonicalId](const PatternRecord& pattern) {
                return pattern.id == canonicalId;
            });
    return found == patterns.end() ? nullptr : &*found;
}

juce::String PatternLibrary::newUserId() const {
    return "user-" + juce::Uuid().toString().removeCharacters("{}");
}

std::optional<PatternRecord> PatternLibrary::saveUserPattern(
        const juce::String& id,
        const juce::String& name,
        const PresetMidiSequence& sequence,
        const juce::String& tag,
        const juce::StringArray& tags) {
    const auto* existing = find(id);
    if (!validId(id) || name.trim().isEmpty()
            || (existing != nullptr && existing->factory)) {
        return std::nullopt;
    }
    auto tagsToSave = !tags.isEmpty() ? tags
            : existing != nullptr ? existing->tags : juce::StringArray { tag };
    const auto primaryTag = tag.isNotEmpty()
            ? tag : (existing != nullptr ? existing->tag : juce::String());
    if (primaryTag.isNotEmpty() && !tagsToSave.contains(primaryTag, true)) {
        tagsToSave.insert(0, primaryTag);
    }
    PatternRecord record { id, name.trim(), sequence,
            userDirectory.getChildFile(id + ".cyclepattern"), false,
            primaryTag, tagsToSave };
    if (!PresetPresentationCodec::readSequenceJSON(
            PresetPresentationCodec::writeSequenceJSON(sequence)).has_value()
            || userDirectory.createDirectory().failed()) {
        return std::nullopt;
    }
    const juce::String content = juce::JSON::toString(writePattern(record), true);
    juce::TemporaryFile temporary(record.file);
    if (!temporary.getFile().replaceWithText(content)
            || !temporary.overwriteTargetFileWithTemporary()) {
        return std::nullopt;
    }
    reload();
    return *find(id);
}

std::optional<PatternRecord> PatternLibrary::renameUserPattern(
        const juce::String& id,
        const juce::String& name) {
    const auto* pattern = find(id);
    if (pattern == nullptr || pattern->factory) {
        return std::nullopt;
    }
    return saveUserPattern(id, name, pattern->sequence, pattern->tag, pattern->tags);
}

bool PatternLibrary::deleteUserPattern(const juce::String& id) {
    const auto* pattern = find(id);
    if (pattern == nullptr || pattern->factory || !pattern->file.moveToTrash()) {
        return false;
    }
    reload();
    return true;
}

}
