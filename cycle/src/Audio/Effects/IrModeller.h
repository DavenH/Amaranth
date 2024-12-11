#ifndef _tubemodel_h
#define _tubemodel_h


#include <vector>
#include <ipp.h>
#include <ippdefs.h>
#include <cmath>

#include <Algo/ConvReverb.h>
#include <Algo/Oversampler.h>
#include <App/Transforms.h>
#include <Array/Buffer.h>
#include <Array/ScopedAlloc.h>
#include <Array/StereoBuffer.h>
#include <Audio/PitchedSample.h>
#include <Audio/SmoothedParameter.h>
#include <Curve/FXRasterizer.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>
#include <Thread/PendingAction.h>
#include "JuceHeader.h"

#include "AudioEffect.h"

using std::vector;

class IrModellerUI;

class IrModeller:
	public Effect
{
	class ConvState;

public:
	enum { Length, Postamp, Highpass, numTubeParams };

	enum PendingUpdate
	{
		impulseSize,
		convSize,
		blockSize,
		rasterize,
		unloadWav,
		prefilterChg
	};

	explicit IrModeller(SingletonRepo* repo);
	~IrModeller() override;

	void initGraphicVars();
	void cleanUp();
	void doPostWaveLoad();
	void doPostDeconvolve(int size);

	void processVertexBuffer(Buffer<Ipp32f> buffer);
	void processBuffer(AudioSampleBuffer& buffer) override;
	void audioThreadUpdate() override;
	void resetIndices();

	void trimWave();

	void clearGraphicOverlaps();
	void updateGraphicConvState(int graphicRes, bool force);
	void updateSmoothedParameters(int deltaSamples);
	void checkForPendingUpdates();

	/* Accessors */
	void setMesh(Mesh* mesh);

	bool 			willBeEnabled() const;
	void 			setUI(IrModellerUI* comp);
	bool 			isEnabled() const override		{ return enabled; 								}
	bool 			isWavLoaded() const 			{ return waveLoaded; 							}
	int 			getConvBufferSize() const		{ return convBufferSize; 						}
	int 			getLatencySamples() const		{ return willBeEnabled() ? convBufferSize : 0; 	}
	bool 			isUsingWave() const				{ return usingWavFile; 							}
	int 			getImpulseLength()				{ return audio.impulse.size(); 					}
	PitchedSample& 	getWrapper() 					{ return wavImpulse; 							}

	Buffer<float> 	getMagnitudes() 				{ return graphic.fft.getMagnitudes();			}
	Buffer<float> 	getGraphicImpulse() 			{ return graphic.impulse;						}

	static int 		calcLength(double value)		{ return (int) pow(2, int(7 + value * 7)); }
	static double 	calcKnobValue(int length) 		{ return (log(length) / log(2.0) - 7.) / 7.; }
	static double 	calcPostamp(double value)		{ return exp(10 * value - 5); 					}
	static double 	calcPrefilt(double value)		{ return value * value * value; 				}

	bool doParamChange(int param, double value, bool doFurtherUpdate) override;

	void setGraphicImpulseLength(int length);
	void rasterizeImpulseDirect();
	void rasterizeGraphicImpulse();
	void setPendingAction(PendingUpdate type, int value = -1);
	void audioFileModelled();

private:
	void filterImpulse(ConvState& chan);
	void rasterizeImpulse(Buffer<float> impulse, FXRasterizer& rast, bool isAudioThread);
	void unloadWave();
	void setImpulseLength(ConvState& state, int length);
	void setAudioImpulseLength(int length);
	void setAudioBlockSize(int size);
	void setConvBufferSize(int size);
	void calcPrefiltLevels(Buffer<float> buff);

	class ConvState
	{
	public:
		ConvState() : blockSize(1) {}

		ScopedAlloc<Ipp32f> memory;

		int blockSize;
		Buffer<float> impulse, rawImpulse;
		Buffer<float> levels;
		Transform fft;

		vector<BlockConvolver*> convolvers{};
	};

	// params
	SmoothedParameter preamp;
	SmoothedParameter postamp;
	SmoothedParameter prefilt;

	// flags
	bool usingWavFile;
	bool waveLoaded;
	bool enabled;

	int convBufferSize;
	int accumWritePos, convReadPos;

	ScopedAlloc<float> outputMem;
	StereoBuffer output;

	ConvState audio, graphic;

	BlockConvolver convolvers[2]{};
	BlockConvolver graphicConv{};

	Oversampler 	oversampler;
	FXRasterizer 	audioThdRasterizer;
	PitchedSample 	wavImpulse{};

	Ref<IrModellerUI> ui;
	Array<PendingAction*> pendingActions;

	PendingActionValue<int> unloadWavAction;
	PendingActionValue<int> convSizeAction;
	PendingActionValue<int> impulseSizeAction;
	PendingActionValue<int> blockSizeAction;
	PendingActionValue<int> prefiltAction;
	PendingActionValue<bool> rasterizeAction;


	IrModeller(const IrModeller& IrModeller);
	friend class ConvTest;
};

#endif
