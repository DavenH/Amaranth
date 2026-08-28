#pragma once

#include <JuceHeader.h>

#include <UI/Widgets/AmaranthMidiKeyboard.h>

#include "Runtime/RealtimeMidiEventQueue.h"

namespace CycleV2 {

class PerformanceKeyboard final : public AmaranthMidiKeyboard {
public:
    PerformanceKeyboard(MidiKeyboardState& state, MidiEventSink& sink);
    ~PerformanceKeyboard() override;

    int baseNote() const { return rangeStart; }
    int heldNote() const { return currentHeldNote; }
    float heldVelocity() const { return currentVelocity; }
    Rectangle<float> noteBounds(int noteNumber) const;
    String noteLabel(int noteNumber) const;

    void shiftOctave(int octaveDelta);
    void releaseAllNotes();
    void resized() override;

protected:
    void drawWhiteNote(
            int midiNoteNumber,
            Graphics& graphics,
            Rectangle<float> area,
            bool isDown,
            bool isOver,
            Colour lineColour,
            Colour textColour) override;

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
    String baseNoteLabel() const { return keyboard.noteLabel(keyboard.baseNote()); }
    String highestNoteLabel() const { return keyboard.noteLabel(keyboard.baseNote() + 12); }
    Rectangle<float> noteBounds(int noteNumber) const;
    Rectangle<float> octaveDownBounds() const;
    Rectangle<float> octaveUpBounds() const;

    void releaseAllNotes() { keyboard.releaseAllNotes(); }
    void paint(Graphics& graphics) override;
    void resized() override;

private:
    class OctaveButton final : public Button {
    public:
        explicit OctaveButton(bool advancesOctave);

        void paintButton(Graphics& graphics, bool highlighted, bool down) override;

    private:
        bool advances;
    };

    PerformanceKeyboard keyboard;
    OctaveButton octaveDown { false };
    OctaveButton octaveUp { true };
};

}
