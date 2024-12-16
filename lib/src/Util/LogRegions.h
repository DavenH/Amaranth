#pragma once
#include <vector>
#include "JuceHeader.h"
#include "../Array/Buffer.h"
#include "../Array/ScopedAlloc.h"
#include "../App/SingletonAccessor.h"
#include "../App/AppConstants.h"
#include "../App/SingletonRepo.h"
#include "../Definitions.h"
#include "../Obj/Ref.h"
#include "../Util/NumberUtils.h"

using std::vector;

class LogRegions : public SingletonAccessor {
public:
    explicit LogRegions(SingletonRepo* repo) :
            SingletonAccessor(repo, "LogRegions") {
    }

    ~LogRegions() override = default;

    void init() override {
        const double samplerate = 44100.0;

        float ampTension = getConstant(LogTension);
        midiRange = Range<int>(getConstant(LowestMidiNote), getConstant(HighestMidiNote));

        vector<int> sizes;
        int totalSize = 0;

        for (int i = 0; i < midiRange.getLength() + 1; ++i) {
            int key = midiRange.getStart() + i - 12;

            int size = ceilf(0.5f * samplerate / MidiMessage::getMidiNoteInHertz(key));
            sizes.push_back(size);
            totalSize += size;
        }

        memory.resize(totalSize);
        frequencyRamps.resize(sizes.size());

        for (int i = 0; i < frequencyRamps.size(); ++i) {
			Buffer<float>& ramp = frequencyRamps[i];
			ramp = memory.place(sizes[i]);

			int size 		= sizes[i];
			float tension 	= size * ampTension;
			float leftOffset= (powf(tension + 1, 0.05f) - 1) / float(tension);
			float ix 		= (1.f - leftOffset) / float(size - 1.f);
			float iln 		= 1 / logf(tension + 1.f);

			ramp.ramp(leftOffset, ix).mul(tension).add(1.f).ln().mul(iln);
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
	vector<Buffer<float> > frequencyRamps;
};
