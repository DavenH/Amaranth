#pragma once
#include <vector>
#include <App/SingletonAccessor.h>

using std::vector;

class Multisample;
class PitchTracker;
class PitchedSample;
class AudioSourceRepo;

class SampleUtils :
    public SingletonAccessor
{
public:
    explicit SampleUtils(SingletonRepo* repo);
    ~SampleUtils() override = default;

    int calcFundDelta();
    void init() override;

    void unloadWav();
    void processWav(bool isMulti, bool invokerIsDialog);
    void resetWavOffset();
    void shiftWaveNoteOctave(bool up);
    float getWavLengthSeconds();
    void waveNoteChanged(PitchedSample*, bool isMulti, bool invokerIsDialog = true);
    void waveOverlayChanged(bool shouldDrawWave);
    void updateMidiNoteNumber(int note);

private:
    Ref<Multisample> multisample;
    Ref<PitchTracker> tracker;
    Ref<AudioSourceRepo> audioRepo;
};