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
    ,   state(nullptr) {
	setToOverrideDim(true);
	setScalingMode(Bipolar);
	setCalcDepthDimensions(false);

	oversamplingChanged();
    setUpdateCurvesProvider([this]() {
        Cycle::Rasterization::VoiceWaveformUpdatePolicy().update(
                createRasterizerRuntime(),
                [this]() {
                    cleanUp();
                },
                [this]() {
                    prepareCurvesForWaveform();
                },
                [this]() {
                    calcWaveform();
                });
    });
}

void VoiceMeshRasterizer::calcCrossPointsChaining(float oscPhase) {
    Cycle::Rasterization::VoiceRasterizationPipeline::Context context;
    context.mesh = getMesh();
    context.state = state;
    context.morph = getMorphPosition();
    context.runtime = createRasterizerRuntime();
    context.reductionData = &reduct;
    context.oscPhase = oscPhase;
    context.interceptPadding = interceptPadding;
    context.initialAdvancement = getRealConstant(MinLineLength) * 1.1f;

    Cycle::Rasterization::VoiceRasterizationPipeline().render(
            context,
            createGuideCurveApplier(),
            [this](std::vector<Intercept>& intercepts) {
                restrictIntercepts(intercepts);
            },
            [](::Rasterization::RasterizerRuntime runtime) {
                ::Rasterization::RasterizerCleanupPolicy::markWaveformUnsampleable(runtime);
            },
            [this]() {
                updateCurves();
            });
}

void VoiceMeshRasterizer::orphanOldVerts() {
    createRasterizerRuntime().clearIntercepts();
}
