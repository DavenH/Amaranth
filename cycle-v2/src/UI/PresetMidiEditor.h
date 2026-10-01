#pragma once

#include <JuceHeader.h>

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

class PresetMidiEditor final : public juce::Component {
public:
    using ChangeCallback = std::function<void(PresetMidiSequence)>;

    PresetMidiEditor(PresetMidiSequence sequence, ChangeCallback callback,
            std::function<void()> togglePlayback = {});

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
            const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    juce::Rectangle<float> noteGrid() const;
    juce::Rectangle<float> controlGrid() const;
    void paintNoteGrid(juce::Graphics& graphics) const;
    void paintNotes(juce::Graphics& graphics) const;
    void paintModulation(juce::Graphics& graphics) const;
    int pitchAt(float y) const;
    double timeAt(float x) const;
    float xForTime(double time) const;
    double visibleDuration() const;
    int controllerLane() const;
    void editControl(juce::Point<float> point);
    void publish();

    PresetMidiSequence sequence;
    ChangeCallback onChange;
    std::function<void()> onTogglePlayback;
    juce::TextButton clearButton { "Clear" };
    int lowestPitch { 48 };
    int activeNote { -1 };
    int selectedNote { -1 };
    int activeControl { -1 };
    int selectedControl { -1 };
    double timeOffset {};
    juce::Point<float> gestureStart;
    PresetMidiNote originalNote;
    bool resizingNote {};
    bool pendingChange {};
};

}
