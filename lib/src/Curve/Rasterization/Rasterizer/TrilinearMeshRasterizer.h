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
        const RasterizationRequest& getRequest() const { return request; }

        RenderResult& renderGeometry(const GeometryRenderCommand& command) {
            renderTrilinearGeometry(
                    command.mesh,
                    command.request,
                    command.oscillatorPhase);
            return output;
        }

        RenderResult& renderWaveform(const WaveformRenderCommand& command) {
            renderTrilinearWaveform(
                    command.mesh,
                    command.request,
                    command.oscillatorPhase);
            return output;
        }

        void publish(
                const RenderResult& result,
                const PublicationMetadata& metadata) {
            RasterizerSnapshotSource source = createSnapshotSource(result, metadata);
            publishSnapshot(source);
        }

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

        // Cycle v1 compatibility facade. These methods use stored request state;
        // update* renders and publishes, while render*Only never publishes.
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
            return createSnapshotSource(output, PublicationMetadata { request.cyclic });
        }

        RasterizerSnapshotSource createSnapshotSource(
                const RenderResult& result,
                const PublicationMetadata& metadata) const {
            RasterizerSnapshotSource source;
            source.intercepts = &result.intercepts;
            source.colorPoints = &result.colorPoints;
            source.curves = &result.curves;
            source.waveform = result.waveform;
            source.paddingSize = result.paddingSize;
            source.wrapsVertices = metadata.wrapsVertices;
            source.sampleable = result.sampleable;

            return source;
        }

        GuideCurveApplier createGuideCurveApplier(
                VertCube::ReductionData& reductionData,
                bool* needsResorting,
                const RasterizationRequest& renderRequest) const {
            GuideCurvePolicyContext context;
            context.guideCurveProvider = guideCurveProvider;
            context.reduction = &reductionData;
            context.scalingMode = renderRequest.scalingMode;
            context.cyclic = renderRequest.cyclic;
            context.needsResorting = needsResorting;
            context.noiseSeed = renderRequest.noiseSeed;
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

        void setLowResCurves(bool lowResolution) {
            request.lowResCurves = lowResolution;
        }

        void setPrimaryViewDimension(int dimension) {
            request.primaryViewDimension = dimension;
        }

        void setScalingMode(PointScalingMode mode) {
            request.scalingMode = mode;
        }

        void setXLimits(float minimum, float maximum) {
            request.xMinimum = minimum;
            request.xMaximum = maximum;
        }

        void setGuideCurveProvider(GuideCurveProvider* provider) {
            guideCurveProvider = provider;
        }

        void setIntegralSampling(bool does) {
            request.integralSampling = does;
        }

        void setInterpolateCurves(bool interpolate) {
            request.interpolateCurves = interpolate;
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

        void updateOffsetSeeds(
                int layerSize,
                int tableSize,
                GuideCurveSeed seed) {
            guideCurveOffsetSeeds.derive(layerSize, tableSize, seed);
        }

    protected:
        RasterizationRequest& compatibilityRequest() {
            return request;
        }

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

            renderTrilinearGeometry(*mesh, request, oscPhase);
        }

        void renderTrilinearGeometry(
                Mesh& renderMesh,
                const RasterizationRequest& renderRequest,
                float oscPhase) {
            if (renderMesh.getNumCubes() == 0) {
                clearTrilinearOutput();
                return;
            }

            bool needsResorting = false;

            GuideCurveApplier guideApplier = createGuideCurveApplier(
                    reduction,
                    &needsResorting,
                    renderRequest);
            meshSlicer.sliceMesh(
                    &renderMesh,
                    renderRequest,
                    oscPhase,
                    guideApplier,
                    output,
                    reduction);
        }

        void renderTrilinearWaveform(float oscPhase) {
            if (mesh == nullptr) {
                clearTrilinearOutput();
                return;
            }

            renderTrilinearWaveform(*mesh, request, oscPhase);
        }

        void renderTrilinearWaveform(
                Mesh& renderMesh,
                const RasterizationRequest& renderRequest,
                float oscPhase) {
            renderTrilinearGeometry(renderMesh, renderRequest, oscPhase);

            if (output.intercepts.empty()) {
                return;
            }

            waveformBuilder.renderResult(
                    output,
                    renderRequest,
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
