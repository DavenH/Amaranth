#pragma once

#include <JuceHeader.h>

#include <optional>
#include <vector>

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

class PresetMidiEditor final :
        public juce::Component
    ,   private juce::Timer {
public:
    using ChangeCallback = std::function<void(PresetMidiSequence)>;
    using AuditionCallback = std::function<void(int, int, bool)>;
    using PlaybackTimeCallback = std::function<std::optional<double>()>;

    PresetMidiEditor(PresetMidiSequence phrase, ChangeCallback callback,
            std::function<void()> togglePlayback = {},
            AuditionCallback audition = {},
            PlaybackTimeCallback playbackTime = {},
            std::vector<int> mappedControllers = {});
    ~PresetMidiEditor() override;

    juce::Rectangle<float> noteGrid() const;
    juce::Rectangle<float> velocityGrid() const;
    juce::Rectangle<float> controlGrid() const;
    juce::Rectangle<float> controlGrid(int controller) const;
    juce::Rectangle<float> miniMap() const;
    const std::vector<int>& controllerLanes() const { return controllers; }

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
            const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;

    void paintNoteGrid(juce::Graphics& graphics) const;
    void paintNotes(juce::Graphics& graphics) const;
    void paintVelocity(juce::Graphics& graphics) const;
    void paintModulation(juce::Graphics& graphics) const;
    void paintModulationLane(juce::Graphics& graphics, int controller) const;
    void paintMiniMap(juce::Graphics& graphics) const;
    void paintPlaybackCursor(juce::Graphics& graphics) const;
    int pitchAt(float y) const;
    double timeAt(float x) const;
    double rawTimeAt(float x) const;
    float xForTime(double time) const;
    double visibleDuration() const;
    double gridStep() const;
    double timeOffset() const;
    int maximumTimeOffsetSteps() const;
    int velocityAt(float y) const;
    juce::Range<int> overviewPitches() const;
    int noteAt(juce::Point<float> point) const;
    int velocityNoteAt(float x) const;
    void navigateMiniMap(juce::Point<float> point);
    void updateDurationRange();
    void audition(int pitch, int velocity);
    void releaseAudition();
    void configureControllers(std::vector<int> mappedControllers);
    int controllerAt(juce::Point<float> point) const;
    void editControl(juce::Point<float> point, int controller);
    void publish();

    PresetMidiSequence sequence;
    ChangeCallback onChange;
    std::function<void()> onTogglePlayback;
    AuditionCallback onAudition;
    PlaybackTimeCallback playbackTime;
    juce::TextButton clearButton { "Clear" };
    std::vector<int> controllers;
    juce::Label durationLabel;
    juce::Slider durationSlider;
    int lowestPitch { 48 };
    int activeNote { -1 };
    int activeVelocity { -1 };
    int selectedNote { -1 };
    int activeControl { -1 };
    int selectedControl { -1 };
    int auditionPitch { -1 };
    int timeOffsetSteps {};
    double minimumDurationSeconds { 0.01 };
    double durationBeforeDrag {};
    float horizontalWheelRemainder {};
    float verticalWheelRemainder {};
    juce::Point<float> gestureStart;
    PresetMidiNote originalNote;
    bool draggingMiniMap {};
    bool resizingNote {};
    bool draggingDuration {};
    bool velocitySelected {};
    bool wasPlaybackActive {};
    bool pendingChange {};
};

}
