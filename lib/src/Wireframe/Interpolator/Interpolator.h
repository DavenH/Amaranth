#pragma once

#include "../Vertex/Intercept.h"
#include "../State/RasterizerParameters.h"

using std::vector;

/**
 *
 * The interpolator produces a 'slice' of a potentially multidimensional form.
 * The output is a vector of 2D vertices, which are called Intercepts.
 *
 * Subclasses:
 * - SimpleInterpolator: is just the identity function. A series of points in => same series out
 * - TrilinearInterpolator: this does a more complex operation to take repeated cross-sections
 *                          of a 5D form to produce the 2D output.
 *
 * @tparam InputType The type of object being interpolated.
 * @tparam OutputPointType The type of vertex class created as list.
 */
template<typename InputType, typename OutputPointType>
class Interpolator {
public:
    virtual ~Interpolator() = default;

    virtual vector<OutputPointType> interpolate(InputType arg, const InterpolatorParameters& params) = 0;
};
