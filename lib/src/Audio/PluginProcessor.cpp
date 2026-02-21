#if PLUGIN_MODE

#include "PluginProcessor.h"

#include <App/AppConstants.h>
#include <UI/IConsole.h>
#include <Util/ScopedBooleanSwitcher.h>

#include "AudioHub.h"
#include "AudioSourceProcessor.h"
#include "../App/SingletonRepo.h"
#include "../App/DocumentLibrary.h"
#include "../App/Settings.h"
#include "../Obj/Ref.h"
#include "../App/Settings.h"
#include "../Definitions.h"


PluginProcessor::PluginProcessor() :
        suspendStateRead(false)
    ,   presetOpenTime(0)
{
    // Set up some default values.
    lastPosInfo.resetToDefault();

    repo = new SingletonRepo();
    repo->init();
    repo->setPluginProcessor(this);

    audioHub = &getObj(AudioHub);
}

PluginProcessor::~PluginProcessor() {
    repo->setPluginProcessor(nullptr);
}

float PluginProcessor::getParameter(int index) {
    if (isPositiveAndBelow(index, (int) parameters.size())) {
        parameters[index].getValue();
    }

    return 0;
}

void PluginProcessor::setParameter(int index, float value) {
    if (isPositiveAndBelow(index, (int) parameters.size())) {
        parameters[index].setValue(value);
    }
}

const String PluginProcessor::getParameterName(int index) {
    if (isPositiveAndBelow(index, (int) parameters.size()))
        return parameters[index].name;

    return String();
}

const String PluginProcessor::getParameterText(int index) {
    return String (getParameter (index), 2);
}

void PluginProcessor::addParameter(const Parameter& param) {
    parameters.push_back(param);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    audioHub->prepareToPlay(samplesPerBlock, sampleRate);
}

void PluginProcessor::releaseResources() {
    audioHub->resetKeyboardState();
}

void PluginProcessor::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) {
    // ask the host for the current time so we can display it...
    AudioPlayHead::CurrentPositionInfo newTime;

    if (getPlayHead() != 0 && getPlayHead()->getCurrentPosition(newTime)) {
        double lastBpm = lastPosInfo.bpm;

        // Successfully got the current time from the host..
        lastPosInfo = newTime;

        if (lastBpm != lastPosInfo.bpm) {
            listeners.call(&Listener::tempoChanged, lastPosInfo.bpm);
        }
    } else {
        // If the host fails to fill-in the current time, we'll just clear it to a default.
        lastPosInfo.resetToDefault();
    }

    audioHub->processBlock(buffer, midiMessages);
}

void PluginProcessor::getStateInformation(MemoryBlock& destData) {
    if (!suspendStateRead) {
        XmlElement  xml ("PluginSettings");
        MemoryBlock xmlBlock;

        getObj(Settings).saveGlobalSettings(&xml);
        copyXmlToBinary (xml, xmlBlock);

        MemoryOutputStream out(destData, false);

        out.writeInt(getConstant(DocMagicCode));
        out.writeInt(xmlBlock.getSize());
        out.write(xmlBlock.getData(), xmlBlock.getSize());

        getObj(Document).save(&out);
    }
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (!suspendStateRead) {
        MemoryBlock xmlBlock;
        MemoryInputStream in(data, sizeInBytes, false);

        if (in.readInt() == getConstant(DocMagicCode)) {
            int xmlSize = in.readInt();
            in.readIntoMemoryBlock(xmlBlock, xmlSize);
            std::unique_ptr xmlState(getXmlFromBinary(xmlBlock.getData(), xmlBlock.getSize()));

            if (xmlState != 0 && xmlState->hasTagName("PluginSettings"))
                getObj(Settings).readGlobalSettings(xmlState.get());
            else {
                jassertfalse;
            }

            suspendProcessing(true);
            ScopedLock lock(getObj(AudioHub).getLock());

            if (getObj(Document).open(&in)) {
                getObj(DocumentLibrary).setShouldOpenDefault(false);
            }

            suspendProcessing(false);
        } else {
            showConsoleMsg("Error Opening preset");
        }
    }
}

void PluginProcessor::setCurrentProgramStateInformation(const void* data, int sizeInBytes) {
  #ifdef AU_PLUGIN_MODE
    setStateInformation(data, sizeInBytes);
  #endif
}

void PluginProcessor::getCurrentProgramStateInformation(MemoryBlock& destData)
{
  #ifdef AU_PLUGIN_MODE
    getStateInformation(destData);
  #endif
}

int PluginProcessor::getNumPrograms() {
    return getObj(DocumentLibrary).getSize();
}

int PluginProcessor::getCurrentProgram() {
    return getObj(DocumentLibrary).getCurrentIndex();
}

void PluginProcessor::setCurrentProgram(int index) {
    int64 currentTime = Time::currentTimeMillis();

    if (currentTime - presetOpenTime < 400 || currentTime - creationTime < 400) {
        return;
    }

    getObj(DocumentLibrary).triggerAsyncLoad(index);
}

const String PluginProcessor::getProgramName(int index) {
    return getObj(DocumentLibrary).getProgramName(index);
}

bool PluginProcessor::isParameterAutomatable(int index) const {
    if (isPositiveAndBelow(index, (int) parameters.size())) {
        return parameters[index].isAutomable;
    }

    return true;
}

void PluginProcessor::updateLatency() {
    int totalLatency = 0;
    bool realtime    = isNonRealtime();

    for (auto* adder : latencies) {
        totalLatency += adder->getLatency(realtime);
    }
    ScopedBooleanSwitcher switcher(suspendStateRead);
    setLatencySamples(totalLatency);
}

const String PluginProcessor::getName() const {
    return   getObj(AppConstants).getStringAppConstant(Constants::ProductName);
    // return getStrConstant(ProductName);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PluginProcessor();
}

void PluginProcessor::presetHasLoaded() {
    presetOpenTime = Time::currentTimeMillis();
}

#endif
