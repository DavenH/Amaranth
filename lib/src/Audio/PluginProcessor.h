#pragma once
#include "../Definitions.h"

#if PLUGIN_MODE

#ifndef _pluginprocessor_h
#define _pluginprocessor_h


#include <vector>
#include "Parameter.h"
#include "../App/Vst/juce_PluginHostType.h"
#include "../Obj/Ref.h"
#include "JuceHeader.h"
#include <PluginCharacteristics.h>

using std::vector;

class SingletonRepo;

class PluginProcessor :
		public AudioProcessor
	,	public Document::Listener
{
public:
	class Listener
	{
	public:
		virtual void tempoChanged(double bpm) = 0;
	};

	class LatencyAdder
	{
	public:
		virtual int getLatency(bool isRealtime) = 0;
	};

    /* ----------------------------------------------------------------------------- */

    PluginProcessor();
    virtual ~PluginProcessor();

    void releaseResources();
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);
    void documentAboutToLoad() {}
    void documentHasLoaded();
    void updateLatency();

    void setParameter (int index, float newValue);
    void addParameter(const Parameter& param);
    int getNumParameters();
    bool isParameterAutomatable 	(int parameterIndex) const;
    float getParameter 				(int index);
    const String getParameterName 	(int index);
    const String getParameterText 	(int index);

    void changeProgramName (int index, const String& newName) {}
    void setCurrentProgram (int index);
    const String getProgramName (int index);
    int getCurrentProgram();

    void getCurrentProgramStateInformation (MemoryBlock& destData);
    void getStateInformation (MemoryBlock& destData);
    void setCurrentProgramStateInformation (const void* data, int sizeInBytes);
    void setStateInformation (const void* data, int sizeInBytes);

    /* ����������������������������������������������������������������������������� */

    bool acceptsMidi() const									{ return true; 						}
    bool hasEditor() const                  					{ return true; 						}
    bool isInputChannelStereoPair (int index) const				{ return true; 						}
    bool isOutputChannelStereoPair (int index) const			{ return true; 						}
    bool silenceInProducesSilenceOut() const 					{ return false; 					}
    bool producesMidi() const									{ return true; 						}
    int getNumPrograms()										{ return parameters.size(); 		}
    double getTailLengthSeconds() const    						{ return 0; 			   			}
    AudioPlayHead::CurrentPositionInfo getCurrentPosition() 	{ return lastPosInfo; 				}

    const String getInputChannelName (int channelIndex) const 	{ return String (channelIndex + 1); }
    const String getOutputChannelName (int channelIndex) const	{ return String (channelIndex + 1); }
    const String getName() const            					{ return String (JucePlugin_Name); 	}

    void setSuspendStateRead(bool suspend) 						{ suspendStateRead = suspend; 		}

    virtual AudioProcessorEditor* createEditor() = 0;

private:
    bool suspendStateRead;
    int totalLatency;
    int64 presetOpenTime, creationTime;

    AudioPlayHead::CurrentPositionInfo lastPosInfo;

	Ref<SingletonRepo>	 repo;
	Ref<AudioHub>		 audioHub;

    vector<Parameter> 	 parameters;
    Array<LatencyAdder*> latencies;
    ListenerList 	 	 listeners;

    PluginHostType hostType;

	friend class PluginWindow;

    JUCE_DECLARE_NON_COPYABLE(PluginProcessor);
};

#endif

#endif // PLUGIN_MODE
