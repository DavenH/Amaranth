#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

#include <Audio/PitchedSample.h>

namespace CycleV2 {

struct ReferenceSampleAnalysis {
    std::unique_ptr<PitchedSample> sample;
    std::vector<Vertex2> pitchPath;
    std::vector<float> cyclogram;
    uint64_t cyclogramRevision {};
    int columns {};
    int rows {};
    float pitchInsecurity {};
    bool lowConfidence {};
    juce::String error;

    bool succeeded() const { return sample != nullptr && error.isEmpty(); }
    int standardMidiNote() const;
};

class ReferenceSampleAnalyzer {
public:
    static ReferenceSampleAnalysis analyze(
            const juce::File& file,
            int columns = 160,
            int rows = 256);
    static void refreshCyclogram(ReferenceSampleAnalysis& analysis);
};

}
