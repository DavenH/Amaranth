#include "OscAudioProcessor.h"
#include "RealTimePitchTracker.h"
#include <cmath>

OscAudioProcessor::OscAudioProcessor(RealTimePitchTracker* pitchTracker)
    :   workBuffer(1024 * 4)
    ,   workBufferUI(1024 * 128)
    ,   rwBufferAudioThread(1024 * 4)
    ,   pitchTracker(pitchTracker)
{
}

OscAudioProcessor::~OscAudioProcessor() {
}

void OscAudioProcessor::start() {
    deviceManager = std::make_unique<AudioDeviceManager>();
    auto error = deviceManager->initialise(1, 0, nullptr, true);
    if (error.isNotEmpty()) {
        std::cout << error << std::endl;;
    }
    jassert(error.isEmpty());
    deviceManager->addAudioCallback(this);
}

void OscAudioProcessor::stop() {
    deviceManager->removeAudioCallback(this);
}

void OscAudioProcessor::setTargetFrequency(float freq) {
    auto sampleRate = getCurrentSampleRate();
    targetPeriod    = sampleRate / freq;
}

std::vector<Buffer<Float32>> OscAudioProcessor::getAudioPeriods() const {
    const SpinLock::ScopedLockType lock(bufferLock);

    // return a copy because underlying vector will be updated by audio thread
    return {periods};
}

void OscAudioProcessor::audioDeviceAboutToStart(AudioIODevice* device) {
    sampleRateHz = device->getCurrentSampleRate();
    auto freq = device->getCurrentSampleRate() / targetPeriod;
    setTargetFrequency(freq);
    pitchTracker->setSampleRate(device->getCurrentSampleRate());
}

// called from UI thread
void OscAudioProcessor::resetPeriods() {
    const SpinLock::ScopedLockType lock(bufferLock);
    periods.clear();
    workBufferUI.resetPlacement();
}

void OscAudioProcessor::audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const AudioIODeviceCallbackContext& context) {
    if (numInputChannels == 0) {
        return;
    }

    Buffer input(const_cast<float*>(inputChannelData[0]), numSamples);
    jassert(numSamples <= workBuffer.size());

    Buffer<float> currentSamples = workBuffer.section(0, numSamples);
    input.copyTo(currentSamples);

    pitchTracker->write(currentSamples);
    currentSamples.mul(2.0f).tanh();

    detectOnsets(currentSamples);
    appendSamplesRetractingPeriods(currentSamples);

    for (int i = 0; i < numOutputChannels; ++i) {
        if (outputChannelData[i]) {
            Buffer output(outputChannelData[i], numSamples);
            output.set(0);
        }
    }
}

void OscAudioProcessor::appendSamplesRetractingPeriods(Buffer<float>& audioBlock) {
    if (!rwBufferAudioThread.hasRoomFor(audioBlock.size())) {
        // std::cout << "Retracting, " << accumulatedSamples << " " << targetPeriod << std::endl;
        rwBufferAudioThread.retract();
    }
    rwBufferAudioThread.write(audioBlock);

    float period = targetPeriod; // copy for thread safety
    const SpinLock::ScopedLockType lock(bufferLock);

    accumulatedSamples += (float) audioBlock.size();
    int periodThisTime = (int) (accumulatedSamples + period) - (int) accumulatedSamples;

    while (accumulatedSamples >= (float) periodThisTime && rwBufferAudioThread.hasDataFor(periodThisTime)) {
        Buffer<float> periodData   = rwBufferAudioThread.read(periodThisTime);
        Buffer<float> periodDataUI = workBufferUI.place(periodData.size());
        periodData.copyTo(periodDataUI);
        applyDynamicRangeCompression(periodDataUI);
        periods.push_back(periodDataUI);
        accumulatedSamples -= period;
        periodThisTime = (int)(accumulatedSamples + period) - (int) accumulatedSamples;
    }
}

void OscAudioProcessor::applyDynamicRangeCompression(Buffer<float> audio) {
    float min = 0, max = 0;
    audio.minmax(min, max);
    float absMax = jmax(std::abs(max), std::abs(min));
    if (absMax < 0.001) {
        return;
    }

    float scalingFactor = 1.0f / absMax;
    float adjustedScalingFactor = std::pow(scalingFactor, 0.66f);
    audio.mul(adjustedScalingFactor);
}

double OscAudioProcessor::getCurrentSampleRate() const {
    return deviceManager->getCurrentAudioDevice()->getCurrentSampleRate();
}

void OscAudioProcessor::detectOnsets(Buffer<float>& audioBlock) {
    if (audioBlock.size() == 0) {
        return;
    }

    totalSamplesProcessed += audioBlock.size();
    samplesSinceLastOnset += audioBlock.size();

    const float rms = audioBlock.normL2() / std::sqrt((float) audioBlock.size());
    onsetEnvelope = (1.0f - kOnsetEnvSmoothing) * onsetEnvelope + kOnsetEnvSmoothing * rms;

    const float flux = rms - previousRms;
    previousRms = rms;

    const bool aboveThreshold = rms > jmax(kOnsetMinRms, onsetEnvelope * kOnsetThresholdRatio);
    const bool fastRise = flux > kOnsetFluxThreshold;
    const int64 minSamplesBetween = (int64) (kOnsetMinIntervalSec * sampleRateHz);

    if (aboveThreshold && fastRise && samplesSinceLastOnset >= minSamplesBetween) {
        int start1 = 0, block1 = 0, start2 = 0, block2 = 0;
        onsetFifo.prepareToWrite(1, start1, block1, start2, block2);
        if (block1 > 0) {
            onsetQueue[(size_t) start1] = {
                totalSamplesProcessed / sampleRateHz,
                rms
            };
            onsetFifo.finishedWrite(1);
            samplesSinceLastOnset = 0;
        }
    }
}

void OscAudioProcessor::popOnsetEvents(std::vector<OnsetEvent>& out) {
    out.clear();
    int start1 = 0, block1 = 0, start2 = 0, block2 = 0;
    onsetFifo.prepareToRead(kOnsetQueueSize, start1, block1, start2, block2);
    if (block1 > 0) {
        for (int i = 0; i < block1; ++i) {
            out.push_back(onsetQueue[(size_t) (start1 + i)]);
        }
    }
    if (block2 > 0) {
        for (int i = 0; i < block2; ++i) {
            out.push_back(onsetQueue[(size_t) (start2 + i)]);
        }
    }
    onsetFifo.finishedRead(block1 + block2);
}
