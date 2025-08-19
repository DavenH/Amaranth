#pragma once

#include "TrilinearCube.h"
#include "MorphPosition.h"

#include "../Interpolator.h"
#include "../../Positioner/CyclicPointPositioner.h"

using std::vector;

class PathDeformingPositioner;

/**
 * @class TrilinearInterpolator
 * @brief Evaluates a morphable Mesh (Time/Red/Blue) and returns a 2D control-point
 *        slice (“Intercepts”) suitable for curve generation.
 *
 * ## Why
*
 * We ultimately want a waveform or spectrum filter that can smoothly transform
 * with three degrees of freedom, for gaining the dynamism and expressivity of real
 * instruments.
 *
 * This class is instrumental to granting those three degrees of freedom.
 *
 * ## How
 *
 *
 * Those three degrees are dimensions we call Red, Yellow, and Blue.
 * Given a Mesh built from TrilinearCube elements (each cell has 8 corner vertices),
 * this class computes the trilinear blend of those corners at a particular
 * MorphPosition { time (aka “yellow”), red, blue }.
 *
 * For every cube that contains the MorphPosition, we produce one Intercept:
 *   - x   := phase (wrapped or clamped)
 *   - y   := amplitude (optionally re-scaled: Unipolar/Bipolar/HalfBipolar)
 *   - shp := curve sharpness hint
 * The resulting vector<Intercept> is sorted by x and is ready for downstream
 * curve assembly/sampling.
 *
 * ## Coordinate system
 * - **Independent (morph) axes:** Time (“yellow”), Red, Blue
 * - **Dependent (output) axes:** Phase → x, Amplitude → y, Sharpness -> shp
 * - **z-dim (view axis):** whichever morph axis the UI is currently visualizing.
 *   This does not change the math; it only affects optional cross-section data
 *   exported for visualization.
 *
 * ## Trilinear evaluation (per cube)
 * 1) Interpolate along the four edges parallel to the chosen z-dim → 4 lerped points
 * 2) Interpolate those along the second morph axis → 2 points
 * 3) Interpolate those along the final morph axis → 1 blended point v
 * 4) Convert v to an Intercept: x = v.phase, y = v.amp, shp = v.curve
 *
 * ## Responsibilities and boundaries
 * - **In scope:** visiting cubes, trilinear blending, x wrapping/clamping,
 *   amplitude scaling, basic sorting of Intercepts.
 * - **Out of scope (handled elsewhere in the refactor):** endpoint padding,
 *   cyclic line unwrapping, path-based deformations, and other point
 *   repositioning. Those live in PointPositioner/PathDeformingPositioner and friends.
 *
 * ## Usage
 *   TrilinearInterpolator tri(pointPositioner);
 *   tri.setMorphPosition({ time, red, blue });
 *   auto points = tri.interpolate(mesh); // sorted vector<Intercept>
 *
 * Returns empty if the Mesh is null or has no cubes. Thread-safe if callers
 * do not mutate the Mesh concurrently.
 *
 * @see Mesh, TrilinearCube, MorphPosition, Intercept, PointPositioner
 */
class TrilinearInterpolator : public Interpolator<Mesh*, Intercept> {
public:
    explicit TrilinearInterpolator(PathDeformingPositioner* pathDeformer);
    ~TrilinearInterpolator() override = default;

    /**
     * @brief Compute control points (Intercepts) for the current MorphPosition.
     * @param mesh Mesh to evaluate; must outlive this call.
     * @return Sorted vector of Intercepts (by x/phase). Empty if mesh is null
     *         or has no cubes. Phases are wrapped to [0,1) when cyclic, otherwise
     *         clamped to [xMinimum, xMaximum].
     *
     * @note Transitional code paths may also compute mid-plane “depth” samples for
     *       visualization when enabled; these will migrate to dedicated components
     *       (PointPositioner/visualization) as part of the rasterization refactor.
     */
    vector<Intercept> interpolate(Mesh* mesh, const RasterizerParams& params) override;

    void setMorphPosition(const MorphPosition& position) { morph = position; }

    void setYellow(float yellow)   { morph.time = yellow;   }
    void setBlue(float blue)       { morph.blue = blue;     }
    virtual void setRed(float red) { morph.red  = red;      }

protected:
    MorphPosition morph;
    TrilinearCube::ReductionData reduct;

    PathDeformingPositioner* pathDeformer;
    CyclicPointPositioner cyclicPositioner;
};