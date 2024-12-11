#pragma once
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
