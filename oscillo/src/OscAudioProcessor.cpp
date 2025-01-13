#include "OscAudioProcessor.h"

OscAudioProcessor::OscAudioProcessor()
    :   workBuffer(1024 * 4)
    ,   workBufferUI(1024 * 128)
    ,   rwBufferAudioThread(1024 * 4) {
}

OscAudioProcessor::~OscAudioProcessor() {
}

void OscAudioProcessor::start() {
    deviceManager = std::make_unique<AudioDeviceManager>();
    auto error = deviceManager->initialise(1, 0, nullptr, true);
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
    auto freq = device->getCurrentSampleRate() / targetPeriod;
    setTargetFrequency(freq);
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
    currentSamples.mul(3.0f).tanh();

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
        periods.push_back(periodDataUI);
        accumulatedSamples -= period;
        periodThisTime = (int)(accumulatedSamples + period) - (int) accumulatedSamples;
    }
}

double OscAudioProcessor::getCurrentSampleRate() const {
    return deviceManager->getCurrentAudioDevice()->getCurrentSampleRate();
}
