#include "UI/PresetMetadataEditor.h"
#include "UI/DialogLookAndFeel.h"

#include "Graph/PresetMetadataStore.h"

namespace CycleV2 {

void PresetMetadataEditor::editTags(
        juce::Component& owner,
        const juce::File& file,
        const juce::StringArray& currentTags,
        TagsSaved onSaved) {
    auto* prompt = new StyledAlertWindow(
            "Edit preset tags", "Separate tags with commas.",
            owner.getTopLevelComponent());
    prompt->addTextEditor("tags", currentTags.joinIntoString(", "), "Tags");
    prompt->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    prompt->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    prompt->enterModalState(true,
            juce::ModalCallbackFunction::create([
                    prompt, file, onSaved = std::move(onSaved)](int result) {
                if (result != 1) {
                    return;
                }
                juce::StringArray tags;
                tags.addTokens(prompt->getTextEditorContents("tags"), ",", "\"");
                tags = PresetMetadataStore::normalize(std::move(tags));
                juce::String error;
                if (!PresetMetadataStore::save(file, tags, error)) {
                    juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Unable to save tags", error);
                    return;
                }
                if (onSaved) {
                    onSaved(tags);
                }
            }), true);
}

void PresetMetadataEditor::rename(
        juce::Component& owner,
        const juce::File& file,
        const juce::String& currentTitle,
        TitleSaved onSaved) {
    auto* prompt = new StyledAlertWindow(
            "Rename preset", "Choose a display title for this preset.",
            owner.getTopLevelComponent());
    prompt->addTextEditor("title", currentTitle, "Preset title");
    prompt->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    prompt->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    prompt->enterModalState(true,
            juce::ModalCallbackFunction::create([
                    prompt, file, onSaved = std::move(onSaved)](int result) {
                if (result != 1) {
                    return;
                }
                const auto title = prompt->getTextEditorContents("title").trim();
                juce::String error;
                if (!PresetMetadataStore::saveTitle(file, title, error)) {
                    juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Unable to rename preset", error);
                    return;
                }
                if (onSaved) {
                    onSaved(title);
                }
            }), true);
}

}
