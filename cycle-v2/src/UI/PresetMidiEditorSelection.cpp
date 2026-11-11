#include "UI/PresetMidiEditor.h"

namespace CycleV2 {

bool PresetMidiEditor::isNoteSelected(int index) const {
    return selectedNotes.count(index) != 0;
}

void PresetMidiEditor::selectNote(int index) {
    selectedNotes.clear();
    selectedNote = index;
    if (index >= 0) {
        selectedNotes.insert(index);
    }
}

juce::Rectangle<float> PresetMidiEditor::noteBounds(const PresetMidiNote& note) const {
    const auto grid = noteGrid();
    const float height = grid.getHeight() / 24.f;
    return { xForTime(note.startSeconds),
            grid.getY() + (lowestPitch + 23 - note.pitch) * height,
            juce::jmax(3.f, xForTime(note.startSeconds + note.durationSeconds)
                    - xForTime(note.startSeconds)), height };
}

void PresetMidiEditor::updateBoxSelection(juce::Point<float> position) {
    selectionBox = juce::Rectangle<float>(gestureStart, position).getIntersection(noteGrid());
    selectedNotes = selectionBeforeBox;
    for (size_t index = 0; index < sequence.notes.size(); ++index) {
        if (selectionBox->intersects(noteBounds(sequence.notes[index]))) {
            selectedNotes.insert((int) index);
        }
    }
    selectedNote = selectedNotes.empty() ? -1 : *selectedNotes.begin();
    repaint();
}

bool PresetMidiEditor::moveSelectedNotes(int semitones, double seconds) {
    for (const int index : selectedNotes) {
        const auto& note = sequence.notes[(size_t) index];
        if (note.pitch + semitones < 0 || note.pitch + semitones > 127
                || note.startSeconds + seconds < 0.0
                || note.startSeconds + note.durationSeconds + seconds
                        > PresetMidiSequence::maximumDurationSeconds) {
            return true;
        }
    }
    if (!selectedNotes.empty()) {
        releaseAudition();
        for (const int index : selectedNotes) {
            auto& note = sequence.notes[(size_t) index];
            note.pitch += semitones;
            note.startSeconds += seconds;
            extendDurationTo(note.startSeconds + note.durationSeconds);
        }
        publish();
    }
    return true;
}

void PresetMidiEditor::dragSelectedNotes(juce::Point<float> position) {
    int semitones = pitchAt(position.y) - pitchAt(gestureStart.y);
    double seconds = timeAt(position.x) - timeAt(gestureStart.x);
    for (const auto& [index, note] : draggedNotes) {
        semitones = juce::jlimit(-note.pitch, 127 - note.pitch, semitones);
        seconds = juce::jlimit(-note.startSeconds,
                PresetMidiSequence::maximumDurationSeconds
                        - note.startSeconds - note.durationSeconds, seconds);
    }
    for (const auto& [index, original] : draggedNotes) {
        auto& note = sequence.notes[(size_t) index];
        note.pitch = original.pitch + semitones;
        note.startSeconds = original.startSeconds + seconds;
        extendDurationTo(note.startSeconds + note.durationSeconds);
    }
}

}
