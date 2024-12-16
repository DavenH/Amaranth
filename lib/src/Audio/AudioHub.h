#pragma once

#include "AudioSourceProcessor.h"
#include "../App/SingletonAccessor.h"
#include "../Definitions.h"
#include "../Thread/PendingAction.h"
#include "JuceHeader.h"

class AudioHub:
  	  #if ! PLUGIN_MODE
		public AudioIODeviceCallback,
  	  #endif
		public AudioSourceProcessor
	,	public SingletonAccessor {
public:
	class SettingListener {
    public:
        enum {
				SampleRateAction
			,	BufferSizeAction
			,	RareSampleRateAction
			,	LastActionEnum
		};

		SettingListener() :
				sampleRateAction	(SampleRateAction)
			,	bufferSizeAction	(BufferSizeAction)
			,	rareSamplerateAction(RareSampleRateAction) {
        }

        void samplerateChanged(int samplerate) {
            sampleRateAction.setValueAndTrigger(samplerate);
        }

        void becameRareSamplerate(int samplerate) {
            rareSamplerateAction.setValueAndTrigger(samplerate);
        }

        void bufferSizeChanged(int newSize) {
            bufferSizeAction.setValueAndTrigger(newSize);
		}

	protected:
		PendingActionValue<int> sampleRateAction;
		PendingActionValue<bool> rareSamplerateAction;
		PendingActionValue<int> bufferSizeAction;
	};

	class DeviceListener {
	public:
		virtual ~DeviceListener() = default;

		virtual void audioDeviceIsReady() 	= 0;
		virtual void audioDeviceIsUnready() = 0;
	};

    /* ----------------------------------------------------------------------------- */

	explicit AudioHub(SingletonRepo* repo);
	~AudioHub() override;

  #if ! PLUGIN_MODE
	void audioDeviceIOCallbackWithContext(
		const float* const* inputChannelData,
		int totalNumInputChannels,
		float* const* outputChannelData,
		int totalNumOutputChannels,
		int numSamples,
		const AudioIODeviceCallbackContext& context) override;

	void audioDeviceAboutToStart(AudioIODevice* device) override;
	void audioDeviceStopped() override;
	void resumeAudio();


	void stopAudio();
	void suspendAudio();

	AudioDeviceManager* getAudioDeviceManager();
  #endif

	void releaseResources() override;
	void initialiseAudioDevice(XmlElement*);
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
	void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;

	AudioSourceProcessor* getAudioSourceProcessor() const { return currentProcessor; }
	void setAudioSourceProcessor(AudioSourceProcessor* processor) { currentProcessor = processor; }

	String getDeviceErrorAndReset();

	void resetKeyboardState() 					{ keyboardState.reset(); 		 	}
	void addListener(SettingListener* listener) { settingListeners.add(listener); 	}
	void addListener(DeviceListener* listener) 	{ deviceListeners.add(listener); 	}
	int getSampleRate() const 					{ return sampleRate; 			 	}
	int getBufferSize() const 					{ return bufferSize; 			 	}
	MidiKeyboardState& getKeyboardState() 		{ return keyboardState; 			}

protected:
	int sampleRate, bufferSize;

	String 				 deviceError;
	AudioDeviceManager 	 audioDeviceManager;
	AudioSourcePlayer 	 audioSourcePlayer;
	MidiMessageCollector midiCollector;
	MidiKeyboardState 	 keyboardState;

	AudioSourceProcessor* currentProcessor;

	ListenerList<SettingListener> settingListeners;
	ListenerList<DeviceListener> deviceListeners;
};
