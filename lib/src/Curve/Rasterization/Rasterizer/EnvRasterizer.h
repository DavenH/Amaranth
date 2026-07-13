#pragma once

#include <climits>
#include <vector>

#include <App/MeshLibrary.h>
#include <Array/ScopedAlloc.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>
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

    // one per unison voice
    class EnvParams {
    public:
        EnvParams() : sustainLevel(1.f),
                      samplePosition(0.),
                      sampleIndex(0) {
        }

        void reset() {
            samplePosition = 0;
            sampleIndex = 0;
            guideCurveContext.currentIndex = 0;
        }

        int sampleIndex;
        float sustainLevel;
        double samplePosition;

        GuideCurveContext guideCurveContext;
    };

    /* ----------------------------------------------------------------------------- */

    explicit EnvRasterizer(
            GuideCurveProvider* guideCurveProvider = nullptr,
            const String& name = String());
    EnvRasterizer& operator =(const EnvRasterizer& copy);
    EnvRasterizer(const EnvRasterizer& copy);
    ~EnvRasterizer() override;

    void ensureParamSize(int numUnisonVoices);
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
    void updateGeometry() override;
    void updateGeometry(Mesh* mesh, float oscPhase = 0.f);
    void updateWaveform() override;
    void updateWaveform(Mesh* mesh, float oscPhase = 0.f);
    void reset() { cleanUp(); }

    bool canRasterizeWaveform();

    Rasterization::SamplerView sampler() const override {
        return Rasterization::SamplerView(result.waveform, !unsampleable);
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
        rasterizerData.wrapsVertices = wraps;
    }
    void update(UpdateType updateType) { Updateable::update(updateType); }
    void updateOffsetSeeds(int layerSize, int tableSize);
    void updateValue(int dim, float value);

    const EnvelopeMesh* getEnvMesh() const      { return envMesh;                           }
    float getSustainLevel(int paramIndex) const { return params[paramIndex].sustainLevel;   }
    int getMode() const                         { return state;                             }
    void setMode(int mode)                      { this->state = mode;                       }
    bool wantsOneSamplePerCycle() const         { return oneSamplePerCycle;                 }
    Buffer<float> getRenderBuffer()             { return renderBuffer;                  }


private:
    void changedToRelease();
    void clearRasterizationResult(bool clearCurves);
    Rasterization::EnvelopePaddingContext createPaddingContext() const;
    void installEnvelopeProviders();
    void padIcptsForRender(vector<Intercept>& icpts, vector<Curve>& curves);
    void renderEnvelopeCrossPoints();
    void processEnvelopeIntercepts(vector<Intercept>& intercepts);
    void rebuildCurvesFromIntercepts();
    void bakeWaveform();
    void copyWaveformForRelease();
    void publishSnapshot();
    void updateBuffers(int size);
    Rasterization::GuideCurveApplier createGuideCurveApplier();

    bool canLoop() const;
    bool isSampleable() const;
    bool isSampleableAt(float x) const;
    void markWaveformUnsampleable();
    float sampleAt(double angle);
    float sampleAt(double angle, int& currentIndex);
    float sampleAtDecoupled(double angle, GuideCurveContext& context);
    template<typename T>
    T sampleWithInterval(Buffer<float> buffer, T delta, T phase) {
        return Rasterization::WaveformSampler::sampleWithInterval(
                result.waveform,
                buffer,
                delta,
                phase);
    }

    int vectorizedRenderToBuffer(Buffer<float> buffer, int numSamples, double deltaX, int unisonIdx);
    float getLoopLength() const;

    /* ----------------------------------------------------------------------------- */

    bool sampleReleaseNextCall, oneSamplePerCycle;
    int loopIndex, sustainIndex, state;
    double releaseScale;

    MeshLibrary::EnvProps defaultProps;
    EnvelopeMesh* envMesh;

    vector<EnvParams> params;

    ScopedAlloc<float> preallocated;
    ScopedAlloc<float> waveformMemory;
    Buffer<float> waveXCopy, waveYCopy, slopeCopy, renderBuffer;

    GuideCurveProvider* guideCurveProvider;
    Rasterization::RasterizationRequest request;
    Rasterization::RenderResult result;
    Rasterization::GuideCurveOffsetSeeds guideCurveOffsetSeeds;
    VertCube::ReductionData reduction;

    int paddingSize;
    bool unsampleable, needsResorting;
    String name;

    JUCE_LEAK_DETECTOR(EnvRasterizer)
};
