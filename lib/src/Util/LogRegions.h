#pragma once

#include <JuceHeader.h>

#include <vector>

#include "LogRegionMapping.h"
#include "NumberUtils.h"
#include "../Array/Buffer.h"
#include "../Array/ScopedAlloc.h"
#include "../App/AppConstants.h"
#include "../App/SingletonAccessor.h"
#include "../App/SingletonRepo.h"
#include "../Definitions.h"
#include "../Obj/Ref.h"

using std::vector;

class LogRegions : public SingletonAccessor {
public:
    explicit LogRegions(SingletonRepo* repo) :
            SingletonAccessor(repo, "LogRegions") {
    }

    ~LogRegions() override = default;

    void init() override {
        const double samplerate = LogRegionMapping::defaultSampleRate;
        const float freqTension = getRealConstant(LogFreqTensionScale);
        midiRange = Range<int>(Constants::LowestMidiNote, Constants::HighestMidiNote);

        vector<int> sizes;
        int totalSize = 0;

        for (int i = 0; i < midiRange.getLength() + 1; ++i) {
            const int midiNote = midiRange.getStart() + i;
            const int size = LogRegionMapping(
                    midiNote,
                    samplerate,
                    freqTension).regionSize();
            sizes.push_back(size);
            totalSize += size;
        }

        memory.resize(totalSize);
        frequencyRamps.resize(sizes.size());

        for (int i = 0; i < frequencyRamps.size(); ++i) {
            Buffer<float>& ramp = frequencyRamps[i];
            ramp = memory.place(sizes[i]);
            LogRegionMapping(
                    midiRange.getStart() + i,
                    samplerate,
                    freqTension).fillDisplayUnits(ramp);
        }
    }

    void clear() {
        for (auto& frequencyRamp : frequencyRamps)
            frequencyRamp.nullify();

        memory.clear();
    }

    Buffer<float> getRegion(int midiKey) {
        NumberUtils::constrain(midiKey, midiRange);

        return frequencyRamps[midiKey - midiRange.getStart()];
    }

private:
    Range<int> midiRange;
    ScopedAlloc<float> memory;
    vector<Buffer<float>> frequencyRamps;
};
