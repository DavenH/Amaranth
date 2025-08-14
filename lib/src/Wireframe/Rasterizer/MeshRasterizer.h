#pragma once
#include "../Positioner/PointPositioner.h"
#include "../Interpolator/MeshInterpolator.h"
#include "../Sampler/CurveSampler.h"

using std::unique_ptr;

/**
 * New thin facade which should be sufficient to accomplish any meshrasterizer behavior with no subclassing,
 * by composing different interpolator/positioner/sampler types.
 *
 * Note, it may be required for positioners to be an array, e.g. if CurvePathPositioner -> ChainingPointPositioner.
 * Maybe CurvePathPositioner should be always available?
 *
 * @tparam T
 */
template<typename T> class MeshRasterizer {
public:
    explicit MeshRasterizer(MeshInterpolator<T> interpolator);

private:
    unique_ptr<MeshInterpolator<T>> interpolator;
    vector<PointPositioner> positioners;
    unique_ptr<CurveSampler> sampler;
};
