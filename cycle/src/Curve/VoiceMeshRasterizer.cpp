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


VoiceMeshRasterizer::VoiceMeshRasterizer(SingletonRepo* repo) : SingletonAccessor(repo, "VoiceMeshRasterizer"), state(nullptr) {
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

        // the points contain x
		// todo wtf does 'the point contain x' mean. Overlaps all what?
        if (reduct.pointOverlaps) {
            jassert(a->values[Vertex::Phase] >= 0);

            // rules are that both cannot be > 1, and both are always positive
            if (a->values[Vertex::Phase] > 1 && b->values[Vertex::Phase] > 1) {
                a->values[Vertex::Phase] -= 1;
                b->values[Vertex::Phase] -= 1;
            }

            if ((a->values[Vertex::Phase] > 1) != (b->values[Vertex::Phase] > 1)) {
                float icpt = a->values[Vertex::Time]
                             + (1 - a->values[Vertex::Phase])
                               / ((a->values[Vertex::Phase] - b->values[Vertex::Phase])
                                  / (a->values[Vertex::Time] - b->values[Vertex::Time]));
                if (icpt > voiceTime) {
                    a->values[Vertex::Phase] -= 1;
					b->values[Vertex::Phase] -= 1;
				}
			}

            VertCube::vertexAt(voiceTime, Vertex::Time, a, b, vertex);

            float phase = vertex->values[Vertex::Phase] + oscPhase;

            while (phase >= 1) {
	            phase -= 1;
            }
            while (phase < 0) {
	            phase += 1;
            }

            jassert(phase >= 0 && phase < 1);

            Intercept intercept(phase, 2.f * vertex->values[Vertex::Amp] - 1.f, cube, 0);
			intercept.shp = vertex->values[Vertex::Curve];
			intercept.adjustedX = intercept.x;

			applyDeformers(intercept, morph);

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
	    state->advancement = getConstant(MinLineLength) * 1.1f;
    }

    // the first call is just padding for curves
    if (state->callCount > 0) {
        paddingSize = 2;

        int end = icpts.size() - 1;

		Intercept back1, back2, back3, back4, back5;
		back1.assignWithTranslation(state->backIcpts[0], 1);
		back2.assignWithTranslation(state->backIcpts[1], 1);
		back3.assignBack(state->backIcpts, 3);
		back4.assignBack(state->backIcpts, 4);
		back5.assignBack(state->backIcpts, 5);

		bool extraPadFront 	= state->frontB.x > -4 * interceptPadding;
		bool padFront 		= state->frontA.x > -4 * interceptPadding;
		bool padBack 		= back1.x < 1 + 4 * interceptPadding;
		bool extraPadBack 	= back2.x < 1 + 4 * interceptPadding;

		curves.clear();
		curves.reserve(icpts.size() + 5 + int(extraPadFront) + int(padFront) + int(padBack) + int(extraPadBack));

		if(extraPadFront) {
			curves.emplace_back(state->frontE, state->frontD, state->frontC);
		}

		if(padFront) {
			curves.emplace_back(state->frontD, state->frontC, state->frontB);
		}

		curves.emplace_back(state->frontC, state->frontB, state->frontA);
		curves.emplace_back(state->frontB, state->frontA, icpts[0]);
		curves.emplace_back(state->frontA, icpts[0], icpts[1]);

		for(int i = 0; i < (int) icpts.size() - 2; ++i) {
			curves.emplace_back(icpts[i], icpts[i + 1], icpts[i + 2]);
		}

		curves.emplace_back(icpts[end - 1], icpts[end], back1);
		curves.emplace_back(icpts[end], back1, back2);
		curves.emplace_back(back1, back2, back3);

		if(padBack) {
			curves.emplace_back(back2, back3, back4);
		}

		if(extraPadBack) {
			curves.emplace_back(back3, back4, back5);
		}

		// update front
		{
			end = icpts.size() - 1;

			// policy: icpt size is guaranteed to be >= 2 at this point
			state->frontE.assignFront(icpts, 5);
			state->frontD.assignFront(icpts, 4);
			state->frontC.assignFront(icpts, 3);
			state->frontB.assignWithTranslation(icpts[end - 1], -1);
			state->frontA.assignWithTranslation(icpts[end], 	-1);
		}

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
