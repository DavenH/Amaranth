#pragma once

#include <JuceHeader.h>

#include <optional>
#include <vector>

#include "Reference/ReferenceSampleAnalysis.h"

namespace CycleV2 {

struct ReferenceRecord {
    juce::String id;
    juce::String name;
    juce::File audioFile;
    bool factory {};
    int editedFundamental { -1 };
    std::vector<Vertex2> editedPitchPath;
};

class ReferenceLibrary {
public:
    ReferenceLibrary(juce::File factoryDirectory, juce::File userDirectory);

    void reload();
    const std::vector<ReferenceRecord>& records() const { return entries; }
    const ReferenceRecord* selectedRecord() const;
    const ReferenceSampleAnalysis* selectedAnalysis() const;
    const ReferenceSampleAnalysis* analysisFor(const juce::String& id) const;
    std::vector<juce::String> openReferenceIds() const;
    const juce::String& selectedId() const { return currentId; }
    bool select(const juce::String& id, juce::String& error);
    bool close(const juce::String& id);
    bool importFile(const juce::File& source, juce::String& error);
    bool setFundamental(int midiNote);
    bool movePitchPoint(size_t index, juce::Point<float> position);
    bool replacePitchPath(std::vector<Vertex2> points);
    bool undoPitchEdit();
    bool redoPitchEdit();
    bool spectralViewFor(const juce::String& id) const;
    bool setSpectralView(const juce::String& id, bool spectral);

private:
    struct PitchEdit {
        int note {};
        std::vector<Vertex2> path;
    };

    struct OpenReference {
        juce::String id;
        ReferenceSampleAnalysis analysis;
        bool spectral {};
    };

    static bool supportedAudioFile(const juce::File& file);
    static std::optional<ReferenceRecord> readRecord(
            const juce::File& metadata,
            const juce::File& audioFile,
            bool factory);
    static juce::var writeRecord(const ReferenceRecord& record);
    juce::File metadataFile(const ReferenceRecord& record) const;
    void saveRecord(const ReferenceRecord& record) const;
    bool applyPitchEdit(PitchEdit edit, bool recordUndo);
    PitchEdit currentPitchEdit() const;

    juce::File factoryDirectory;
    juce::File userDirectory;
    std::vector<ReferenceRecord> entries;
    juce::String currentId;
    std::vector<OpenReference> openReferences;
    std::vector<PitchEdit> undoStack;
    std::vector<PitchEdit> redoStack;
};

}
