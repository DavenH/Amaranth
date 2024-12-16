#pragma once

#if PLUGIN_MODE

#include <vector>
#include "Parameter.h"
#include "../Obj/Ref.h"
#include "JuceHeader.h"
#include "../App/Doc/Document.h"

class AudioHub;
using std::vector;
using namespace juce;

class SingletonRepo;

class PluginProcessor :
		public AudioProcessor
	,	public Document::Listener
{
public:
	class Listener {
	public:
		virtual ~Listener() = default;

		virtual void tempoChanged(double bpm) = 0;
	};

	class LatencyAdder {
	public:
		virtual ~LatencyAdder() = default;

		virtual int getLatency(bool isRealtime) = 0;
	};

    /* ----------------------------------------------------------------------------- */

    PluginProcessor();
    ~PluginProcessor() override;

    void releaseResources() override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;
    void documentAboutToLoad() override {}
    void documentHasLoaded() override;
    void updateLatency();

    void setParameter (int index, float newValue) override;
    void addParameter(const Parameter& param);
    int getNumPrograms() override;
    bool isParameterAutomatable 	(int parameterIndex) const override;
    float getParameter 				(int index) override;
    const String getParameterName 	(int index) override;
    const String getParameterText 	(int index) override;

    void changeProgramName (int index, const String& newName) override {}
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    int getCurrentProgram() override;

    void getCurrentProgramStateInformation (MemoryBlock& destData) override;
    void getStateInformation (MemoryBlock& destData) override;
    void setCurrentProgramStateInformation (const void* data, int sizeInBytes) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

	/* ----------------------------------------------------------------------------- */

    bool acceptsMidi() const override							{ return true; 				}
    bool hasEditor() const override                  			{ return true; 				}
    bool isInputChannelStereoPair (int index) const override	{ return true; 				}
    bool isOutputChannelStereoPair (int index) const override	{ return true; 				}
    bool silenceInProducesSilenceOut() const override 			{ return false; 			}
    bool producesMidi() const override							{ return true; 				}
    double getTailLengthSeconds() const override    			{ return 0; 			   	}
    AudioPlayHead::CurrentPositionInfo getCurrentPosition() 	{ return lastPosInfo; 		}
	void setSuspendStateRead(bool suspend) 						{ suspendStateRead = suspend; }
    int getNumParameters() override								{ return parameters.size(); }

    const String getInputChannelName (int channelIndex) const override 	{ return String (channelIndex + 1); }
    const String getOutputChannelName (int channelIndex) const override	{ return String (channelIndex + 1); }
    const String getName() const override;

    AudioProcessorEditor* createEditor() override = 0;

private:
    bool suspendStateRead;
    int totalLatency;
    int64 presetOpenTime, creationTime;

    AudioPlayHead::CurrentPositionInfo lastPosInfo;

	Ref<SingletonRepo>	 repo;
	Ref<AudioHub>		 audioHub;

    vector<Parameter> 	 parameters;
    Array<LatencyAdder*> latencies;
    ListenerList<Listener> listeners;

    PluginHostType hostType;

	friend class PluginWindow;

    JUCE_DECLARE_NON_COPYABLE(PluginProcessor);
};

#endif

// #endif // PLUGIN_MODE
