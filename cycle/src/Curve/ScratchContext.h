#ifndef CURVE_SCRATCHCONTEXT_H_
#define CURVE_SCRATCHCONTEXT_H_

struct ScratchInflection
{
	Range<float> range;
	int startIndex;
};

struct ScratchContext
{
	Buffer<float> gridBuffer;
	Buffer<float> panelBuffer;

	vector<ScratchInflection> inflections;
};

#endif
