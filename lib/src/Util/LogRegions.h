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
        regions.initialize(
                LogRegionMapping::defaultSampleRate,
                getRealConstant(LogFreqTensionScale));
    }

    void clear() {
        regions.clear();
    }

    Buffer<float> getRegion(int midiKey) {
        return regions.get(midiKey);
    }

    static void prepareDefaultRegions() {
        defaultRegions();
    }

    static Buffer<float> getDefaultRegion(int midiKey) {
        return defaultRegions().get(midiKey);
    }

private:
    class RegionBank {
    public:
        void initialize(double sampleRate, float tensionScale) {
            midiRange = Range<int>(
                    Constants::LowestMidiNote,
                    Constants::HighestMidiNote);

            vector<int> sizes;
            int totalSize = 0;
            for (int midiNote = midiRange.getStart();
                    midiNote <= midiRange.getEnd();
                    ++midiNote) {
                const int size = LogRegionMapping(
                        midiNote,
                        sampleRate,
                        tensionScale).regionSize();
                sizes.push_back(size);
                totalSize += size;
            }

            memory.resize(totalSize);
            memory.resetPlacement();
            frequencyRamps.resize(sizes.size());
            for (int index = 0; index < (int) frequencyRamps.size(); ++index) {
                Buffer<float>& ramp = frequencyRamps[(size_t) index];
                ramp = memory.place(sizes[(size_t) index]);
                LogRegionMapping(
                        midiRange.getStart() + index,
                        sampleRate,
                        tensionScale).fillDisplayUnits(ramp);
            }
        }

        void clear() {
            for (auto& frequencyRamp : frequencyRamps) {
                frequencyRamp.nullify();
            }
            memory.clear();
        }

        Buffer<float> get(int midiKey) {
            NumberUtils::constrain(midiKey, midiRange);
            return frequencyRamps[(size_t) (midiKey - midiRange.getStart())];
        }

    private:
        Range<int> midiRange;
        ScopedAlloc<float> memory;
        vector<Buffer<float>> frequencyRamps;
    };

    class DefaultRegionBank final : public RegionBank {
    public:
        DefaultRegionBank() {
            initialize(
                    LogRegionMapping::defaultSampleRate,
                    LogRegionMapping::defaultTensionScale);
        }
    };

    static RegionBank& defaultRegions() {
        static DefaultRegionBank regions;
        return regions;
    }

    RegionBank regions;
};
