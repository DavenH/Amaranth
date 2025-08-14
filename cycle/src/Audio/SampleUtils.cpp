#include <Algo/PitchTracker.h>
#include <App/AppConstants.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/Multisample.h>
#include <Audio/PitchedSample.h>
#include <Definitions.h>
#include <Design/Updating/Updater.h>
#include <UI/IConsole.h>
#include <Util/NumberUtils.h>

#include "SampleUtils.h"

#include <App/Directories.h>

#include "WavAudioSource.h"
#include "../App/Initializer.h"
#include "../Audio/AudioSourceRepo.h"
#include "../Audio/SynthAudioSource.h"
#include "../CycleDefs.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../Inter/SpectrumInter2D.h"
#include "../UI/Panels/GeneralControls.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/Panels/OscControlPanel.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/Panels/SynthMenuBarModel.h"
#include "../UI/VertexPanels/Spectrum2D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"
#include "../UI/Widgets/MidiKeyboard.h"
#include "../Util/CycleEnums.h"

SampleUtils::SampleUtils(SingletonRepo* repo) :
        SingletonAccessor(repo, "SampleUtils") {
}

void SampleUtils::init() {
    multisample = &getObj(Multisample);
    audioRepo = &getObj(AudioSourceRepo);
}

int SampleUtils::calcFundDelta() {
    int fundDiff = 0;
    int numSamples 	= multisample->size();

    if(numSamples > 3) {
        PitchedSample* testSamples[] = {
            multisample->getSampleAt(numSamples / 4),
            multisample->getSampleAt(numSamples / 2),
            multisample->getSampleAt(jmin(numSamples - 1, 3 * numSamples / 4))
        };

        int diffs[] = { 0, 0, 0 };
        int numGood = 0;

        for (int i = 0; i < numElementsInArray(testSamples); ++i) {
            PitchedSample* sample = testSamples[i];

            if (sample == nullptr || sample->fundNote < 10) {
                continue;
            }

            tracker->setSample(sample);
            tracker->trackPitch();

            if (tracker->getConfidence() < 10) {
                float wavFrequency = sample->samplerate / sample->getAveragePeriod();
                int fundCalced     = roundToInt(NumberUtils::frequencyToNote(wavFrequency));

                ++numGood;
                diffs[i] = fundCalced - sample->fundNote;
            }
        }

        float avg = numGood > 0 ? (diffs[0] + diffs[1] + diffs[2]) / numGood : 0;
        fundDiff = roundToInt(avg / 12.f) * 12;
    }

    return fundDiff;
}

void SampleUtils::processWav(bool isMulti, bool invokerIsDialog) {
//	ScopedLock sl(wavSource->getLock());

    vector<PitchedSample*> samples;

    if (isMulti) {
        int index = 0;
        PitchedSample* sample = nullptr;

        while ((sample = multisample->getSampleAt(index++)) != nullptr) {
            samples.push_back(sample);

            jassert(sample->mesh != nullptr);
        }
    } else {
        samples.push_back(multisample->getCurrentSample());
    }

    int fundDiff = 0;
    bool noteRangesChanged = false;

    if(isMulti)
        fundDiff = calcFundDelta();

    Range midiRange(getConstant(LowestMidiNote), getConstant(HighestMidiNote));

    for (auto & i : samples) {
        PitchedSample* sample = i;

        if (sample == nullptr || sample->size() == 0) {
            if(! isMulti) {
                showConsoleMsg("Wave too short or empty");
            }

            continue;
        }

        tracker->setSample(sample);

        if (isMulti && NumberUtils::within(sample->fundNote, midiRange)) {
            // check actual frequency to see if fundamental note derived from filename is accurate
            if (fundDiff != 0) {
                sample->fundNote += fundDiff;
                sample->midiRange += fundDiff;

                noteRangesChanged = true;
            }

            sample->createDefaultPeriods();
            PitchTracker::refineFrames(sample, sample->getAveragePeriod());
        } else {
            tracker->trackPitch();
        }

        float insecurity = tracker->getConfidence();

        if (sample->periods.empty()) {
            if (!isMulti)
                showConsoleMsg("Wave too short or empty");

            continue;
        }

        if (insecurity > 50) {
            sample->createDefaultPeriods();

            if(sample->fundNote < 10)
                sample->fundNote = 41;
        } else {
            if (!NumberUtils::within<int>(sample->fundNote, midiRange)) {
                float wavFrequency 	= sample->samplerate / sample->getAveragePeriod();

                sample->fundNote = roundToInt(NumberUtils::frequencyToNote(wavFrequency));
            }
        }

        waveNoteChanged(sample, isMulti, invokerIsDialog);
    }

    if (isMulti) {
        if (noteRangesChanged) {
            multisample->fillRanges();
        }

        multisample->performUpdate(Update);

        if(PitchedSample* current = multisample->getCurrentSample()) {
            getObj(MidiKeyboard).setAuditionKey(current->fundNote);
        }
    }

    if(invokerIsDialog) {
        getObj(OscControlPanel).setLengthInSeconds(getWavLengthSeconds());
    }
}

void SampleUtils::waveNoteChanged(PitchedSample* sample, bool isMulti, bool invokerIsDialog) {
    if (!getSetting(WaveLoaded) || sample == nullptr) {
        showImportant("No audio file loaded.");
        return;
    }

    if (!isMulti && invokerIsDialog) {
        getObj(MidiKeyboard).setAuditionKey(sample->fundNote);
        getObj(MorphPanel).setKeyValueForNote(sample->fundNote);
    }

    auto& pitchRast = getObj(EnvPitchRast);

    sample->createEnvFromPeriods(isMulti);
    sample->createPeriodsFromEnv(static_cast<MeshRasterizer*>(&pitchRast));
}


void SampleUtils::shiftWaveNoteOctave(bool up) {
    multisample->shiftAllByOctave(up);

    getObj(Updater).update(UpdateSources::SourceMorph);
}

void SampleUtils::updateMidiNoteNumber(int note) {
    if (!getSetting(WaveLoaded)) {
        return;
    }

    double period = 44100.0 / NumberUtils::noteToFrequency(note, 0);

    if (PitchedSample* sample = multisample->getCurrentSample()) {
        vector<PitchFrame>& frames = sample->periods;
        frames.clear();

        float increment = sample->size() * 0.01f;
        float cume = 0;

        for(int i = 0; i < 100; ++i) {
            PitchFrame frame;
            frame.sampleOffset 	= roundToInt(cume);
            frame.period 		= period;

            frames.push_back(frame);
            cume += increment;
        }

        sample->fundNote = note;
        tracker->setSample(sample);
        PitchTracker::refineFrames(sample, period);

        waveNoteChanged(sample, false, false);

        getObj(Updater).update(UpdateSources::SourceMorph);
    }
}

void SampleUtils::unloadWav() {
    audioRepo->setAudioProcessor(AudioSourceRepo::SynthSource);

    multisample->clear();
}

void SampleUtils::resetWavOffset() {
    audioRepo->getWavAudioSource()->reset();
}

void SampleUtils::waveOverlayChanged(bool shouldDrawWave) {
    progressMark

    getObj(PlaybackPanel).stopPlayback();

    if (shouldDrawWave) {
        getObj(SynthAudioSource).allNotesOff();
        audioRepo->setAudioProcessor(AudioSourceRepo::WavSource);

        if (PitchedSample* sample = multisample->getCurrentSample()) {
            getObj(MidiKeyboard).setAuditionKey(sample->fundNote);
        }
    } else {
        audioRepo->getWavAudioSource()->allNotesOff();
        audioRepo->setAudioProcessor(AudioSourceRepo::SynthSource);
    }

    if (Util::assignAndWereDifferent(getSetting(DrawWave), shouldDrawWave)) {
        getObj(SpectrumInter2D).setClosestHarmonic(0);
        getObj(EnvelopeInter2D).waveOverlayChanged();

        if(shouldDrawWave) {
            showImportant(getObj(Directories).getLoadedWavePath());
        }

        if (getSetting(DrawWave) && getSetting(WaveLoaded)) {
            if (getSetting(ViewStage) < ViewStages::PostEnvelopes) {
                getObj(SynthMenuBarModel).triggerClick(SynthMenuBarModel::ViewStageB);
            } else {
                doUpdate(SourceMorph);
            }
        } else {
            doUpdate(SourceMorph);
        }

        getObj(GeneralControls).updateHighlights();
        int numCols = getObj(Waveform3D).getWindowWidthPixels();
        getObj(VisualDsp).rasterizeEnv(LayerGroups::GroupPitch, numCols);
    }
}

float SampleUtils::getWavLengthSeconds() {
    return multisample->getGreatestLengthSeconds();
}

