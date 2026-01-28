#pragma once
#include <JuceHeader.h>
#include <array>
#include "OscAudioProcessor.h"
#include <Algo/FFT.h>

#include "GradientColorMap.h"
#include "RealTimePitchTracker.h"
#include "TempermentControls.h"

class HighlightKeyboard : public MidiKeyboardComponent {
public:
    using MidiKeyboardComponent::MidiKeyboardComponent;

    void setHighlightedNote(int note) {
        highlightedNote = note;
        repaint();
    }

    std::function<void(int)> onArrowKey;

    bool keyPressed(const KeyPress& key) override {
        if (key.getKeyCode() == KeyPress::rightKey) {
            if (onArrowKey) {
                onArrowKey(1);
            }
            return true;
        }
        if (key.getKeyCode() == KeyPress::leftKey) {
            if (onArrowKey) {
                onArrowKey(-1);
            }
            return true;
        }
        return false;
    }

    void drawWhiteNote(int midiNoteNumber, Graphics& g, Rectangle<float> area,
                       bool isDown, bool isOver, Colour lineColour, Colour textColour) override {
        MidiKeyboardComponent::drawWhiteNote(midiNoteNumber, g, area, isDown, isOver, lineColour, textColour);
        if (midiNoteNumber == highlightedNote) {
            g.setColour(Colours::yellow.withAlpha(0.6f));
            g.fillRect(area);
        }
    }

    void drawBlackNote(int midiNoteNumber, Graphics& g, Rectangle<float> area,
                       bool isDown, bool isOver, Colour noteFillColour) override {
        MidiKeyboardComponent::drawBlackNote(midiNoteNumber, g, area, isDown, isOver, noteFillColour);
        if (midiNoteNumber == highlightedNote) {
            g.setColour(Colours::orange.withAlpha(0.7f));
            g.fillRect(area);
        }
    }

private:
    int highlightedNote = -1;
};

class MainComponent : public Component,
                      public Timer,
                      public MidiKeyboardStateListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(Graphics&) override;
    void drawPhaseVelocityBarChart(Graphics& g, const Rectangle<int>& area);
    void drawHarmonicPhaseVelocityPlot(Graphics& g, const Rectangle<int>& area);
    void showTemperamentDialog();
    void stepRootNote(int delta);

    void resized() override;
    void timerCallback() override;

    void updateCurrentNote();
    void handleNoteOn(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

private:
    void calculateTrueDrift();
    void handleOnsetEvents();
    void pushNoteHistory(int midiNoteNumber);
    void appendCurrentPhaseVelocity();
    void clearNoteHistories();

    static constexpr int kTopKHarmonics = 4;
    static constexpr int kImageHeight = 512;
    static constexpr int kNumPhasePartials = 8;
    static constexpr int kHistoryFrames = 512;
    static constexpr int kNoteHistoryCount = 8;
    static constexpr int kNoteHistoryFrames = 512;
    static constexpr int kNoteHistoryAdvancePerTick = 4;
    static constexpr int kSpectrogramHeight = 64;
    static constexpr int kNumColours = 64;
    static constexpr int kPhaseVelocityWidth = 20;
    static constexpr float kPhaseSmoothing = 0.05f; // Exponential smoothing factor (0 to 1)
    static constexpr float kBarChartMaxVelocity = 0.1f; // Maximum velocity for scaling bars
    static constexpr float kPlotMaxVelocity = 0.1f;

    struct NoteHistory {
        int midiNote = 60;
        int writeIndex = 0;
        int length = 0;
        int sequence = 0;
        bool active = false;
        std::array<float, kNoteHistoryFrames> values{};
    };

    MidiKeyboardState keyboardState;

    std::unique_ptr<OscAudioProcessor> processor;
    std::unique_ptr<HighlightKeyboard> keyboard;
    std::unique_ptr<TemperamentControls> temperamentControls;
    std::unique_ptr<RealTimePitchTracker> pitchTracker;
    std::unique_ptr<DialogWindow> temperamentDialog;

    int lastClickedMidiNote = 60;
    float trueDrift = 0.0f;
    float keyboardKeyWidth = 24.0f;

    Image phaseVelocityBar;
    Image cyclogram, spectrogram, phasigram;
    Rectangle<int> plotBounds;
    ScopedAlloc<Float32> resampleBuffer{kImageHeight};
    ScopedAlloc<Float32> workBuffer;
    ScopedAlloc<Float32> prevPhases{kNumPhasePartials};
    ScopedAlloc<Float32> phaseDiff{kNumPhasePartials};
    ScopedAlloc<Float32> phaseVelocity{kNumPhasePartials};
    ScopedAlloc<Float32> avgMagnitudes{kNumPhasePartials};
    Transform transform;
    std::array<NoteHistory, kNoteHistoryCount> noteHistories{};
    int currentNoteHistory = -1;
    int noteSequenceCounter = 0;

    GradientColourMap viridis, inferno;
    GradientColourMap bipolar;

    void updateHistoryImage();
    void drawHistoryImage(Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
