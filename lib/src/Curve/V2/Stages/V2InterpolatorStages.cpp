#include "V2InterpolatorStages.h"

#include <algorithm>

#include "../../VertCube.h"

namespace {
enum class V2InterpolatorMode {
    TrilinearFast,
    TrilinearAccurate,
    SimplePoleBlend
};

float getPrimaryMorphValue(const MorphPosition& morph, int dimension) {
    switch (dimension) {
        case Vertex::Time:
            return morph.time;
        case Vertex::Red:
            return morph.red;
        case Vertex::Blue:
            return morph.blue;
        default:
            return morph.time;
    }
}

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
                int dim = context.primaryDimension;
                float independent = getPrimaryMorphValue(context.morph, dim);

                cube->getInterceptsFast(dim, reduction, context.morph);

                if (! reduction.pointOverlaps) {
                    continue;
                }

                VertCube::vertexAt(independent, dim, &reduction.v0, &reduction.v1, &reduction.v);
                appendInterceptFromVertex(reduction.v, cube, context.wrapPhases, outIntercepts);
                break;
            }

            case V2InterpolatorMode::TrilinearAccurate: {
                int dim = context.primaryDimension;
                float independent = getPrimaryMorphValue(context.morph, dim);

                cube->getInterceptsAccurate(dim, reduction, context.morph);

                if (! reduction.pointOverlaps) {
                    continue;
                }

                VertCube::vertexAt(independent, dim, &reduction.v0, &reduction.v1, &reduction.v);
                appendInterceptFromVertex(reduction.v, cube, context.wrapPhases, outIntercepts);
                break;
            }

            case V2InterpolatorMode::SimplePoleBlend: {
                int dim = context.primaryDimension;
                float independent = getPrimaryMorphValue(context.morph, dim);

                cube->getInterceptsFast(dim, reduction, context.morph);

                if (! reduction.pointOverlaps) {
                    continue;
                }

                float distA = independent - reduction.v0.values[dim];
                if (distA < 0.0f) {
                    distA = -distA;
                }

                float distB = independent - reduction.v1.values[dim];
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

bool V2FxVertexInterpolatorStage::run(
    const V2InterpolatorContext& context,
    std::vector<Intercept>& outIntercepts,
    int& outCount) noexcept {
    outIntercepts.clear();
    outCount = 0;

    if (context.mesh == nullptr) {
        return false;
    }

    const auto& verts = context.mesh->getVerts();
    outIntercepts.reserve(verts.size());

    for (auto* vert : verts) {
        if (vert == nullptr) {
            continue;
        }

        Vertex point = *vert;
        appendInterceptFromVertex(point, nullptr, context.wrapPhases, outIntercepts);
    }

    outCount = static_cast<int>(outIntercepts.size());
    return outCount > 0;
}
