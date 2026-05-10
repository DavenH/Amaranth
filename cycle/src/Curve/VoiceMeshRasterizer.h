#pragma once

#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/Interfaces/RasterizerInterfaces.h>
#include <Curve/Rasterization/MeshWaveformRasterizer.h>
#include <Curve/Rasterization/RenderResult.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Curve/RasterizerData.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

class SingletonAccessor;
class CycleState;

class VoiceMeshRasterizer :
        public SingletonAccessor
    ,   public Rasterization::GuideCurveBindableRasterizer
    ,   public Rasterization::MeshBindableRasterizer
    ,   public Rasterization::RasterizerSampler
    ,   public Rasterization::RasterizerSnapshotProvider
    ,   public Rasterization::RasterizerUpdateTarget
    ,   public Rasterization::RasterizerVertexDomain {
private:
	JUCE_LEAK_DETECTOR(VoiceMeshRasterizer)

public:
	CycleState* state;

	explicit VoiceMeshRasterizer(SingletonRepo* repo);

	/*
	 * Chains the minimum necessary number of points from previous call to head of the subsequent call
	 * This provides 100% continuity between cycles
	 */
	void calcCrossPointsChaining(float phase);
	void orphanOldVerts();
	void setState(CycleState* state) { this->state = state; }

    void calcCrossPoints(Mesh* mesh, float oscPhase);
    void cleanUp() override;
    void performUpdate(UpdateType updateType) override;
    void reset() override { cleanUp(); }
    void updateRasterizer(UpdateType updateType) override { performUpdate(updateType); }

    bool doesCalcDepthDimensions() const { return rasterizer.getRequest().calcDepthDimensions; }
    bool doesIntegralSampling() const { return rasterizer.getRequest().integralSampling; }
    bool hasEnoughCubesForCrossSection() override;
    bool isSampleable() override;
    bool isSampleableAt(float x) override;
    bool wrapsVertices() const override { return rasterizer.getRequest().cyclic; }

    float sampleAt(double angle) override;
    float sampleAt(double angle, int& currentIndex) override;
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override;

    template<typename T>
    T sampleWithInterval(Buffer<float> buffer, T delta, T phase) {
        if (!chainedOutputActive) {
            return rasterizer.sampleWithInterval(buffer, delta, phase);
        }

        return Rasterization::WaveformSampler::sampleWithInterval(
                chainResult.waveform,
                buffer,
                delta,
                phase);
    }

    Mesh* getMesh() override { return mesh; }
    void setMesh(Mesh* mesh) override { this->mesh = mesh; }
    int getPaddingSize() const override;
    GuideCurveProvider* getGuideCurveProvider() const override { return rasterizer.getGuideCurveProvider(); }
    RasterizerData& getRasterizerData() override { return rasterizerData; }
    const RasterizerData& getRasterizerData() const override { return rasterizerData; }

    MorphPosition& getMorphPosition() { return rasterizer.getRequest().morph; }
    void setCalcDepthDimensions(bool calc) { rasterizer.getRequest().calcDepthDimensions = calc; }
    void setDims(const Dimensions& dims) override { rasterizer.getRequest().dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { rasterizer.setGuideCurveProvider(provider); }
    void setIntegralSampling(bool does) { rasterizer.getRequest().integralSampling = does; }
    void setInterceptPadding(float value) { rasterizer.getRequest().interceptPadding = value; }
    void setMorphPosition(const MorphPosition& morph) { rasterizer.getRequest().morph = morph; }
    void setNoiseSeed(int seed) { rasterizer.getRequest().noiseSeed = seed; }
    void setWrapsEnds(bool wraps) { rasterizer.getRequest().cyclic = wraps; }
    void updateOffsetSeeds(int layerSize, int tableSize) { rasterizer.updateOffsetSeeds(layerSize, tableSize); }

private:
    Rasterization::WaveformBuffers currentWaveform() const;
    void bakeChainedWaveform();
    void cleanChainedOutput();
    void markChainedWaveformUnsampleable();
    void publishSnapshot();
    void updateChainBuffers(int size);
    void restrictIntercepts(std::vector<Intercept>& intercepts);

    Mesh* mesh {};
    Rasterization::MeshWaveformRasterizer rasterizer;
    Rasterization::RenderResult chainResult;
    RasterizerData rasterizerData;
    VertCube::ReductionData chainReduction;

    int chainPaddingSize { 2 };
    bool chainUnsampleable { true };
    bool chainNeedsResorting {};
    bool chainedOutputActive {};
};
