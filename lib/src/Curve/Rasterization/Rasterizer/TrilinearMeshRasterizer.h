#pragma once

#include <vector>

#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Builders/CurveWaveformBuilder.h>
#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>

#include "BaseRasterizer.h"

namespace Rasterization {
    class TrilinearMeshRasterizer : public BaseRasterizer {
    public:
        RasterizationRequest& getRequest() { return request; }
        const RasterizationRequest& getRequest() const { return request; }

        SamplerView sampler() const override {
            return SamplerView(output.waveform, output.sampleable);
        }

        void updateGeometry() override {
            updateTrilinearGeometry(0.f);
        }

        void updateWaveform() override {
            updateTrilinearWaveform(0.f);
        }

        void updateGeometry(Mesh* mesh, float oscPhase = 0.f) {
            setMesh(mesh);
            updateTrilinearGeometry(oscPhase);
        }

        void updateWaveform(Mesh* mesh, float oscPhase = 0.f) {
            setMesh(mesh);
            updateTrilinearWaveform(oscPhase);
        }

        void render(Mesh* mesh, float oscPhase = 0.f) {
            updateWaveform(mesh, oscPhase);
        }

        void renderGeometryOnly(float oscPhase = 0.f) {
            renderTrilinearGeometry(oscPhase);
        }

        void renderGeometryOnly(Mesh* mesh, float oscPhase = 0.f) {
            setMesh(mesh);
            renderTrilinearGeometry(oscPhase);
        }

        void renderWaveformOnly(float oscPhase = 0.f) {
            renderTrilinearWaveform(oscPhase);
        }

        void renderWaveformOnly(Mesh* mesh, float oscPhase = 0.f) {
            setMesh(mesh);
            renderTrilinearWaveform(oscPhase);
        }

        void setMesh(Mesh* mesh) {
            this->mesh = mesh;
        }

        bool canRasterizeWaveform() const {
            return mesh != nullptr && mesh->hasEnoughCubesForCrossSection();
        }

        bool doesCalcDepthDimensions() const {
            return request.calcDepthDimensions;
        }

        bool doesIntegralSampling() const {
            return request.integralSampling;
        }

        bool isSampleable() const {
            return output.sampleable;
        }

        MorphPosition& getMorphPosition() {
            return request.morph;
        }

        GuideCurveProvider* getGuideCurveProvider() const {
            return guideCurveProvider;
        }

        int getPaddingSize() const {
            return output.paddingSize;
        }

        WaveformBuffers waveform() const {
            return output.waveform;
        }

        const RenderResult& result() const {
            return output;
        }

        RasterizerSnapshotSource createSnapshotSource() const {
            RasterizerSnapshotSource source;
            source.intercepts = &output.intercepts;
            source.colorPoints = &output.colorPoints;
            source.curves = &output.curves;
            source.waveform = output.waveform;
            source.paddingSize = getPaddingSize();
            source.wrapsVertices = request.cyclic;
            source.sampleable = output.sampleable;

            return source;
        }

        GuideCurveApplier createGuideCurveApplier(
                VertCube::ReductionData& reductionData,
                bool* needsResorting) const {
            GuideCurvePolicyContext context;
            context.guideCurveProvider = guideCurveProvider;
            context.reduction = &reductionData;
            context.scalingMode = request.scalingMode;
            context.cyclic = request.cyclic;
            context.needsResorting = needsResorting;
            context.noiseSeed = request.noiseSeed;
            context.offsetSeeds = &guideCurveOffsetSeeds;

            return GuideCurveApplier(context);
        }

        void setBlue(float blue) {
            request.morph.blue.setValueDirect(blue);
        }

        void setCalcDepthDimensions(bool calc) {
            request.calcDepthDimensions = calc;
        }

        void setDims(const Dimensions& dims) {
            request.dims = dims;
        }

        void setGuideCurveProvider(GuideCurveProvider* provider) {
            guideCurveProvider = provider;
        }

        void setIntegralSampling(bool does) {
            request.integralSampling = does;
        }

        void setInterceptPadding(float value) {
            request.interceptPadding = value;
        }

        void setMorphPosition(const MorphPosition& morph) {
            request.morph = morph;
        }

        void setNoiseSeed(int seed) {
            request.noiseSeed = seed;
        }

        void setRed(float red) {
            request.morph.red.setValueDirect(red);
        }

        void setWrapsEnds(bool wraps) {
            request.cyclic = wraps;
        }

        void setYellow(float yellow) {
            request.morph.time.setValueDirect(yellow);
        }

        void updateOffsetSeeds(int layerSize, int tableSize) {
            Random random(Time::currentTimeMillis());
            guideCurveOffsetSeeds.randomize(layerSize, tableSize, random);
        }

    protected:
        void cleanTrilinearRasterization() {
            clearTrilinearOutput();
            publishTrilinearSnapshot();
        }

        void publishTrilinearSnapshot() {
            publishSnapshot(createSnapshotSource());
        }

        void updateTrilinearGeometry(float oscPhase) {
            renderTrilinearGeometry(oscPhase);
            publishTrilinearSnapshot();
        }

        void updateTrilinearWaveform(float oscPhase) {
            renderTrilinearWaveform(oscPhase);
            publishTrilinearSnapshot();
        }

        Mesh* mesh {};

        void clearTrilinearOutput() {
            output.clear();
        }

        void renderTrilinearGeometry(float oscPhase) {
            if (mesh == nullptr || mesh->getNumCubes() == 0) {
                clearTrilinearOutput();
                return;
            }

            bool needsResorting = false;

            GuideCurveApplier guideApplier = createGuideCurveApplier(reduction, &needsResorting);
            meshSlicer.sliceMesh(
                    mesh,
                    request,
                    oscPhase,
                    guideApplier,
                    output,
                    reduction);
        }

        void renderTrilinearWaveform(float oscPhase) {
            renderTrilinearGeometry(oscPhase);

            if (output.intercepts.empty()) {
                return;
            }

            waveformBuilder.renderResult(
                    output,
                    request,
                    guideCurveProvider,
                    &guideCurveOffsetSeeds);
        }

    private:
        GuideCurveProvider* guideCurveProvider {};

        RasterizationRequest request;
        GuideCurveOffsetSeeds guideCurveOffsetSeeds;
        TrilinearMeshSlicer meshSlicer;
        CurveWaveformBuilder waveformBuilder;
        RenderResult output;
        VertCube::ReductionData reduction;
    };
}
