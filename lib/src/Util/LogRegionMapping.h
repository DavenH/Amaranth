#pragma once

#include <cmath>

#include "../App/AppConstants.h"
#include "../Array/Buffer.h"

class LogRegionMapping {
public:
    static constexpr int defaultMidiNote = 48;
    static constexpr double defaultSampleRate = 44100.0;
    static constexpr float defaultTensionScale = 0.5f;

    explicit LogRegionMapping(
            int midiNoteToUse,
            double sampleRateToUse = defaultSampleRate,
            float tensionScaleToUse = defaultTensionScale) :
            size          (calculateSize(
                    jlimit((int) Constants::LowestMidiNote,
                           (int) Constants::HighestMidiNote,
                           midiNoteToUse),
                    sampleRateToUse))
        ,   tension      ((float) size * tensionScaleToUse)
        ,   leftOffset   ((std::pow(tension + 1.f, 0.05f) - 1.f) / tension)
        ,   inverseRange (1.f / (1.f - leftOffset))
        ,   inverseLog   (1.f / std::log(tension + 1.f)) {}

    int regionSize() const { return size; }

    void fillDisplayUnits(Buffer<float> destination) const {
        if (destination.empty()) {
            return;
        }

        const float step = destination.size() > 1
                ? (1.f - leftOffset) / (float) (destination.size() - 1)
                : 0.f;
        destination.ramp(leftOffset, step)
                .mul(tension)
                .add(1.f)
                .ln()
                .mul(inverseLog);
    }

    void fillSourceUnits(Buffer<float> destination) const {
        if (destination.empty()) {
            return;
        }

        const float step = destination.size() > 1
                ? 1.f / (float) (destination.size() - 1)
                : 0.f;
        destination.ramp(0.f, step)
                .mul(std::log(tension + 1.f))
                .exp()
                .add(-1.f)
                .mul(1.f / tension)
                .add(-leftOffset)
                .clip(0.f, 1.f - leftOffset)
                .mul(inverseRange);
    }

    float displayUnitForSourceUnit(float sourceUnit) const {
        const float mappedSource = leftOffset
                + jlimit(0.f, 1.f, sourceUnit) * (1.f - leftOffset);
        return std::log(tension * mappedSource + 1.f) * inverseLog;
    }

private:
    static int calculateSize(int midiNote, double sampleRate) {
        const double baseFrequency = MidiMessage::getMidiNoteInHertz(midiNote - 12);
        return jmax(2, (int) std::ceil(0.5 * sampleRate / baseFrequency));
    }

    int size {};
    float tension {};
    float leftOffset {};
    float inverseRange {};
    float inverseLog {};
};
