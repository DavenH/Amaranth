#include "UI/NodeWorkspace.h"
#include "UI/DialogLookAndFeel.h"

namespace CycleV2 {

void NodeWorkspace::configurePatternLibrary(File factoryDirectory, File userDirectory) {
    patternLibrary = std::make_unique<PatternLibrary>(
            std::move(factoryDirectory), std::move(userDirectory));
    keyboard.setSequence(resolvedPresetSequence());
    refreshPatternSidebar();
}

std::optional<PresetMidiSequence> NodeWorkspace::resolvedPresetSequence() const {
    if (patternLibrary != nullptr && canvas.presetPatternId().isNotEmpty()) {
        if (const auto* pattern = patternLibrary->find(canvas.presetPatternId())) {
            return pattern->sequence;
        }
    }
    return canvas.presetSequence();
}

void NodeWorkspace::refreshPatternSidebar() {
    if (patternLibrary != nullptr) {
        canvas.setPatternSidebarRecords(
                patternLibrary->records(), canvas.presetPatternId());
    }
}

void NodeWorkspace::selectPattern(const String& id) {
    if (patternLibrary == nullptr) {
        return;
    }
    const auto* pattern = patternLibrary->find(id);
    if (pattern == nullptr) {
        return;
    }
    if (canvas.setPresetPatternId(id)) {
        keyboard.setSequence(pattern->sequence);
        refreshPatternSidebar();
    }
}

void NodeWorkspace::createPattern(const String& name, const StringArray& tags) {
    if (patternLibrary == nullptr) {
        return;
    }
    const auto sequence = resolvedPresetSequence().value_or(PresetMidiSequence {});
    const auto saved = patternLibrary->saveUserPattern(
            patternLibrary->newUserId(), name, sequence,
            tags.isEmpty() ? String("Other") : tags[0], tags);
    if (saved.has_value()) {
        selectPattern(saved->id);
        editPattern(saved->id);
    } else {
        showPatternSaveError();
    }
}

void NodeWorkspace::editPattern(const String& id) {
    if (patternLibrary == nullptr) {
        return;
    }
    const auto* pattern = patternLibrary->find(id);
    if (pattern == nullptr) {
        return;
    }
    selectPattern(id);
    const Component::SafePointer<NodeWorkspace> safe(this);
    keyboard.showSequenceEditor(pattern->sequence, {},
            [safe, id](PresetMidiSequence sequence, bool closing,
                    std::function<void(bool)> complete) {
                if (safe != nullptr) {
                    safe->savePatternCopy(id, std::move(sequence), closing, std::move(complete));
                } else {
                    complete(false);
                }
            }, [safe] {
                if (safe != nullptr) {
                    safe->keyboard.setSequence(safe->resolvedPresetSequence());
                }
            });
}

void NodeWorkspace::renamePattern(const String& id, const String& name) {
    if (patternLibrary == nullptr) {
        return;
    }
    if (patternLibrary->renameUserPattern(id, name).has_value()) {
        refreshPatternSidebar();
        return;
    }
    AlertWindow::showMessageBoxAsync(
            MessageBoxIconType::WarningIcon,
            "Pattern not renamed",
            "The selected user pattern could not be renamed.",
            "OK");
}

void NodeWorkspace::deletePattern(const String& id) {
    if (patternLibrary == nullptr) {
        return;
    }
    if (patternLibrary->deleteUserPattern(id)) {
        keyboard.setSequence(resolvedPresetSequence());
        refreshPatternSidebar();
        return;
    }
    AlertWindow::showMessageBoxAsync(
            MessageBoxIconType::WarningIcon,
            "Pattern not deleted",
            "The selected user pattern could not be moved to Trash.",
            "OK");
}

void NodeWorkspace::saveEditedSequence(
        PresetMidiSequence sequence, const String& sourceId) {
    if (patternLibrary == nullptr) {
        canvas.setPresetSequence(std::move(sequence));
        return;
    }
    const auto* source = patternLibrary->find(sourceId);
    const bool updateUserPattern = source != nullptr && !source->factory;
    const String id = updateUserPattern
            ? sourceId : patternLibrary->newUserId();
    const String presetName = canvas.graphFile().getFileNameWithoutExtension();
    const String name = source != nullptr
            ? source->name + (updateUserPattern ? "" : " Variation")
            : (presetName.isNotEmpty() ? presetName : "Untitled") + " Pattern";
    const auto saved = patternLibrary->saveUserPattern(id, name, sequence,
            source != nullptr ? source->tag : juce::String(),
            source != nullptr ? source->tags : juce::StringArray());
    if (saved.has_value()) {
        canvas.setPresetPatternId(saved->id);
        refreshPatternSidebar();
    } else {
        keyboard.setSequence(resolvedPresetSequence());
        showPatternSaveError();
    }
}

void NodeWorkspace::showPatternSaveError() {
    AlertWindow::showMessageBoxAsync(
            MessageBoxIconType::WarningIcon,
            "Pattern not saved",
            "The pattern library could not save this phrase.",
            "OK");
}


void NodeWorkspace::savePatternCopy(const String& sourceId,
        PresetMidiSequence sequence, bool closing, std::function<void(bool)> complete) {
    const auto* source = patternLibrary != nullptr ? patternLibrary->find(sourceId) : nullptr;
    if (source == nullptr) {
        showPatternSaveError();
        complete(false);
        return;
    }
    const auto tag = source->tag;
    const auto tags = source->tags;
    auto* prompt = new StyledAlertWindow("Save pattern copy",
            "Name a new pattern. The original pattern will stay unchanged.",
            getTopLevelComponent());
    prompt->setComponentID("PatternCopy.Prompt");
    prompt->addTextEditor("name", source->name + " Variation", "Pattern name");
    prompt->addButton("Save copy", 1, KeyPress(KeyPress::returnKey));
    if (closing) {
        prompt->addButton("Discard", 2);
    }
    prompt->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));
    auto* nameEditor = prompt->getTextEditor("name");
    nameEditor->onTextChange = [prompt, nameEditor] {
        prompt->getButton(0)->setEnabled(nameEditor->getText().trim().isNotEmpty());
    };
    prompt->getButton(0)->setEnabled(nameEditor->getText().trim().isNotEmpty());
    const Component::SafePointer<NodeWorkspace> safe(this);
    prompt->enterModalState(true, ModalCallbackFunction::create(
            [safe, prompt, sequence = std::move(sequence), tag, tags,
                    complete = std::move(complete)](int result) mutable {
                if (result != 1 || safe == nullptr || safe->patternLibrary == nullptr) {
                    complete(result == 2);
                    return;
                }
                const auto saved = safe->patternLibrary->saveUserPattern(
                        safe->patternLibrary->newUserId(),
                        prompt->getTextEditorContents("name").trim(), sequence, tag, tags);
                if (!saved.has_value()) {
                    safe->showPatternSaveError();
                    complete(false);
                    return;
                }
                safe->selectPattern(saved->id);
                complete(true);
            }), true);
}

}
