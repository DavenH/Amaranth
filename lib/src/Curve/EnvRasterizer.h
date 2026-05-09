#pragma once

#include <climits>
#include <vector>

#include "../Array/ScopedAlloc.h"
#include "../Design/Updating/Updateable.h"
#include "EnvelopeMesh.h"
#include "Rasterization/Builders/RasterizerSnapshotBuilder.h"
#include "Rasterization/GuideCurveOffsetSeeds.h"
#include "Rasterization/Interfaces/GuideCurveBindableRasterizer.h"
#include "Rasterization/Interfaces/MeshBindableRasterizer.h"
#include "Rasterization/Interfaces/RasterizerSampler.h"
#include "Rasterization/Interfaces/RasterizerSnapshotProvider.h"
#include "Rasterization/Interfaces/RasterizerUpdateTarget.h"
#include "Rasterization/Interfaces/RasterizerVertexDomain.h"
#include "Rasterization/Policies/Envelope/EnvelopePaddingPolicy.h"
#include "Rasterization/Policies/Mesh/GuideCurvePolicy.h"
#include "Rasterization/RasterizationRequest.h"
#include "Rasterization/RasterizerRuntime.h"
#include "Rasterization/Sampling/GuideCurveSampler.h"
#include "Rasterization/State/RasterizerStorage.h"
#include "../App/MeshLibrary.h"

using std::vector;

class GuideCurveProvider;
class SingletonRepo;

/**
 * Policies:
 *
 * - An env mesh's sustain line cannot be assigned to the last icpt
 * - If env mesh has a sustain line, this implies it has a release curve
 * - No sustain line means sustain on last icpt
 *
 */
class EnvRasterizer :
        public Updateable
    ,   public Rasterization::GuideCurveBindableRasterizer
    ,   public Rasterization::MeshBindableRasterizer
    ,   public Rasterization::RasterizerSampler
    ,   public Rasterization::RasterizerSnapshotProvider
    ,   public Rasterization::RasterizerUpdateTarget
    ,   public Rasterization::RasterizerVertexDomain
    ,   public SingletonAccessor {
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

    explicit EnvRasterizer(SingletonRepo* repo, GuideCurveProvider* guideCurveProvider, const String& name = String());
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

    void calcCrossPoints();
    void calcCrossPoints(Mesh* mesh, float oscPhase);
    void calcIntercepts();
    void cleanUp() override;
    void performUpdate(UpdateType updateType) override;
    void reset() override { cleanUp(); }
    void updateRasterizer(UpdateType updateType) override { update(updateType); }

    bool hasEnoughCubesForCrossSection() override;
    bool isSampleable() override;
    bool isSampleableAt(float x) override;
    bool wrapsVertices() const override { return request.cyclic; }

    float sampleAt(double angle) override;
    float sampleAt(double angle, int& currentIndex) override;
    float sampleAtDecoupled(double angle, GuideCurveContext& context);
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override;

    template<typename T>
    T sampleWithInterval(Buffer<float> buffer, T delta, T phase) {
        return Rasterization::WaveformSampler::sampleWithInterval(
                storage.waveform.waveform,
                buffer,
                delta,
                phase);
    }

    Mesh* getMesh() override { return envMesh; }
    void setMesh(Mesh* mesh) override;
    GuideCurveProvider* getGuideCurveProvider() const override { return guideCurveProvider; }
    int getPaddingSize() const override { return paddingSize; }
    RasterizerData& getRasterizerData() override { return rasterizerData; }
    const RasterizerData& getRasterizerData() const override { return rasterizerData; }
    RasterizerData& getRastData() { return rasterizerData; }

    MorphPosition& getMorphPosition() { return request.morph; }
    Rasterization::PointScalingMode getScalingType() const { return request.scalingMode; }
    void setCalcDepthDimensions(bool calc) { request.calcDepthDimensions = calc; }
    void setDecoupleComponentDfrm(bool does) { request.decoupleComponentDeforms = does; }
    void setDims(const Dimensions& dims) override { request.dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { guideCurveProvider = provider; }
    void setLimits(float min, float max) { request.xMinimum = min; request.xMaximum = max; }
    void setLowresCurves(bool areLow) { request.lowResCurves = areLow; }
    void setMorphPosition(const MorphPosition& morph) { request.morph = morph; }
    void setNoiseSeed(int seed) { request.noiseSeed = seed; }
    void setToOverrideDim(bool does) { request.overrideDimension = does; }
    void setWrapsEnds(bool wraps) { request.cyclic = wraps; }
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
    Rasterization::RasterizerRuntime runtime();
    Rasterization::RasterizerRuntime runtime() const;
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
    ScopedAlloc<float> rasterizerMemory;
    ScopedAlloc<float> waveformMemory;
    Buffer<float> waveXCopy, waveYCopy, slopeCopy, renderBuffer;

    GuideCurveProvider* guideCurveProvider;
    Rasterization::RasterizationRequest request;
    Rasterization::RasterizerStorage storage;
    Rasterization::GuideCurveOffsetSeeds guideCurveOffsetSeeds;
    RasterizerData rasterizerData;
    VertCube::ReductionData reduction;

    int paddingSize;
    bool unsampleable, needsResorting;

    JUCE_LEAK_DETECTOR(EnvRasterizer)
};
