#pragma once

#include <algorithm>
#include <vector>

#include <Curve/Intercept.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Rasterization/Sources/MeshCubeSource.h>
#include <Curve/VertCube.h>
#include <Obj/MorphPosition.h>

#include "../Policies/Voice/VoicePointPositionPolicy.h"

namespace Cycle::Rasterization {
    class VoiceSlicePipeline {
    public:
        struct Output {
            std::vector<Intercept> intercepts;
            bool sampleable {};
        };

        template<typename GuideApplier>
        Output render(
                const ::Rasterization::MeshCubeSource& source,
                const MorphPosition& morph,
                float advancement,
                float oscPhase,
                GuideApplier&& applyGuide,
                VertCube::ReductionData& reductionData) const {
            Output output;

            if (source.empty()) {
                return output;
            }

            float voiceTime = jmin(1.f, morph.time + advancement);

            for (int i = 0; i < source.size(); ++i) {
                appendCubeIntercept(
                        source.cubeAt(i),
                        morph,
                        voiceTime,
                        oscPhase,
                        applyGuide,
                        reductionData,
                        output.intercepts);
            }

            std::sort(output.intercepts.begin(), output.intercepts.end());
            output.sampleable = output.intercepts.size() >= 2;

            return output;
        }

    private:
        template<typename GuideApplier>
        void appendCubeIntercept(
                VertCube* cube,
                const MorphPosition& morph,
                float voiceTime,
                float oscPhase,
                GuideApplier&& applyGuide,
                VertCube::ReductionData& reductionData,
                std::vector<Intercept>& intercepts) const {
            slicer.slice(*cube, Vertex::Time, reductionData, MorphPosition(voiceTime, morph.red, morph.blue));

            Vertex* a = &reductionData.v0;
            Vertex* b = &reductionData.v1;
            Vertex* vertex = &reductionData.v;

            VoicePointPositionPolicy::Context pointContext;
            pointContext.voiceTime = voiceTime;
            pointContext.oscPhase = oscPhase;

            auto point = pointPositionPolicy.resolve(
                    pointContext,
                    reductionData.pointOverlaps,
                    a,
                    b,
                    vertex);

            if (!point.intersects) {
                return;
            }

            Intercept intercept(point.phase, 2.f * vertex->values[Vertex::Amp] - 1.f, cube, 0);
            intercept.shp = vertex->values[Vertex::Curve];
            intercept.adjustedX = intercept.x;

            applyGuide(intercept, morph);
            intercepts.push_back(intercept);
        }

        VoicePointPositionPolicy pointPositionPolicy;
        ::Rasterization::TrilinearMeshSlicer slicer;
    };
}
