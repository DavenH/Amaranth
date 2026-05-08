#include <algorithm>

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
#include "../Util/CycleEnums.h"


VoiceMeshRasterizer::VoiceMeshRasterizer(SingletonRepo* repo) :
		SingletonAccessor(repo, "VoiceMeshRasterizer")
    ,   state(nullptr) {
	unsampleable = true;
	overrideDim = true;
	scalingType = Bipolar;
	calcDepthDims = false;

	oversamplingChanged();
}

void VoiceMeshRasterizer::calcCrossPointsChaining(float oscPhase) {
    if (mesh->getNumCubes() == 0 || state == nullptr) {
	    return;
    }

    if (state->callCount > 0) {
        std::swap(state->backIcpts, icpts);
		state->backIcpts.clear();
	}

    needsResorting = false;

    ::Rasterization::MeshCubeSource source(mesh);
    ::Rasterization::GuideCurvePolicyContext guideContext;
    guideContext.guideCurveProvider = guideCurveProvider;
    guideContext.reduction = &reduct;
    guideContext.scalingMode = ::Rasterization::pointScalingModeFromLegacy(scalingType);
    guideContext.cyclic = cyclic;
    guideContext.needsResorting = &needsResorting;
    guideContext.noiseSeed = noiseSeed;
    guideContext.phaseOffsetSeeds = phaseOffsetSeeds;
    guideContext.vertOffsetSeeds = vertOffsetSeeds;

    auto output = Cycle::Rasterization::VoiceRasterizerFacade().buildIntercepts(
            source,
            morph,
            state->advancement,
            oscPhase,
            [&guideContext](Intercept& intercept, const MorphPosition& position) {
                guideContext.noOffsetAtEnds = false;
                ::Rasterization::GuideCurvePolicy(guideContext).apply(intercept, position);
            },
            reduct);

    state->backIcpts = output.intercepts;

	// and set x = adjustedX
	restrictIntercepts(state->backIcpts);

    if (needsResorting) {
        std::sort(state->backIcpts.begin(), state->backIcpts.end());
        needsResorting = false;
    }

    if (state->backIcpts.size() < 2 || icpts.size() < 2) {
        waveX.nullify();
		waveY.nullify();
		state->callCount++;
		unsampleable = true;

		return;
	}

    if (state->callCount == 0) {
	    state->advancement = getRealConstant(MinLineLength) * 1.1f;
    }

    // the first call is just padding for curves
    if (state->callCount > 0) {
        paddingSize = Cycle::Rasterization::VoiceRasterizerFacade().buildChainedPadding(
                icpts,
                state->backIcpts,
                *state,
                curves,
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

	adjustDeformingSharpness();

	for(auto& curve : curves) {
		curve.recalculateCurve();
	}

	calcWaveform();

	unsampleable = false;
}

void VoiceMeshRasterizer::orphanOldVerts() {
    icpts.clear();
}
