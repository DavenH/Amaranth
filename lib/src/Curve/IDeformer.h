#pragma once

#include "../Array/Buffer.h"

class IDeformer {
public:
	enum { tableSize = 0x2000 };

	struct NoiseContext {
    	int noiseSeed;
    	short vertOffset, phaseOffset;

    	NoiseContext() : noiseSeed(0), vertOffset(0), phaseOffset(0) {}
    };

	virtual float getTableValue(int guideIndex, float progress, const NoiseContext& context) = 0;
	virtual void sampleDownAddNoise(int index, Buffer<float> dest, const NoiseContext& context) = 0;
	virtual Buffer<Ipp32f> getTable(int index) = 0;
	virtual int getTableDensity(int index) = 0;
};