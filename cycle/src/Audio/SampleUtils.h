#ifndef SAMPLEUTILS_H_
#define SAMPLEUTILS_H_


#include <vector>
#include <App/SingletonAccessor.h>

using std::vector;

class Multisample;
class PitchTracker;
class SampleWrapper;
class AudioSourceRepo;

class SampleUtils :
	public SingletonAccessor
{
public:
	SampleUtils(SingletonRepo* repo);
	virtual ~SampleUtils();

	int calcFundDelta();
	void init();

	void unloadWav();
	void processWav(bool isMulti, bool invokerIsDialog);
	void resetWavOffset();
	void shiftWaveNoteOctave(bool up);
	float getWavLengthSeconds();
	void waveNoteChanged(SampleWrapper*, bool isMulti, bool invokerIsDialog = true);
	void waveOverlayChanged(bool shouldDrawWave);
	void updateMidiNoteNumber(int note);

private:
	Ref<Multisample> multisample;
	Ref<PitchTracker> tracker;
	Ref<AudioSourceRepo> audioRepo;
};

#endif
