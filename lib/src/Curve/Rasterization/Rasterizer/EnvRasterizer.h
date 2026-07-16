#pragma once

#include <climits>
#include <vector>

#include <App/MeshLibrary.h>
#include <Array/ScopedAlloc.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>
#include <Curve/Rasterization/EnvelopePlaybackState.h>
#include <Curve/Rasterization/Policies/Envelope/EnvelopePolicies.h>
#include <Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h>
#include <Curve/Rasterization/RasterizationRequest.h>
#include <Curve/Rasterization/RenderResult.h>
#include <Curve/Rasterization/Sampling/GuideCurveSampler.h>

#include "BaseRasterizer.h"

using std::vector;

class GuideCurveProvider;

/**
 * Policies:
 *
 * - An env mesh's sustain line cannot be assigned to the last icpt
 * - If env mesh has a sustain line, this implies it has a release curve
 * - No sustain line means sustain on last icpt
 *
 */
class EnvRasterizer :
        public Rasterization::BaseRasterizer {
public:
    using GuideCurveContext = Rasterization::GuideCurveContext;

    enum { loopMinSizeIcpts = 1, graphicIndex = 0, headUnisonIndex };
    enum { NormalState, Looping, Releasing };

    using EnvParams = Rasterization::EnvelopeVoicePlaybackState;

    /* ----------------------------------------------------------------------------- */

    explicit EnvRasterizer(
            GuideCurveProvider* guideCurveProvider = nullptr,
            const String& name = String());
    EnvRasterizer& operator =(const EnvRasterizer& copy);
    EnvRasterizer(const EnvRasterizer& copy);
    ~EnvRasterizer() override;

    void ensureParamSize(int numUnisonVoices);
    void adoptPreparedData(const EnvRasterizer& source);
    void evaluateLoopSustainIndices();
    void getIndices(int& loopIdx, int& sustIdx) const;
    void resetGraphicParams();
    void setMesh(EnvelopeMesh* mesh);
    void setNoteOff();
    void setNoteOn();
    void setWantOneSamplePerCycle(bool does);
    void simulateStart(double& lastPosition);
    void validateState();

    bool hasReleaseCurve();
    bool renderToBuffer(
            int numSamples,
            double deltaX,
            int unisonIdx, const
            MeshLibrary::EnvProps& props,
            float tempoScale);
    bool simulateRender(
            double deltaX,
            double& lastPosition,
            const MeshLibrary::EnvProps& props,
            float tempoScale);
    bool simulateStop(double& lastPosition);

    Mesh* getCurrentMesh();
    const String& getName() const { return name; }

    void calcIntercepts();
    void cleanUp();
    void renderGeometryOnly(Mesh* mesh, float oscPhase = 0.f);
    void renderWaveformOnly(Mesh* mesh, float oscPhase = 0.f);
    void publishCurrentResult();
    void updateGeometry() override;
    void updateGeometry(Mesh* mesh, float oscPhase = 0.f);
    void updateWaveform() override;
    void updateWaveform(Mesh* mesh, float oscPhase = 0.f);
    void reset() { cleanUp(); }

    bool canRasterizeWaveform();

    Rasterization::SamplerView sampler() const override {
        const auto& active = samplingResult();
        return Rasterization::SamplerView(active.waveform, active.sampleable);
    }

    void setMesh(Mesh* mesh);

    MorphPosition& getMorphPosition() { return request.morph; }
    Rasterization::PointScalingMode getScalingType() const { return request.scalingMode; }
    void setCalcDepthDimensions(bool calc) { request.calcDepthDimensions = calc; }
    void setDecoupleComponentDfrm(bool does) { request.decoupleComponentDeforms = does; }
    void setDims(const Dimensions& dims) { request.dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) { guideCurveProvider = provider; }
    void setLimits(float min, float max) { request.xMinimum = min; request.xMaximum = max; }
    void setLowresCurves(bool areLow) { request.lowResCurves = areLow; }
    void setMorphPosition(const MorphPosition& morph) { request.morph = morph; }
    void setNoiseSeed(int seed) { request.noiseSeed = seed; }
    void setToOverrideDim(bool does) { request.overrideDimension = does; }
    void setWrapsEnds(bool wraps) {
        request.cyclic = wraps;
    }
    void update(UpdateType updateType) { Updateable::update(updateType); }
    void updateOffsetSeeds(int layerSize, int tableSize);
    void updateValue(int dim, float value);

    const EnvelopeMesh* getEnvMesh() const      { return envMesh;                           }
    const Rasterization::RenderResult& preparedResult() const { return result; }
    float getSustainLevel(int paramIndex) const { return playback.voice(paramIndex).sustainLevel; }
    int getMode() const {
        return playback.mode == Rasterization::EnvelopePlaybackMode::Looping
                ? Looping
                : playback.mode == Rasterization::EnvelopePlaybackMode::Releasing
                        ? Releasing
                        : NormalState;
    }
    void setMode(int mode) {
        playback.mode = mode == Looping
                ? Rasterization::EnvelopePlaybackMode::Looping
                : mode == Releasing
                        ? Rasterization::EnvelopePlaybackMode::Releasing
                        : Rasterization::EnvelopePlaybackMode::Normal;
    }
    bool wantsOneSamplePerCycle() const { return playback.oneSamplePerCycle; }
    Buffer<float> getRenderBuffer()             { return renderBuffer;                  }


private:
    void changedToRelease();
    void clearOutput();
    void clearRasterizationResult(bool clearCurves);
    Rasterization::EnvelopePaddingContext createPaddingContext() const;
    void installEnvelopeProviders();
    void preparePlaybackResults();
    void renderEnvelopeCrossPoints();
    void processEnvelopeIntercepts(vector<Intercept>& intercepts);
    void rebuildCurvesFromIntercepts();
    void bakeWaveform(Rasterization::RenderResult& target);
    Rasterization::GuideCurveApplier createGuideCurveApplier();

    bool canLoop() const;
    const Rasterization::RenderResult& samplingResult() const;
    bool isSampleable() const;
    bool isSampleableAt(float x) const;
    void markWaveformUnsampleable();
    float sampleAt(double angle);
    float sampleAt(double angle, int& currentIndex);
    float sampleAtDecoupled(double angle, GuideCurveContext& context);
    template<typename T>
    T sampleWithInterval(Buffer<float> buffer, T delta, T phase) {
        return Rasterization::WaveformSampler::sampleWithInterval(
                samplingResult().waveform,
                buffer,
                delta,
                phase);
    }

    int vectorizedRenderToBuffer(Buffer<float> buffer, int numSamples, double deltaX, int unisonIdx);
    float getLoopLength() const;

    /* ----------------------------------------------------------------------------- */

    int loopIndex, sustainIndex;

    MeshLibrary::EnvProps defaultProps;
    EnvelopeMesh* envMesh;

    Rasterization::EnvelopePlaybackState playback;

    ScopedAlloc<float> preallocated;
    Buffer<float> renderBuffer;

    GuideCurveProvider* guideCurveProvider;
    Rasterization::RasterizationRequest request;
    Rasterization::RenderResult result;
    Rasterization::RenderResult loopResult;
    Rasterization::GuideCurveOffsetSeeds guideCurveOffsetSeeds;
    VertCube::ReductionData reduction;

    String name;

    JUCE_LEAK_DETECTOR(EnvRasterizer)
};
