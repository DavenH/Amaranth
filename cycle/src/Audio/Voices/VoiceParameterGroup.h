#pragma once
#include <vector>
#include <Algo/Resampler.h>
#include <Array/RingBuffer.h>
#include "../../Wireframe/CycleState.h"

using std::vector;

class VoiceParameterGroup
{
public:
    VoiceParameterGroup() {
        reset();
    }

    int samplesThisCycle{};
    int unisonIndex{};

    long sampledFrontier{};

    double cumePos{};
    double angleDelta{};

    float padding				[2][7]{};
    double samplingSpillover	[2]{};
    Buffer<float> lastLerpHalf	[2];	// copy of first half of biased cycle for next round
    ReadWriteBuffer cycleBuffer	[2];
    Resampler resamplers		[2];

    // one for each mesh layer
    vector<CycleState> layerStates;

    VoiceParameterGroup(const VoiceParameterGroup& group) {
        this->operator =(group);
    }

    VoiceParameterGroup& operator=(const VoiceParameterGroup& group) {
        cumePos 				= group.cumePos;
        sampledFrontier 		= group.sampledFrontier;
        angleDelta 				= group.angleDelta;
        unisonIndex				= group.unisonIndex;
        samplesThisCycle		= group.samplesThisCycle;

        for(int i = 0; i < 2; ++i) {
            cycleBuffer[i] 		 = group.cycleBuffer[i];
            lastLerpHalf[i] 	 = group.lastLerpHalf[i];
            resamplers[i] 		 = group.resamplers[i];
            samplingSpillover[i] = group.samplingSpillover[i];

            for(int j = 0; j < 7; ++j) {
                padding[i][j] = group.padding[i][j];
            }
        }

        layerStates = group.layerStates;

        return *this;
    }

    void reset() {
        cycleBuffer[0].reset();
        cycleBuffer[1].reset();

        for(auto& layerState : layerStates) {
            layerState.reset();
        }

        cumePos 				= 0;
        sampledFrontier 		= 0;
        angleDelta 				= 0;
        samplesThisCycle		= 0;
        samplingSpillover[0]	= 0;
        samplingSpillover[1]	= 0;
    }
};


