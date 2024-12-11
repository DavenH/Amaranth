#ifndef _scratchrasterizer_h
#define _scratchrasterizer_h

#include <Curve/EnvRasterizer.h>

class EnvRenderContext
{
public:
	EnvRenderContext(const EnvRasterizer& env, int layerIndex) :
			rast(env)
		,	scratchTime(0)
		,	layerIndex(layerIndex)
		, 	sampleable(false)
	{
	}

	bool sampleable;
	int layerIndex;
	float scratchTime;

	EnvRasterizer rast;
	Buffer<float> rendBuffer;
};

struct EnvRastGroup
{
	vector<EnvRenderContext> envGroup;
	int layerGroup;

	int size() { return (int) envGroup.size(); }
	EnvRenderContext& operator[] (const int index) { return envGroup[index]; }

	explicit EnvRastGroup(int layerGroup) : layerGroup(layerGroup) {}
};

typedef vector<EnvRenderContext>::iterator RenderIter;

#endif
