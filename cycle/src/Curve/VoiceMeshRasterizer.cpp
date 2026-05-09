#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Curve/Intercept.h>
#include <Curve/Mesh.h>
#include <Curve/Rasterization/Policies/GuideCurvePolicy.h>
#include <Curve/Rasterization/Policies/RasterizerCleanupPolicy.h>
#include <Definitions.h>

#include "VoiceMeshRasterizer.h"
#include "CycleState.h"
#include "Rasterization/Pipelines/VoiceRasterizationPipeline.h"
#include "Rasterization/Policies/VoiceWaveformUpdatePolicy.h"
#include "../Util/CycleEnums.h"


VoiceMeshRasterizer::VoiceMeshRasterizer(SingletonRepo* repo) :
		SingletonAccessor(repo, "VoiceMeshRasterizer")
    ,   rasterizer("VoiceMeshRasterizer")
    ,   state(nullptr) {
	rasterizer.setToOverrideDim(true);
	rasterizer.setScalingMode(MeshRasterizer::Bipolar);
	rasterizer.setCalcDepthDimensions(false);

	rasterizer.oversamplingChanged();
    rasterizer.setUpdateCurvesProvider([this]() {
        Cycle::Rasterization::VoiceWaveformUpdatePolicy().update(
                rasterizer.createRasterizerRuntime(),
                [this]() {
                    cleanUp();
                },
                [this]() {
                    rasterizer.prepareLegacyCurvesForWaveform();
                },
                [this]() {
                    rasterizer.calcLegacyWaveform();
                });
    });
}

void VoiceMeshRasterizer::calcCrossPointsChaining(float oscPhase) {
    Cycle::Rasterization::VoiceRasterizationPipeline::Context context;
    context.mesh = rasterizer.getMesh();
    context.state = state;
    context.morph = rasterizer.getMorphPosition();
    context.runtime = rasterizer.createRasterizerRuntime();
    context.reductionData = &rasterizer.getReductionData();
    context.oscPhase = oscPhase;
    context.interceptPadding = rasterizer.getInterceptPadding();
    context.initialAdvancement = getRealConstant(MinLineLength) * 1.1f;

    Cycle::Rasterization::VoiceRasterizationPipeline().render(
            context,
            rasterizer.createLegacyGuideCurveApplier(),
            [this](std::vector<Intercept>& intercepts) {
                rasterizer.restrictIntercepts(intercepts);
            },
            [](::Rasterization::RasterizerRuntime runtime) {
                ::Rasterization::RasterizerCleanupPolicy::markWaveformUnsampleable(runtime);
            },
            [this]() {
                rasterizer.updateCurves();
            });
}

void VoiceMeshRasterizer::orphanOldVerts() {
    rasterizer.createRasterizerRuntime().clearIntercepts();
}
