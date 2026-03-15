#pragma once

#include <Curve/MeshRasterizer.h>
#include <Obj/Ref.h>

class SingletonAccessor;
class CycleState;

/*
 * Voice rasterizers generate the audio-rate waveform for cycle playback.
 *
 * Contract:
 * - chain consecutive cycles without introducing a step at the phase boundary,
 * - treat the first call as a possible priming step,
 * - remain finite and phase-valid on every render,
 * - preserve continuity as morph and mesh state evolve across blocks.
 */
class VoiceMeshRasterizer :
	public MeshRasterizer,
	public SingletonAccessor
{
private:
	JUCE_LEAK_DETECTOR(VoiceMeshRasterizer)

public:
	CycleState* state;

	explicit VoiceMeshRasterizer(SingletonRepo* repo);

	/*
	 * Chains the minimum necessary number of points from previous call to head of the subsequent call
	 * This provides 100% continuity between cycles
	 */
	void calcCrossPointsChaining(float phase);
	void updateCurves() override;
	void orphanOldVerts();
	void setState(CycleState* state) { this->state = state; }
};
