#pragma once

#include <Curve/MeshRasterizer.h>
#include <Obj/Ref.h>

class CycleState;

class VoiceMeshRasterizer : public MeshRasterizer
{
private:
	JUCE_LEAK_DETECTOR(VoiceMeshRasterizer)

public:
	CycleState* state;

	VoiceMeshRasterizer(SingletonRepo* repo);
	virtual ~VoiceMeshRasterizer();

	/*
	 * Chains the minimum necessary number of points from previous call to head of the subsequent call
	 * This provides 100% continuity between cycles
	 */
	void calcCrossPointsChaining(float phase);
	void updateCurves();
	void orphanOldVerts();
	void setState(CycleState* state) { this->state = state; }
};
