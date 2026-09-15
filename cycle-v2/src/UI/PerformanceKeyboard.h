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

    void setPreviewNoteSelectedCallback(std::function<void(int)> callback) {
        previewNoteSelected = std::move(callback);
    }
    void setPrimaryGestureStartedCallback(std::function<void()> callback) {
        primaryGestureStarted = std::move(callback);
    }
    void shiftOctave(int octaveDelta);
    void releaseAllNotes();
    bool mouseDownOnKey(int midiNoteNumber, const MouseEvent& event) override;
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

    static constexpr int visibleSemitones = 24;

    int rangeStart { 48 };
    int currentHeldNote { -1 };
    float currentVelocity {};

    std::function<void(int)> previewNoteSelected;
    std::function<void()> primaryGestureStarted;

    MidiKeyboardState& keyboardState;
    MidiEventSink& eventSink;
    StateListener stateListener;
};

class PerformanceKeyboardPanel final :
        public Component
    ,   private Timer {
public:
    PerformanceKeyboardPanel(MidiKeyboardState& state, MidiEventSink& sink);

    int baseNote() const { return keyboard.baseNote(); }
    int heldNote() const { return keyboard.heldNote(); }
    float heldVelocity() const { return keyboard.heldVelocity(); }
    String baseNoteLabel() const { return keyboard.noteLabel(keyboard.baseNote()); }
    String highestNoteLabel() const { return keyboard.noteLabel(keyboard.baseNote() + 24); }
    Rectangle<float> noteBounds(int noteNumber) const;
    Rectangle<float> octaveDownBounds() const;
    Rectangle<float> octaveUpBounds() const;
    Rectangle<float> modWheelBounds() const;
    Rectangle<float> playButtonBounds() const;
    Rectangle<float> progressBounds() const;

    int modWheelValue() const { return modWheel.value(); }
    int previewNote() const { return selectedPreviewNote; }
    float playbackProgress() const { return progress; }
    float playbackDurationSeconds() const { return playbackDuration; }
    bool isPlaying() const { return playing; }
    void setPreviewNote(int midiNote);
    void setPreviewNoteSelectedCallback(std::function<void(int)> callback);
    void setModWheelValueChangedCallback(std::function<void(int)> callback);
    void setModWheelValue(int value);
    void setPlaybackDurationSeconds(float seconds);
    bool startPlayback(double nowMilliseconds);
    void togglePlayback();
    void stopPlayback(bool resetProgress = true);
    void updatePlayback(double nowMilliseconds);
    void releaseAllNotes();
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

    class PlayButton final : public Button {
    public:
        explicit PlayButton(const PerformanceKeyboardPanel& owner);

        void paintButton(Graphics& graphics, bool highlighted, bool down) override;

    private:
        const PerformanceKeyboardPanel& owner;
    };

    class ModWheel final :
            public Component
        ,   public SettableTooltipClient {
    public:
        ModWheel();

        int value() const { return currentValue; }
        void setValue(int value, bool sendNotification);

        void focusGained(FocusChangeType cause) override;
        void focusLost(FocusChangeType cause) override;
        bool keyPressed(const KeyPress& key) override;
        void mouseDown(const MouseEvent& event) override;
        void mouseDrag(const MouseEvent& event) override;
        void paint(Graphics& graphics) override;

        std::function<void(int)> onValueChanged;

    private:
        Rectangle<float> wheelTrack() const;
        void updateFromPointer(float y);

        int currentValue {};
    };

    void timerCallback() override;
    void sendModWheelValue();

    bool playing {};
    int selectedPreviewNote { 48 };
    int playbackNote { -1 };
    float playbackDuration { 1.f };
    float progress {};
    double playbackStartedAtMilliseconds {};

    std::function<void(int)> modWheelValueChanged;

    MidiKeyboardState& keyboardState;
    MidiEventSink& eventSink;
    PerformanceKeyboard keyboard;
    OctaveButton octaveDown { false };
    OctaveButton octaveUp { true };
    ModWheel modWheel;
    PlayButton playButton { *this };
};

}
