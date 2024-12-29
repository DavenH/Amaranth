#pragma once

#include <vector>
#include "../Array/ScopedAlloc.h"

using std::vector;

class PitchedSample;

class PitchTracker {
public:
    enum { AlgoAuto, AlgoYin, AlgoSwipe };

    struct FrequencyBin {
        float averagePeriod;
        float averageAperiodicity;
        int population;

        [[nodiscard]] bool contains(float value) const {
            float lower = (1 - 0.2f) * averagePeriod;
            float upper = (1 + 0.2f) * averagePeriod;
            return value >= lower && value <= upper;
        }

        bool operator<(const FrequencyBin& b) const {
            return population < b.population;
        }
    };

    struct TrackingData {
        int     minFrequency;
        int     maxFrequency;
        int     overlap;
        int     step;
        float   helperScale;
        float   octaveRatioThres;
        int     offsetSamples;
        bool    useOffsets;
        int     harmonic;

        void reset()
        {
            minFrequency    = 20;
            maxFrequency    = 2000;
            overlap         = 4;
            step            = 1728;
            helperScale     = 0.1f;
            octaveRatioThres = 1.3f;
            offsetSamples   = 0;
            useOffsets      = false;
            harmonic        = 0;
        }
    };

    struct Window {
        int index, size, paddedSignalSize;
        int erbStart, erbEnd, erbSize;
        int offsetSamples, lastOffset, overlapSamples;
        float erbOffset, optimalFreq;

        Buffer<float> hannWindow, lambda, spectFreqs;
    };

    /* ----------------------------------------------------------------------------- */

    PitchTracker();
    ~PitchTracker() = default;

    static float refineFrames(PitchedSample* sample, float averagePeriod);
    void reset();
    void swipe();
    void trackPitch();
    void yin();

    float getAveragePeriod();
    vector<FrequencyBin>& getFrequencyBins();

    float getConfidence() const             { return confidence; }
    PitchedSample* getSample()              { return sample;     }

    void setAlgo(int algo)                  { this->algo = algo; }
    void setSample(PitchedSample* wrapper)  { sample = wrapper;  }

private:
    struct StrengthColumn {
        float time;
        Buffer<float> column;
    };

    struct ContiguousRegion {
        explicit ContiguousRegion(int start) : startIdx(start), endIdx(0), population(1) {}
        bool operator<(const ContiguousRegion& cg) const { return population < cg.population; }

        int startIdx, endIdx, population;
    };

    /* ----------------------------------------------------------------------------- */

    void fillFrequencyBins();
    static void calcLambda(Window& window, const Buffer<float>& realErbIdx);
    static void getAdjacentValues(
        Buffer<float> buff, int index, float& y1, float& y2, float& y3);

    static void createKernels(
        vector<Buffer<float>>& kernels,
        ScopedAlloc<float>& kernelMemory,
        const Buffer<int>& kernelSizes,
        Buffer<float> erbFreqs,
        Buffer<float> pitchCandidates
    );

    static float erbsToHertz(float x);
    static float hertzToErbs(float x);
    static float logTwo(float x);

    static float interpIndexQuadratic(const Buffer<float>& norms, int troughIndex, int minlag);
    static float interpValueQuadratic(const Buffer<float>& norms, int troughIndex);

    int getTrough(Buffer<float> norms, int minlag);

    /* ----------------------------------------------------------------------------- */

    int algo;
    float confidence{}, aperiodicityThresh;

    CriticalSection paramLock;
    TrackingData data{};
    PitchedSample* sample{};
    vector<FrequencyBin> bins;
};