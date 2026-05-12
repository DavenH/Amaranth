#include "AudioHub.h"

#include <memory>
#include "AudioSourceProcessor.h"
#include "PluginProcessor.h"
#include "../Algo/AutoModeller.h"
#include "../App/SingletonRepo.h"
#include "../App/MeshLibrary.h"
#include "../App/Settings.h"
#include "../Definitions.h"
#include "../UI/IConsole.h"
#include "../Util/NumberUtils.h"
#include "../Util/Util.h"

AudioHub::AudioHub(SingletonRepo* repo) :
        SingletonAccessor(repo, "AudioHub")
    ,   sampleRate  (44100)
    ,   bufferSize  (1)
    ,   deviceError (String())
    ,   currentProcessor(nullptr) {
}

AudioHub::~AudioHub() {
  #if !PLUGIN_MODE
    if (audioDeviceManager != nullptr) {
        audioDeviceManager->removeAudioCallback(this);
        audioDeviceManager->removeMidiInputDeviceCallback(String(), &midiCollector);
    }
  #endif

    currentProcessor = nullptr;
    audioSourcePlayer.setSource(nullptr);
    if (audioDeviceManager != nullptr) {
        audioDeviceManager->closeAudioDevice();
    }
}

AudioDeviceManager& AudioHub::ensureAudioDeviceManager() {
    if (audioDeviceManager == nullptr) {
        audioDeviceManager = std::make_unique<AudioDeviceManager>();
    }

    return *audioDeviceManager;
}

void AudioHub::initialiseAudioDevice(XmlElement* midiSettings) {
  #if !PLUGIN_MODE
    auto& deviceManager = ensureAudioDeviceManager();
    const String error(deviceManager.initialise(1, 2, midiSettings, true));
    AudioIODevice* device = deviceManager.getCurrentAudioDevice();
    AudioDeviceManager::AudioDeviceSetup setup;

    deviceManager.getAudioDeviceSetup(setup);
    bufferSize = setup.bufferSize;
    sampleRate = setup.sampleRate;

    settingListeners.call(&SettingListener::samplerateChanged, sampleRate);
    settingListeners.call(&SettingListener::bufferSizeChanged, bufferSize);

    if (error.isNotEmpty() || !device) {
        deviceError = error;
    } else {
        // start the IO device pulling its data from our callback.
        deviceManager.addAudioCallback(this);
        deviceManager.addMidiInputDeviceCallback ({}, &midiCollector);
        audioSourcePlayer.setSource(this);
    }

    deviceListeners.call(&DeviceListener::audioDeviceIsReady);
  #endif
}

void AudioHub::suspendAudio() {
#if !PLUGIN_MODE
    if (audioDeviceManager == nullptr) {
        return;
    }

    audioDeviceManager->removeAudioCallback(this);
    audioDeviceManager->removeMidiInputDeviceCallback(String(), &midiCollector);
#endif
}

void AudioHub::resumeAudio() {
#if !PLUGIN_MODE
    if (audioDeviceManager == nullptr) {
        return;
    }

    audioDeviceManager->addAudioCallback(this);
    audioDeviceManager->addMidiInputDeviceCallback({}, &midiCollector);
#endif
}

#if !PLUGIN_MODE

void AudioHub::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int totalNumInputChannels,
    float* const* outputChannelData,
    int totalNumOutputChannels,
    int numSamples,
    const AudioIODeviceCallbackContext& context) {
    // pass the audio callback on to our player source, and also the waveform display comp
    audioSourcePlayer.audioDeviceIOCallbackWithContext(
        inputChannelData,
        totalNumInputChannels,
        outputChannelData,
        totalNumOutputChannels,
        numSamples,
        context
    );
}

void AudioHub::audioDeviceAboutToStart(AudioIODevice* device) {
    audioSourcePlayer.audioDeviceAboutToStart(device);
}

void AudioHub::audioDeviceStopped() {
    audioSourcePlayer.audioDeviceStopped();
}

#endif

AudioDeviceManager* AudioHub::getAudioDeviceManager() {
    return &ensureAudioDeviceManager();
}

void AudioHub::stopAudio() {
    if (audioDeviceManager == nullptr) {
        return;
    }

#if !PLUGIN_MODE
    audioDeviceManager->removeAudioCallback(this);
#endif
    audioDeviceManager->closeAudioDevice();

    deviceListeners.call(&DeviceListener::audioDeviceIsUnready);
}

void AudioHub::prepareToPlay(int newBlockSize, double sampleRate) {
    ScopedLock sl(repo->getInitLock());

    if(Util::assignAndWereDifferent(this->sampleRate, sampleRate)) {
        settingListeners.call(&SettingListener::samplerateChanged, sampleRate);
    }

    if(sampleRate != 44100.0) {
        settingListeners.call(&SettingListener::becameRareSamplerate, sampleRate);
    }

    if(Util::assignAndWereDifferent(bufferSize, newBlockSize)) {
        settingListeners.call(&SettingListener::bufferSizeChanged, bufferSize);
    }

    midiCollector.reset(sampleRate);

    if(currentProcessor != nullptr) {
        currentProcessor->prepareToPlay(newBlockSize, sampleRate);
    }
}

void AudioHub::releaseResources() {
    if (currentProcessor != nullptr) {
        currentProcessor->releaseResources();
    }
}

void AudioHub::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) {
    buffer.clear();

    midiCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

  #if PLUGIN_MODE
    if(getObj(PluginProcessor).isSuspended()) {
        return;
    }
  #endif

    if(currentProcessor != nullptr) {
        currentProcessor->processBlock(buffer, midiMessages);
    }
}

void AudioHub::setAudioSourceProcessor(AudioSourceProcessor* processor) {
    currentProcessor = processor;

    if (currentProcessor != nullptr && sampleRate > 0 && bufferSize > 0) {
        currentProcessor->prepareToPlay(bufferSize, sampleRate);
    }
}

String AudioHub::getDeviceErrorAndReset() {
    String copy = deviceError;
    deviceError = String();

    return copy;
}
