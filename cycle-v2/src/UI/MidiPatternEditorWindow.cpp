#include <algorithm>

#include "UI/MidiPatternEditorWindow.h"
#include "UI/PresetMidiEditor.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

MidiPatternEditorWindow::MidiPatternEditorWindow(
        std::unique_ptr<PresetMidiEditor> editor,
        PresetMidiSequence initial, SaveCallback save,
        std::function<void()> dismissed) :
        DialogWindow("MIDI Pattern", CanvasChromePalette::dockSurface, false)
    ,   saved(initial)
    ,   draft(std::move(initial))
    ,   onSave(std::move(save))
    ,   onDismiss(std::move(dismissed)) {
    setComponentID("PresetMidiEditor.Dialog");
    setUsingNativeTitleBar(true);
    setResizable(true, false);
    if (onSave) {
        editor->setSaveRequest([this] { requestSave(false); });
    }
    editor->onCloseRequest = [this] { closeButtonPressed(); };
    setContentOwned(editor.release(), true);
}

void MidiPatternEditorWindow::draftChanged(PresetMidiSequence sequence) {
    draft = std::move(sequence);
}

bool MidiPatternEditorWindow::hasChanges() const {
    return saved.durationSeconds != draft.durationSeconds
            || saved.tempoBpm != draft.tempoBpm
            || saved.notes.size() != draft.notes.size()
            || saved.controls.size() != draft.controls.size()
            || !std::equal(saved.notes.begin(), saved.notes.end(), draft.notes.begin(),
                    [](const auto& a, const auto& b) {
                        return a.pitch == b.pitch && a.velocity == b.velocity
                                && a.startSeconds == b.startSeconds
                                && a.durationSeconds == b.durationSeconds;
                    })
            || !std::equal(saved.controls.begin(), saved.controls.end(), draft.controls.begin(),
                    [](const auto& a, const auto& b) {
                        return a.controller == b.controller && a.value == b.value
                                && a.timeSeconds == b.timeSeconds;
                    });
}

void MidiPatternEditorWindow::requestSave(bool closing) {
    if (saving) {
        return;
    }
    if (!onSave || !hasChanges()) {
        if (closing) {
            dismiss();
        }
        return;
    }
    saving = true;
    const juce::Component::SafePointer<MidiPatternEditorWindow> safe(this);
    onSave(draft, closing, [safe, sequence = draft, closing](bool accepted) {
        if (safe == nullptr) {
            return;
        }
        safe->saving = false;
        if (accepted) {
            safe->saved = sequence;
            if (closing) {
                safe->dismiss();
            }
        }
    });
}

void MidiPatternEditorWindow::dismiss() {
    if (onDismiss) {
        onDismiss();
    }
    exitModalState(0);
    setVisible(false);
}

void MidiPatternEditorWindow::closeButtonPressed() {
    requestSave(true);
}

bool MidiPatternEditorWindow::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() == 'S'
            && (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())) {
        requestSave(false);
        return true;
    }
    if (key == juce::KeyPress::escapeKey) {
        closeButtonPressed();
        return true;
    }
    return false;
}

}
