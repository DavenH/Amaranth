#pragma once

#include <JuceHeader.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace CycleV2 {

enum class PresetPreviewView {
    Time,
    Spectrum
};

struct PresetPreviewImage {
    juce::MemoryBlock jpegData;
    int width {};
    int height {};
    PresetPreviewView view { PresetPreviewView::Spectrum };

    bool isValid() const;
};

struct PresetMidiNote {
    int pitch { 60 };
    int velocity { 100 };
    double startSeconds {};
    double durationSeconds { 0.5 };
};

struct PresetMidiControl {
    int controller { 1 };
    int value {};
    double timeSeconds {};
};

struct PresetMidiSequence {
    static constexpr size_t maximumEventsPerLane = 512;
    static constexpr double maximumDurationSeconds = 120.0;

    double durationSeconds { 4.0 };
    std::vector<PresetMidiNote> notes;
    std::vector<PresetMidiControl> controls;

    bool empty() const { return notes.empty() && controls.empty(); }
};

struct PresetPresentation {
    juce::String title;
    juce::String author;
    juce::String pack;
    juce::String description;
    juce::String timeSurfaceStyle;
    juce::String bipolarSpectralSurfaceStyle;
    juce::StringArray tags;
    bool tagsSpecified {};
    int rating {};
    std::optional<PresetPreviewImage> preview;
    juce::String patternId;
    std::optional<PresetMidiSequence> sequence;

    bool empty() const;
};

struct PresetPresentationDecodeResult {
    PresetPresentation presentation;
    juce::String warning;
};

class PresetPresentationCodec {
public:
    static juce::var writeJSON(const PresetPresentation& presentation);
    static PresetPresentationDecodeResult readJSON(const juce::var& value);
    static PresetPresentationDecodeResult readMetadataJSON(const juce::var& value);
    static juce::var writeSequenceJSON(const PresetMidiSequence& sequence);
    static std::optional<PresetMidiSequence> readSequenceJSON(const juce::var& value);
};

juce::String idForPresetPreviewView(PresetPreviewView view);
std::optional<PresetPreviewView> presetPreviewViewForId(const juce::String& id);

}
