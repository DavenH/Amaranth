#pragma once

#include <vector>

#include "Builders/RasterizerSnapshotBuilder.h"
#include "GuideCurveOffsetSeeds.h"
#include "Interpolation/TrilinearMeshSlicer.h"
#include "Builders/CurveWaveformBuilder.h"
#include "Policies/Mesh/GuideCurvePolicy.h"
#include "Sampling/WaveformSampler.h"

class GuideCurveProvider;
class Mesh;

namespace Rasterization {
    class MeshWaveformRasterizer {
    public:
        RasterizationRequest& getRequest() { return request; }
        const RasterizationRequest& getRequest() const { return request; }

        void setGuideCurveProvider(GuideCurveProvider* provider) {
            guideCurveProvider = provider;
        }

        GuideCurveProvider* getGuideCurveProvider() const {
            return guideCurveProvider;
        }

        void updateOffsetSeeds(int layerSize, int tableSize) {
            Random random(Time::currentTimeMillis());
            guideCurveOffsetSeeds.randomize(layerSize, tableSize, random);
        }

        void render(Mesh* mesh, float oscPhase = 0.f) {
            if (mesh == nullptr || mesh->getNumCubes() == 0) {
                clean();
                return;
            }

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
            waveformOutput = &waveformBuilder.renderIntercepts(
                    meshIntercepts,
                    waveformResult,
                    request,
                    guideCurveProvider,
                    &guideCurveOffsetSeeds);
        }

        void clean() {
            meshIntercepts.clear();
            meshColorPoints.clear();
            waveformOutput = nullptr;
        }

        bool isSampleable() const {
            return waveformOutput != nullptr && waveformOutput->sampleable;
        }

        SamplerView samplerView() const {
            return SamplerView(waveform(), isSampleable());
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
