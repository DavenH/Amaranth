#pragma once

#include <JuceHeader.h>

#include <UI/Widgets/AmaranthMidiKeyboard.h>

#include <functional>

#include "../Runtime/RealtimeMidiEventQueue.h"

namespace CycleV2 {

class PerformanceKeyboard final : public AmaranthMidiKeyboard {
public:
    PerformanceKeyboard(MidiKeyboardState& state, MidiEventSink& sink);
    ~PerformanceKeyboard() override;

    int baseNote() const { return rangeStart; }
    int heldNote() const { return currentHeldNote; }
    float heldVelocity() const { return currentVelocity; }
    Rectangle<float> noteBounds(int noteNumber) const;

    void shiftOctave(int octaveDelta);
    void releaseAllNotes();
    void resized() override;

private:
    class StateListener final : public MidiKeyboardState::Listener {
    public:
        explicit StateListener(PerformanceKeyboard& owner) : owner(owner) {}

        void handleNoteOn(
                MidiKeyboardState*,
                int midiChannel,
                int midiNoteNumber,
                float velocity) override;
        void handleNoteOff(
                MidiKeyboardState*,
                int midiChannel,
                int midiNoteNumber,
                float velocity) override;

    private:
        PerformanceKeyboard& owner;
    };

    void handleNoteOn(
            int midiChannel,
            int midiNoteNumber,
            float velocity);
    void handleNoteOff(
            int midiChannel,
            int midiNoteNumber,
            float velocity);

    static constexpr int visibleSemitones = 12;

    MidiKeyboardState& keyboardState;
    MidiEventSink& eventSink;
    StateListener stateListener;
    int rangeStart { 60 };
    int currentHeldNote { -1 };
    float currentVelocity {};
};

class PerformanceKeyboardPanel final : public Component {
public:
    PerformanceKeyboardPanel(MidiKeyboardState& state, MidiEventSink& sink);

    int baseNote() const { return keyboard.baseNote(); }
    int heldNote() const { return keyboard.heldNote(); }
    float heldVelocity() const { return keyboard.heldVelocity(); }
    Rectangle<float> noteBounds(int noteNumber) const;
    Rectangle<float> octaveDownBounds() const;
    Rectangle<float> octaveUpBounds() const;
    Rectangle<float> dragHandleBounds() const;

    void releaseAllNotes() { keyboard.releaseAllNotes(); }
    void setStatus(const String& status);
    void paint(Graphics& graphics) override;
    void resized() override;
    void mouseDown(const MouseEvent& event) override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;

    std::function<void(Point<int>)> onMoved;

private:
    int headerHeight() const;

    PerformanceKeyboard keyboard;
    TextButton octaveDown { "-" };
    TextButton octaveUp { "+" };
    Label audioStatus;
    ComponentDragger dragger;
    bool dragGestureActive {};
};

}
