#pragma once

#include <JuceHeader.h>

#include <optional>

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

struct PresetPresentation {
    juce::String author;
    juce::String pack;
    juce::String description;
    juce::StringArray tags;
    int rating {};
    std::optional<PresetPreviewImage> preview;

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
};

juce::String idForPresetPreviewView(PresetPreviewView view);
std::optional<PresetPreviewView> presetPreviewViewForId(const juce::String& id);

}
