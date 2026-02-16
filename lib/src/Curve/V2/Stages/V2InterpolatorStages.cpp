#include "V2InterpolatorStages.h"

#include <algorithm>

#include "../../VertCube.h"

namespace {
enum class V2InterpolatorMode {
    TrilinearFast,
    TrilinearAccurate,
    SimplePoleBlend
};

void wrapPhase(float& phase) {
    while (phase >= 1.0f) {
        phase -= 1.0f;
    }

    while (phase < 0.0f) {
        phase += 1.0f;
    }
}

void appendInterceptFromVertex(
    const Vertex& vertex,
    VertCube* cube,
    bool wrapPhases,
    std::vector<Intercept>& outIntercepts) {
    float phase = vertex.values[Vertex::Phase];

    if (wrapPhases) {
        wrapPhase(phase);
    }

    Intercept intercept(phase, vertex.values[Vertex::Amp], cube);
    intercept.shp = vertex.values[Vertex::Curve];
    intercept.adjustedX = intercept.x;
    outIntercepts.emplace_back(intercept);
}

bool runInterpolator(
    const V2InterpolatorContext& context,
    std::vector<Intercept>& outIntercepts,
    int& outCount,
    V2InterpolatorMode mode) {
    outIntercepts.clear();
    outCount = 0;

    if (context.mesh == nullptr) {
        return false;
    }

    const auto& cubes = context.mesh->getCubes();
    outIntercepts.reserve(cubes.size());

    VertCube::ReductionData reduction;

    for (auto* cube : cubes) {
        if (cube == nullptr) {
            continue;
        }

        switch (mode) {
            case V2InterpolatorMode::TrilinearFast: {
                cube->getFinalIntercept(reduction, context.morph);

                if (! reduction.pointOverlaps) {
                    continue;
                }

                appendInterceptFromVertex(reduction.v, cube, context.wrapPhases, outIntercepts);
                break;
            }

            case V2InterpolatorMode::TrilinearAccurate: {
                cube->getInterceptsAccurate(Vertex::Time, reduction, context.morph);

                if (! reduction.pointOverlaps) {
                    continue;
                }

                VertCube::vertexAt(context.morph.time, Vertex::Time, &reduction.v0, &reduction.v1, &reduction.v);
                appendInterceptFromVertex(reduction.v, cube, context.wrapPhases, outIntercepts);
                break;
            }

            case V2InterpolatorMode::SimplePoleBlend: {
                cube->getInterceptsFast(Vertex::Time, reduction, context.morph);

                if (! reduction.pointOverlaps) {
                    continue;
                }

                float distA = context.morph.time - reduction.v0.values[Vertex::Time];
                if (distA < 0.0f) {
                    distA = -distA;
                }

                float distB = context.morph.time - reduction.v1.values[Vertex::Time];
                if (distB < 0.0f) {
                    distB = -distB;
                }
                const Vertex& chosen = (distA <= distB) ? reduction.v0 : reduction.v1;

                appendInterceptFromVertex(chosen, cube, context.wrapPhases, outIntercepts);
                break;
            }
        }
    }

    outCount = static_cast<int>(outIntercepts.size());
    return outCount > 0;
}
}

bool V2TrilinearInterpolatorStage::run(
    const V2InterpolatorContext& context,
    std::vector<Intercept>& outIntercepts,
    int& outCount) noexcept {
    return runInterpolator(context, outIntercepts, outCount, V2InterpolatorMode::TrilinearFast);
}

bool V2BilinearInterpolatorStage::run(
    const V2InterpolatorContext& context,
    std::vector<Intercept>& outIntercepts,
    int& outCount) noexcept {
    return runInterpolator(context, outIntercepts, outCount, V2InterpolatorMode::TrilinearAccurate);
}

bool V2SimpleInterpolatorStage::run(
    const V2InterpolatorContext& context,
    std::vector<Intercept>& outIntercepts,
    int& outCount) noexcept {
    return runInterpolator(context, outIntercepts, outCount, V2InterpolatorMode::SimplePoleBlend);
}
