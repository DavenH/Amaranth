#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Curve/Intercept.h>
#include <Curve/Mesh.h>
#include <Curve/Rasterization/Policies/GuideCurvePolicy.h>
#include <Curve/Rasterization/Sources/MeshCubeSource.h>
#include <Curve/VertCube.h>
#include <Obj/MorphPosition.h>
#include <Definitions.h>

#include "VoiceMeshRasterizer.h"
#include "CycleState.h"
#include "Rasterization/Policies/VoiceChainingPolicy.h"
#include "../Util/CycleEnums.h"


VoiceMeshRasterizer::VoiceMeshRasterizer(SingletonRepo* repo) :
		SingletonAccessor(repo, "VoiceMeshRasterizer")
    ,   state(nullptr) {
	setToOverrideDim(true);
	setScalingMode(Bipolar);
	setCalcDepthDimensions(false);

	oversamplingChanged();
}

void VoiceMeshRasterizer::calcCrossPointsChaining(float oscPhase) {
    Mesh* currentMesh = getMesh();
    if (currentMesh == nullptr || currentMesh->getNumCubes() == 0 || state == nullptr) {
	    return;
    }

    Cycle::Rasterization::VoiceChainingPolicy chainingPolicy(&needsResorting);
    chainingPolicy.beginCall(*state, icpts);

    ::Rasterization::MeshCubeSource source(currentMesh);
    ::Rasterization::GuideCurveApplier guideApplier = createGuideCurveApplier();

    auto output = Cycle::Rasterization::VoiceRasterizerFacade().buildIntercepts(
            source,
            getMorphPosition(),
            state->advancement,
            oscPhase,
            guideApplier,
            reduct);

    chainingPolicy.publishNextIntercepts(
            output,
            *state,
            [this](std::vector<Intercept>& intercepts) {
                restrictIntercepts(intercepts);
            });

    if (!chainingPolicy.canBuildChainedCurves(*state, icpts)) {
		state->callCount++;
		markWaveformUnsampleable();

		return;
	}

    if (state->callCount == 0) {
	    state->advancement = getRealConstant(MinLineLength) * 1.1f;
    }

    // the first call is just padding for curves
    if (state->callCount > 0) {
        Cycle::Rasterization::VoiceRasterizerFacade().buildChainedPadding(
                icpts,
                state->backIcpts,
                *state,
                createRasterizerRuntime(),
                interceptPadding);

		updateCurves();
	}

	++state->callCount;
}

void VoiceMeshRasterizer::updateCurves() {
    if (icpts.size() < 2) {
        cleanUp();

		return;
	}

    Cycle::Rasterization::VoiceRasterizerFacade().applyCurveResolution(curves);

    prepareCurvesForWaveform();
	calcWaveform();
}

void VoiceMeshRasterizer::orphanOldVerts() {
    icpts.clear();
}
