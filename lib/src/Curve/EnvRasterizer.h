#pragma once

#include "MeshRasterizer.h"
#include "Rasterization/Policies/EnvelopePaddingPolicy.h"
#include "../App/MeshLibrary.h"

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

    void calcCrossPoints() { rasterizer.calcCrossPoints(); }
    void calcCrossPoints(Mesh* mesh, float oscPhase) { rasterizer.calcCrossPoints(mesh, oscPhase); }
    void calcIntercepts() { rasterizer.calcIntercepts(); }
    void cleanUp() override { rasterizer.cleanUp(); }
    void performUpdate(UpdateType updateType) override { rasterizer.performUpdate(updateType); }
    void reset() override { rasterizer.reset(); }
    void updateRasterizer(UpdateType updateType) override { rasterizer.updateRasterizer(updateType); }

    bool hasEnoughCubesForCrossSection() override { return rasterizer.hasEnoughCubesForCrossSection(); }
    bool isSampleable() override { return rasterizer.isSampleable(); }
    bool isSampleableAt(float x) override { return rasterizer.isSampleableAt(x); }
    bool wrapsVertices() const override { return rasterizer.wrapsVertices(); }

    float sampleAt(double angle) override { return rasterizer.sampleAt(angle); }
    float sampleAt(double angle, int& currentIndex) override { return rasterizer.sampleAt(angle, currentIndex); }
    float sampleAtDecoupled(double angle, GuideCurveContext& context) {
        return rasterizer.sampleAtDecoupled(angle, context);
    }
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override {
        return rasterizer.samplePerfectly(delta, buffer, phase);
    }

    template<typename T>
    T sampleWithInterval(Buffer<float> buffer, T delta, T phase) {
        return rasterizer.sampleWithInterval(buffer, delta, phase);
    }

    Mesh* getMesh() override { return rasterizer.getMesh(); }
    void setMesh(Mesh* mesh) override { rasterizer.setMesh(mesh); }
    GuideCurveProvider* getGuideCurveProvider() const override { return rasterizer.getGuideCurveProvider(); }
    int getPaddingSize() const override { return rasterizer.getPaddingSize(); }
    RasterizerData& getRasterizerData() override { return rasterizer.getRasterizerData(); }
    const RasterizerData& getRasterizerData() const override { return rasterizer.getRasterizerData(); }
    RasterizerData& getRastData() { return rasterizer.getRastData(); }

    const String& getName() override { return rasterizer.getName(); }
    MorphPosition& getMorphPosition() { return rasterizer.getMorphPosition(); }
    MeshRasterizer::ScalingType getScalingType() const { return rasterizer.getScalingType(); }
    void setCalcDepthDimensions(bool calc) { rasterizer.setCalcDepthDimensions(calc); }
    void setDecoupleComponentDfrm(bool does) { rasterizer.setDecoupleComponentDfrm(does); }
    void setDims(const Dimensions& dims) override { rasterizer.setDims(dims); }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { rasterizer.setGuideCurveProvider(provider); }
    void setLimits(float min, float max) { rasterizer.setLimits(min, max); }
    void setLowresCurves(bool areLow) { rasterizer.setLowresCurves(areLow); }
    void setMorphPosition(const MorphPosition& morph) { rasterizer.setMorphPosition(morph); }
    void setNoiseSeed(int seed) { rasterizer.setNoiseSeed(seed); }
    void setToOverrideDim(bool does) { rasterizer.setToOverrideDim(does); }
    void setWrapsEnds(bool wraps) { rasterizer.setWrapsEnds(wraps); }
    void update(UpdateType updateType) { Updateable::update(updateType); }
    void updateOffsetSeeds(int layerSize, int tableSize) { rasterizer.updateOffsetSeeds(layerSize, tableSize); }
    void updateValue(int dim, float value) { rasterizer.updateValue(dim, value); }

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
    ScopedAlloc<float> waveformMemory;
    Buffer<float> waveXCopy, waveYCopy, slopeCopy, renderBuffer;

    MeshRasterizer rasterizer;

    JUCE_LEAK_DETECTOR(EnvRasterizer)
};
