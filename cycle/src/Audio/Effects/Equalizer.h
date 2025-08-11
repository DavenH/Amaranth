#pragma once
#include "JuceHeader.h"
#include <vector>

#include <Array/ScopedAlloc.h>
#include <Audio/Filters/Butterworth.h>
#include <Audio/Filters/Filter.h>
#include <Audio/SmoothedParameter.h>
#include <Obj/Ref.h>
#include "AudioEffect.h"

using std::vector;

class GuilessEffect;
class EqualizerUI;

class Equalizer :
		public Effect
{
public:
	static const int numPartitions = 5;
	static const int lowShelfPartition = 0;
	static const int highShelfPartition = numPartitions - 1;

	static const int numEqChannels = 3;
	static const int graphicEqChannel = numEqChannels - 1;

	static const int tapsPerBiquad = 6;

	enum Knobs { Band1Gain, Band2Gain, Band3Gain, Band4Gain, Band5Gain,
				 Band1Freq, Band2Freq, Band3Freq, Band4Freq, Band5Freq,
				 numParams };
private:

	class EqPartition
	{
	public:
		EqPartition() :
			gainDB(0.)
		,	centreFreq(100.)
		,	numCascades(1)
		{
			for(auto& state : states)
				state = nullptr;
		}

		IppsIIRState64f_32f* states[numEqChannels]{};
		Buffer<Ipp64f> delayLine[numEqChannels];
		Buffer<Ipp64f> taps[numEqChannels];

		Dsp::Cascade* cascade{};
		int numCascades;
		SmoothedParameter gainDB;
		SmoothedParameter centreFreq;
	};

public:
	explicit Equalizer(SingletonRepo* repo);
	~Equalizer() override;

	/* Rendering */
	void clearGraphicBuffer();
	void processBuffer(AudioSampleBuffer& buffer) override;
	void processVertexBuffer(Buffer<Float32> inputBuffer);

	/* Updating */
	void updatePartition(int idx, bool canUpdateTaps);
	bool doParamChange(int index, double value, bool doFurtherUpdate) override;
	void updateSmoothedParameters(int deltaSamples);

	/* Accessors */
	bool isEnabled() const override;
	double getFrequencyTarget(int idx) const	{ return partitions[idx].centreFreq.getTargetValue(); 	}
	double getGainTarget(int idx) const			{ return partitions[idx].gainDB.getTargetValue(); 		}
	double getFrequency(int idx) const			{ return partitions[idx].centreFreq.getCurrentValue(); 	}
	double getGain(int idx) const				{ return partitions[idx].gainDB.getCurrentValue();		}
	void setUI(GuilessEffect* comp) 			{ ui = comp; }
	void setSampleRate(double samplerate);
	void updateParametersToTarget();

	/* Arithmetic */
	static double calcGain(double value)		{ return 60. * (value - 0.5); }
	static double calcFreq(double value, double logTension);
	static double calcKnobValue(double value, double logTension);

	// void test();

private:
	Ref<GuilessEffect> ui;

	CriticalSection stateLock;

	void freeStates();
	void updateFilters();
	void updateStates();

	double samplerate;
	int filterOrder;
	bool isButterworth;

	EqPartition partitions[numPartitions];

	Dsp::SimpleFilter <Dsp::Butterworth::LowShelf<2>> lsFilter;
	Dsp::SimpleFilter <Dsp::Butterworth::BandShelf<2>> bsFilter[numPartitions - 2];
	Dsp::SimpleFilter <Dsp::Butterworth::HighShelf<2>> hsFilter;

	ScopedAlloc<Float32> overflowBuffer;
	ScopedAlloc<Ipp64f> tapsBuffer;
	ScopedAlloc<Ipp64f> delayBuffer;
	ScopedAlloc<Int8u> stateBuffer;


};
