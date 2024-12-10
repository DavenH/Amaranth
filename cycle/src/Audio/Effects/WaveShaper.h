#ifndef _distortion_h
#define _distortion_h

#include <Algo/Oversampler.h>
#include <Array/ScopedAlloc.h>
#include <Audio/SmoothedParameter.h>
#include <Curve/MeshRasterizer.h>
#include <Obj/Ref.h>
#include <Util/NumberUtils.h>

#include "AudioEffect.h"

class WaveshaperUI;

class Waveshaper :
	public Effect
{
public:
	enum { Preamp, Postamp, numWaveshaperParams };
	enum { tableResolution = 2048 };
	static const int maxOversampleFactor = 8;

	Waveshaper(SingletonRepo* repo);

	void init();

	bool doesGraphicOversample();
	bool isEnabled() const;
	int getLatencySamples();
	void rasterizeTable();
	void processBuffer(AudioSampleBuffer& audioBuffer);
	void processVertexBuffer(Buffer<Ipp32f> outputBuffer);
	void updateSmoothedParameters(int deltaSamples);
	bool doParamChange(int param, double value, bool doFurtherUpdate);
	void audioThreadUpdate();
	void clearGraphicDelayLine();

	void setPendingOversampleFactor(int factor);
	void setRasterizer(MeshRasterizer* rasterizer)	{ this->rasterizer = rasterizer; 				 			}
	void setUI(WaveshaperUI* comp)					{ this->ui = comp; 								 			}
	int getOversampleFactor() const					{ return oversamplers[0]->getOversampleFactor(); 			}

	inline void linInterpTable(float& value)
	{
		float fval = value * (tableResolution - 1);
		int index = (int) fval;
		float remainder = fval - index;
		value = (1.f - remainder) * table[index] + remainder * table[(index + 1) & (tableResolution - 1)];
	}

	static double calcPostamp(double value)			{ return NumberUtils::fromDecibels(45 * (2 * value - 1)); 	}
	static double calcPreamp(double value)			{ return calcPostamp(value); 								}

private:
	static const int graphicOvspIndex = 2;
	int pendingOversampleFactor;

	Ref<MeshRasterizer> rasterizer;
	Ref<WaveshaperUI> ui;

	Buffer<float> workBuffer;
	ScopedAlloc<Ipp32f> rampBuffer;
	ScopedAlloc<Ipp32f> table;
	ScopedAlloc<Ipp32f> graphicOversampleBuf;
	ScopedAlloc<Ipp32f> oversampleBuffers;

	ScopedPointer<Oversampler> oversamplers[3];
	CriticalSection graphicLock;

	SmoothedParameter preamp;
	SmoothedParameter postamp;
};

#endif
