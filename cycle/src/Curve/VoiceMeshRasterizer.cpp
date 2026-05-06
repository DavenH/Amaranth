#include <algorithm>

#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Curve/Intercept.h>
#include <Curve/Mesh.h>
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

	for(auto cube : mesh->getCubes()) {
        float voiceTime = jmin(1.f, morph.time + state->advancement);

		cube->getInterceptsFast(Vertex::Time, reduct, MorphPosition(voiceTime, morph.red, morph.blue));

		Vertex* a = &reduct.v0;
		Vertex* b = &reduct.v1;
		Vertex* vertex = &reduct.v;

        Cycle::Rasterization::VoicePointPositionPolicy::Context pointContext;
        pointContext.voiceTime = voiceTime;
        pointContext.oscPhase = oscPhase;

        auto point = Cycle::Rasterization::VoiceRasterizerFacade().resolvePoint(
                pointContext,
                reduct.pointOverlaps,
                a,
                b,
                vertex);

        if (point.intersects) {
            Intercept intercept(point.phase, 2.f * vertex->values[Vertex::Amp] - 1.f, cube, 0);
			intercept.shp = vertex->values[Vertex::Curve];
			intercept.adjustedX = intercept.x;

			applyGuideCurves(intercept, morph);

			state->backIcpts.push_back(intercept);
		}
	}

	std::sort(state->backIcpts.begin(), state->backIcpts.end());

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

	float base = 0.05f / float(Curve::resolution);
	setResolutionIndices(base);

	int lastIdx = curves.size() - 1;

	curves[0].resIndex = curves[1].resIndex;
	curves[lastIdx].resIndex = curves[lastIdx - 1].resIndex;

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
