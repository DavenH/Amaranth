#include <Algo/HermiteResampler.h>
#include <App/Settings.h>
#include <Audio/Multisample.h>
#include <Util/Arithmetic.h>

#include "WavAudioSource.h"
#include "../Audio/AudioSourceRepo.h"
#include "../Audio/SynthAudioSource.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include <Definitions.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>


WavAudioSource::SampleVoice::SampleVoice(SingletonRepo* repo, WavAudioSource* source) :
        SingletonAccessor(repo, "WavAudioSource")
    ,	sample(nullptr)
    ,	source(source)
    ,	isPlaying(false)
    ,	velocity(1.f) {
}

void WavAudioSource::SampleVoice::startNote(int midiNoteNumber,
                                            float velo, SynthesiserSound* sound,
                                            int currentPitchWheelPosition) {
    sample = getObj(Multisample).getSampleForNote(midiNoteNumber, velo);

    if (sample == nullptr) {
        stop(false);
        return;
    }

    sample->resetPosition();

    releasePos		= 0;
    offsetSamples 	= 0;
    isPlaying 		= true;
    fadingOut 		= false;
    velocity		= velo;
    lastNote		= midiNoteNumber;
    int nextPower 	= NumberUtils::nextPower2(source->blockSize);
    double srcFreq 	= NumberUtils::noteToFrequency(sample->fundNote, 0);
    double dstFreq 	= NumberUtils::noteToFrequency(midiNoteNumber);
    double srcSize 	= sample->samplerate / srcFreq;
    double dstSize 	= 44100. / dstFreq;
    double ratio	= srcSize / dstSize;

    Range<int> midiRange(Constants::LowestMidiNote, Constants::HighestMidiNote);
    getObj(MorphPanel).redDimUpdated(Arithmetic::getUnitValueForNote(midiNoteNumber, midiRange));
    getObj(MorphPanel).blueDimUpdated(1 - velo);

    stateMem.ensureSize(nextPower * 2);
    resMem.ensureSize(source->blockSize * 2);

    for (auto& i : resampleState) {
        i.init(stateMem.place(nextPower));
        i.reset(ratio);
    }
}

void WavAudioSource::SampleVoice::stopNote(float velocity, bool allowTailOff) {
    fadingOut = allowTailOff;

    if (!allowTailOff) {
        isPlaying = false;
        clearCurrentNote();
    }
}

void WavAudioSource::SampleVoice::renderNextBlock(AudioSampleBuffer& outputBuffer,
                                                  int startSample, int numSamples) {
    if (!isPlaying || sample == nullptr) {
        return;
    }

    ScopedLock sl(getObj(Multisample).getLock());

    SynthAudioSource* audioSource = &getObj(SynthAudioSource);

    if (fadingOut) {
        numSamples = jmin(audioSource->releaseDeclick.size() - releasePos, numSamples);
    }

    resMem.resetPlacement();
    StereoBuffer output(outputBuffer, startSample, numSamples);

    source->chanMemory.resetPlacement();
    StereoBuffer renderBuffer(output.numChannels);
    renderBuffer.left = source->chanMemory.place(numSamples);
    renderBuffer.right = source->chanMemory.place(numSamples);

    for (int i = 0; i < sample->audio.numChannels; ++i) {
        HermiteState& state = resampleState[i];

        if (state.srcToDstRatio == 1.f) {
            Buffer<float> input = sample->audio[i].sectionAtMost(offsetSamples, numSamples);
            VecOps::mul(input, velocity, renderBuffer[i]);

            if (sample->audio.numChannels < output.numChannels) {
                input = sample->audio[i].sectionAtMost(offsetSamples, numSamples);
                VecOps::mul(input, velocity, renderBuffer.right);
            }

            if (input.size() < numSamples) {
                stop(false);
            }
        } else {
            int resampOffset = (int) (state.srcToDstRatio * offsetSamples + 0.999999999);
            int resampSize = (int) (state.srcToDstRatio * (offsetSamples + numSamples) + 0.999999999) - resampOffset;

            Buffer<float> src = sample->audio[i].sectionAtMost(resampOffset, resampSize);
            Buffer<float> dst = resMem.place(numSamples);

            int size = 0;

            if (!src.empty()) {
                size = state.resample(src, dst);
                VecOps::mul(dst.withSize(size), velocity, renderBuffer[i]);

                if (sample->audio.numChannels < output.numChannels) {
                    VecOps::mul(dst.withSize(size), velocity, renderBuffer.right);
                }
            }

            if (size < dst.size() && i == sample->audio.numChannels - 1) {
                stop(false);
            }
        }
    }

    if (fadingOut) {
        int samplesToCopy = jmin(numSamples, audioSource->releaseDeclick.size() - releasePos);

        if (samplesToCopy > 0) {
            for (int i = 0; i < renderBuffer.numChannels; ++i) {
                renderBuffer[i].withSize(samplesToCopy).mul(audioSource->releaseDeclick + releasePos);
            }

            releasePos += samplesToCopy;
        }

        if (releasePos >= audioSource->releaseDeclick.size()) {
            stop(false);
        }
    }

    output.add(renderBuffer);

    offsetSamples += numSamples;
}

WavAudioSource::WavAudioSource(SingletonRepo* repo) : SingletonAccessor(repo, "WavAudioSource"),
                                                      updateOffsetFlag(false) {
    for (int i = 0; i < 6; ++i) {
        auto* voice = new SampleVoice(repo, this);
        voices.add(voice);
        synth.addVoice(voice);
    }

    synth.addSound(new SampleSound(repo));
}

void WavAudioSource::allNotesOff() {
    ScopedLock sl(audioLock);

    for (auto voice: voices) {
        voice->stop(false);
    }
}

void WavAudioSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    blockSize = samplesPerBlockExpected;
    synth.setCurrentPlaybackSampleRate(sampleRate);

    chanMemory.ensureSize(blockSize * 2);
}

void WavAudioSource::initResampler() {

    // int blockSize = getObj(AudioSourceRepo).getBufferSize();
    //
    // if(wav != nullptr && wav->samplerate != 44100.0)
    // {
    //     int inRate, outRate;
    //     int srcSize, dstSize;
    //     Arithmetic::getInputOutputRates(inRate, outRate, wav->samplerate);
    //
    //     resampler[0].initFixedWithLength(0.95f, 9.f, outRate, inRate, 10, blockSize, srcSize, dstSize);
    //     resampler[1].initFixedWithLength(0.95f, 9.f, outRate, inRate, 10, blockSize, srcSize, dstSize);
    //
    //     resamplingMem.ensureSize(2 * (srcSize + dstSize));
    //
    //     for(int i = 0; i < 2; ++i)
    //     {
    //         Resampler& r = resampler[i];
    //         r.source 	 = resamplingMem.place(srcSize);
    //         r.dest 		 = resamplingMem.place(dstSize);
    //
    //         r.reset();
    //         r.primeWithZeros();
    //
    //         resampleAccum[i].allocate(blockSize * 2);
    //         resampleAccum[i].reset();
    //         resampleAccum[i].write(0.f);
    //     }
    // }
    //
    // 	bool isStereo  = buffer.getNumChannels() > 1;
    //     int numSamples = buffer.getNumSamples();
    //     int maxSamples = 0;
    //
    //     if(wav->samplerate != 44100.0)
    //     {
    //         double ratio     = wav->samplerate / 44100.0;
    //
    //         int resampOffset = (int) (ratio * (offset + numSamples) + 0.999999999);
    //         int resampSize 	 = resampOffset - (int) (ratio * offset + 0.999999999);
    //
    //         maxSamples = jmin(resampSize, wav->buffer.getNumSamples() - resampOffset);
    //
    //         if(maxSamples > 0)
    //         {
    //             StereoBuffer out(buffer);
    //
    //             for(int i = 0; i < wav->audio.numChannels; ++i)
    //             {
    //                 Buffer<float> toResample = wav->audio[i].section(resampOffset, maxSamples);
    //                 resampleAccum[i].write(resampler[i].resample(toResample));
    //
    //                 Buffer<float> resampled = resampleAccum[i].readAtMost(numSamples);
    //                 resampled.copyTo(out[i]);
    //
    //                 if(out[i].size() > resampled.size()) {
    //                     out[i].offset(resampled.size()).zero();
    //                 }
    //             }
    //
    //             if(out.numChannels > wav->audio.numChannels) {
    //                 out.left.copyTo(out.right);
    //             }
    //         }
    //     }
    //     else
    //     {
    //     }
    //
    //     offset += numSamples;
}

void WavAudioSource::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) {
    ScopedLock lock(audioLock);

    if (updateOffsetFlag) {
        getObj(Multisample).updatePlaybackPosition();
        updateOffsetFlag = false;
    }

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

void WavAudioSource::positionChanged() {
    updateOffsetFlag = true;
}

void WavAudioSource::reset() {
    getObj(Multisample).updatePlaybackPosition();
}

bool WavAudioSource::SampleSound::appliesToNote(int midiNoteNumber) {
    return NumberUtils::within(midiNoteNumber,
                               Range(getConstant(LowestMidiNote), getConstant(HighestMidiNote)));
}

bool WavAudioSource::SampleSound::appliesToChannel(int midiChannel) {
    return true;
}
