#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

#include <UI/Widgets/AmaranthMidiKeyboard.h>

#include "Runtime/RealtimeMidiEventQueue.h"
#include "Graph/PresetPresentation.h"

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
    void revealNote(int midiNote);
    void revealRange(int lowest, int highest);
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
    void setRangeStart(int noteNumber);

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
    Rectangle<float> progressBounds() const;
    Rectangle<float> editBounds() const { return editButton.getBounds().toFloat(); }
    Rectangle<float> recordBounds() const { return recordButton.getBounds().toFloat(); }
    Rectangle<float> playBounds() const { return playButton.getBounds().toFloat(); }
    size_t sequenceNoteCount() const {
        return sequence.has_value() ? sequence->notes.size() : 0;
    }
    size_t sequenceControlCount() const {
        return sequence.has_value() ? sequence->controls.size() : 0;
    }

    int modWheelValue() const { return modWheel.value(); }
    int previewNote() const { return selectedPreviewNote; }
    float playbackProgress() const { return progress; }
    float playbackDurationSeconds() const { return playbackDuration; }
    bool isPlaying() const { return playing; }
    bool isRecording() const { return recording; }
    void setSequence(std::optional<PresetMidiSequence> sequence);
    void setSequenceChangedCallback(std::function<void(PresetMidiSequence)> callback);
    void showSequenceEditor(PresetMidiSequence sequence,
            std::function<void(PresetMidiSequence)> onEdit);
    void setRecordingChangedCallback(std::function<void(bool)> callback) {
        recordingChanged = std::move(callback);
    }
    void setFlushRecordingInputCallback(std::function<void()> callback) {
        flushRecordingInput = std::move(callback);
    }
    void recordMidiMessage(const MidiMessage& message, double nowSeconds);
    void setPreviewNote(int midiNote);
    void setPreviewNoteSelectedCallback(std::function<void(int)> callback);
    void setModWheelValueChangedCallback(std::function<void(int)> callback);
    void setModWheelGestureStartedCallback(std::function<void()> callback);
    void setModWheelGestureEndedCallback(std::function<void()> callback);
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
        void mouseUp(const MouseEvent& event) override;
        void paint(Graphics& graphics) override;

        std::function<void(int)> onValueChanged;
        std::function<void()> onGestureStarted;
        std::function<void()> onGestureEnded;

    private:
        Rectangle<float> wheelTrack() const;
        void updateFromPointer(float y);

        int currentValue {};
    };

    void timerCallback() override;
    void sendModWheelValue();
    void openSequenceEditor();
    void stopRecording();
    void auditionSequenceNote(int pitch, int velocity, bool noteOn);
    void releaseEditorAudition();
    void dispatchSequenceEvents(double elapsedSeconds);
    void rebuildPlaybackEvents();
    bool hasSequenceNotes() const;

    struct PlaybackEvent {
        double timeSeconds {};
        MidiMessage message;
    };

    bool playing {};
    bool recording {};
    int selectedPreviewNote { 48 };
    int playbackNote { -1 };
    int editorAuditionPitch { -1 };
    float playbackDuration { 1.f };
    float progress {};
    double playbackStartedAtMilliseconds {};
    double recordingStartedAtSeconds {};
    size_t nextPlaybackEvent {};
    size_t nextModulationSegment {};
    int lastSentModulation { -1 };
    std::array<double, 128> recordedNoteStarts {};
    std::array<int, 128> recordedVelocities {};
    std::array<int, 128> activePlaybackNoteCounts {};
    std::optional<PresetMidiSequence> sequence;
    std::vector<PlaybackEvent> playbackEvents;
    std::vector<PresetMidiControl> modulationEnvelope;

    std::function<void(PresetMidiSequence)> sequenceChanged;
    std::function<void(bool)> recordingChanged;
    std::function<void()> flushRecordingInput;

    std::function<void(int)> modWheelValueChanged;
    std::function<void()> modWheelGestureStarted;
    std::function<void()> modWheelGestureEnded;

    MidiKeyboardState& keyboardState;
    MidiEventSink& eventSink;
    PerformanceKeyboard keyboard;
    OctaveButton octaveDown { false };
    OctaveButton octaveUp { true };
    ModWheel modWheel;
    TextButton playButton { "PLAY" };
    TextButton editButton { "EDIT" };
    TextButton recordButton { "REC" };
};

}
