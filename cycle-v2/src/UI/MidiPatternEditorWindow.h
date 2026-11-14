#pragma once

#include <JuceHeader.h>

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

class PresetMidiEditor;

class MidiPatternEditorWindow final : public juce::DialogWindow {
public:
    using SaveCallback = std::function<void(PresetMidiSequence, bool,
            std::function<void(bool)>)>;

    MidiPatternEditorWindow(std::unique_ptr<PresetMidiEditor> editor,
            PresetMidiSequence initial, SaveCallback save,
            std::function<void()> dismissed);
    void draftChanged(PresetMidiSequence sequence);
    void requestSave(bool closing);
    void closeButtonPressed() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    bool hasChanges() const;
    void dismiss();

    PresetMidiSequence saved;
    PresetMidiSequence draft;
    SaveCallback onSave;
    std::function<void()> onDismiss;
    bool saving {};
};

}
