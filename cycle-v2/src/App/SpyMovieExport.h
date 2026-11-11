#pragma once

#include <JuceHeader.h>
#include <optional>

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

struct SpyMovieExportSettings {
    juce::File outputDirectory;
    juce::File ffmpeg;
    juce::String gradient;
    int width {1920};
    int height {1080};
    int fps {30};
    double bitrateMbps {8.0};
    double tailSeconds {1.0};

    juce::Result validate(const PresetMidiSequence& sequence) const;
    juce::var toJSON(const juce::String& preset, const PresetMidiSequence& sequence) const;
    static juce::File findFFmpeg();
};

// Owns no GUI, live document, audio device or automation connection.
class SpyMovieExportWorker final : private juce::Timer {
public:
    explicit SpyMovieExportWorker(juce::File job);
    ~SpyMovieExportWorker() override;
    void start();

private:
    struct State;
    void timerCallback() override;
    void finish(const juce::String& status, const juce::String& message);
    void publish(const juce::String& status, const juce::String& message);
    bool prepare(juce::String& error);
    bool renderFrame(juce::String& error);
    bool cancelled() const;

    juce::File jobFile;
    std::unique_ptr<State> state;
};

}
