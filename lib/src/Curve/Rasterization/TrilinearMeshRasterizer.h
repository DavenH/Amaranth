#pragma once

#include <vector>

#include "BaseRasterizer.h"
#include "Builders/CurveWaveformBuilder.h"
#include "GuideCurveOffsetSeeds.h"
#include "Interpolation/TrilinearMeshSlicer.h"
#include "Policies/Mesh/GuideCurvePolicy.h"
#include "Sampling/WaveformSampler.h"
#include "../Mesh.h"

namespace Rasterization {
    class TrilinearMeshRasterizer : public BaseRasterizer {
    public:
        RasterizationRequest& getRequest() { return request; }
        const RasterizationRequest& getRequest() const { return request; }

        SamplerView samplerView() const override {
            return SamplerView(waveform(), isSampleable());
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
            return waveformOutput != nullptr && waveformOutput->sampleable;
        }

        MorphPosition& getMorphPosition() {
            return request.morph;
        }

        GuideCurveProvider* getGuideCurveProvider() const {
            return guideCurveProvider;
        }

        int getPaddingSize() const {
            return waveformOutput != nullptr ? waveformOutput->paddingSize : request.paddingSize;
        }

        WaveformBuffers waveform() const {
            return waveformOutput != nullptr ? waveformOutput->waveform : WaveformBuffers();
        }

        RasterizerSnapshotSource createSnapshotSource() const {
            RasterizerSnapshotSource source;
            source.intercepts = &meshIntercepts;
            source.colorPoints = &meshColorPoints;
            source.paddingSize = getPaddingSize();
            source.wrapsVertices = request.cyclic;

            if (waveformOutput != nullptr) {
                source.curves = &waveformOutput->curves;
                source.waveform = waveformOutput->waveform;
            }

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

        const RenderResult* getWaveformOutput() const {
            return waveformOutput;
        }

        const std::vector<Intercept>& getIntercepts() const {
            return meshIntercepts;
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
            rasterizerData.wrapsVertices = wraps;
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
            meshIntercepts.clear();
            meshColorPoints.clear();
            waveformOutput = nullptr;
        }

        void renderTrilinearGeometry(float oscPhase) {
            if (mesh == nullptr || mesh->getNumCubes() == 0) {
                clearTrilinearOutput();
                return;
            }

            waveformOutput = nullptr;
            bool needsResorting = false;

            GuideCurveApplier guideApplier = createGuideCurveApplier(reduction, &needsResorting);
            const RenderResult& meshOutput = meshSlicer.sliceMesh(
                    mesh,
                    request,
                    oscPhase,
                    guideApplier,
                    meshSliceResult,
                    reduction);

            meshIntercepts = meshOutput.intercepts;
            meshColorPoints = meshOutput.colorPoints;
        }

        void renderTrilinearWaveform(float oscPhase) {
            renderTrilinearGeometry(oscPhase);

            if (meshIntercepts.empty()) {
                return;
            }

            waveformOutput = &waveformBuilder.renderIntercepts(
                    meshIntercepts,
                    waveformResult,
                    request,
                    guideCurveProvider,
                    &guideCurveOffsetSeeds);
        }

    private:
        GuideCurveProvider* guideCurveProvider {};

        RasterizationRequest request;
        GuideCurveOffsetSeeds guideCurveOffsetSeeds;
        TrilinearMeshSlicer meshSlicer;
        RenderResult meshSliceResult;
        CurveWaveformBuilder waveformBuilder;
        RenderResult waveformResult;
        RenderResult const* waveformOutput {};

        std::vector<Intercept> meshIntercepts;
        std::vector<ColorPoint> meshColorPoints;
        VertCube::ReductionData reduction;
    };
}
