#pragma once

#include <Algo/FFT.h>
#include <map>

#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <App/Settings.h>
#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

#include "../Updating/DynamicDetailUpdater.h"
#include "../Curve/GraphicRasterizer.h"
#include "../Curve/ScratchContext.h"

class EnvRasterizer;
class Spectrum3D;
class PhaseTrackingTest;
class Waveform3D;
class Unison;
class MeshLibrary;

using std::map;

class ResizeParams {
public:
    bool isEnvelope;
    int numColumns, overridePow2, overrideHarms, overrideKey;

    ScopedAlloc<Ipp32f> *timeArray, *freqArray, *phaseArray;
    vector<Column> *timeColumns, *freqColumns, *phaseColumns, *timeColumnsToCopy;

    ResizeParams(
            int numColumns,
            ScopedAlloc<Ipp32f>* timeArray, vector<Column>* timeColumns,
            ScopedAlloc<Ipp32f>* freqArray, vector<Column>* freqColumns,
            ScopedAlloc<Ipp32f>* phaseArray, vector<Column>* phaseColumns,
            vector<Column>* timeColumnsToCopyFrom) :
                timeArray(timeArray),
                freqArray(freqArray),
                phaseArray(phaseArray),
                timeColumns(timeColumns),
                freqColumns(freqColumns),
                phaseColumns(phaseColumns),
                timeColumnsToCopy(timeColumnsToCopyFrom),
                isEnvelope(false),
                numColumns(numColumns),
                overridePow2(-1),
                overrideKey(-1),
                overrideHarms(-1)
    {
    }

    void setExtraParams(int overrideLength, int overrideHarms, int overrideKey, bool isEnvelope) {
        this->overridePow2 	= overrideLength;
        this->overrideHarms = overrideHarms;
        this->overrideKey 	= overrideKey;
        this->isEnvelope 	= isEnvelope;
    }

};

class VisualDsp :
        public Timer
    ,	public SingletonAccessor {
    friend class PhaseTrackingTest;

public:
    enum EnvType 	{ VolumeType, ScratchType, ScratchPanelType, PitchType, PhaseType 	};
    enum StageType 	{ TimeStage, EnvStage, FFTStage, FXStage 							};
    enum ColType 	{ TimeColType, EnvColType, FreqColType, FXColType 					};

    class GraphicProcessor :
            public DynamicDetailUpdateable
        ,	public Updateable
        , 	public SingletonAccessor {
    public:
        Ref<VisualDsp> processor;

        GraphicProcessor(VisualDsp* processor, SingletonRepo* repo, StageType stage);
        void performUpdate(UpdateType updateType) override;

    private:
        StageType stage;
    };

    /* ----------------------------------------------------------------------------- */

    explicit VisualDsp(SingletonRepo* repo);
    ~VisualDsp() override;

    void init() override;
    void reset() override;
    void destroyArrays();
    void surfaceResized();
    void calcWaveSpectrogram(int numColumns);

    float getScratchPosition(int scratchChannel);
    int getTimeSamplingResolution();
    void getNumHarmonicsAndNextPower(int& numHarmonics, int& nextPow2, int key = -1);

    const vector<Column>& getFreqColumns();
    const vector<Column>& getTimeColumns();

    Buffer<float> getFreqColumn(float position, bool isMags);
    Buffer<float> getTimeArray();
    Buffer<float> getFreqArray();
    Buffer<float> getPanelSpeedEnv() { return scratchEnvPanel; }

    Updateable* getEnvProcessor()	{ return &envProcessor;  }
    Updateable* getFFTProcessor() 	{ return &fftProcessor;  }
    Updateable* getFXProcessor() 	{ return &fxProcessor; 	 }
    Updateable* getTimeProcessor() 	{ return &timeProcessor; }

    void rasterizeAllEnvs(int numColumns);
    void rasterizeEnv(int envEnum, int numColumns);

    void resizeArrays(int numColumns, int overrideLength, int overrideKey,
                      ScopedAlloc<Ipp32f>* timeArray, vector<Column>* timeColumns,
                      ScopedAlloc<Ipp32f>* freqArray, vector<Column>* freqColumns,
                      ScopedAlloc<Ipp32f>* phaseArray, vector<Column>* phaseColumn,
                      vector<Column>* timeColumnsToCopy);

    void resizeArrays(const ResizeParams& params);
    void rasterizeEnv(Buffer<Ipp32f> env, Buffer<Ipp32f> zoomArray,
                      int type, EnvRasterizer& rasterizer,
                      bool doRestore = true);

    CriticalSection& getCalculationLock();
    CriticalSection& getEnvelopeLock() { return graphicEnvLock; }
    CriticalSection& getColumnLock(int type);

    const ScratchContext& getScratchContext(int scratchChannel);

private:
    GraphicProcessor envProcessor, fftProcessor, fxProcessor, timeProcessor;

    void timerCallback() override;
    void calcTimeDomain(int numColumns);
    void calcSpectrogram(int numColumns);
    void processThroughEffects(int numColumns);
    void unwrapPhaseColumns(vector<Column>& phaseColumns);
    void copyArrayOrParts(const vector<Column>& srcColumns, vector<Column>& destColumns);

    void processFrequency(vector<Column>& columns, bool processUnison);
    void processThroughEnvelopes(int numColumns);
    void trackWavePhaseEnvelope();
    float getVoiceFrequencyCents(int unisonIndex);

    void checkFFTColumns 	 (int numColumns);
    void checkEffectsColumns (int numColumns);
    void checkTimeColumns	 (int numColumns);
    void checkEnvelopeColumns(int numColumns);

    void checkFFTWavColumns		(int numColumns, int numHarms, int overrideKey);
    void checkEffectsWavColumns	(int numColumns, int nextPow2, int numHarms, int overrideKey);
    void checkEnvWavColumns		(int numColumns, int nextPow2, int overrideKey);

    bool areAnyFXActive();
    void updateScratchContexts(int numColumns);

    /* ----------------------------------------------------------------------------- */

    Random random;
    ScratchContext defaultScratchContext;

    map<int, int> sizeToIndex;

    static const int numFFTOrders = 11;
    IppsFFTSpec_R_32f* fftspecs[numFFTOrders];

    Transform ffts[numFFTOrders];

    Ref<Unison> 	unison;
    Ref<Spectrum3D> spectrum3D;
    Ref<Waveform3D> surface;
    Ref<MeshLibrary> meshLib;

    Ref<TimeRasterizer>  timeRasterizer;
    Ref<SpectRasterizer> spectRasterizer;
    Ref<PhaseRasterizer> phaseRasterizer;

    ScopedAlloc<Ipp32f> zoomProgress;
    ScopedAlloc<Ipp32f> fftPreFXArray, fftPostFXArray;			// fft
    ScopedAlloc<Ipp32f> preEnvArray, postEnvArray, postFXArray;	// time
    ScopedAlloc<Ipp32f> phasePreFXArray, phasePostFXArray;		// phase
    ScopedAlloc<Ipp32f> volumeEnv, scratchEnv, pitchEnv, scratchEnvPanel;

    CriticalSection timeColumnLock, envColumnLock, fftColumnLock,
                    fxColumnLock, graphicEnvLock, dummyLock, calculationLock;

    vector<ScratchContext> scratchContexts;

    vector<Column> preEnvCols, postEnvCols, postFXCols;
    vector<Column> fftPreFXCols, fftPostFXCols;
    vector<Column> phasePreFXCols, phasePostFXCols;
};