#pragma once

#include "../../Array/Buffer.h"

/**
 * @class ICurvePath
 * @brief Allows us to make 'meta-curves' by storing a curve path in a pre-rendered table,
 * and reusing this shape as a 'CurvePiece' -- a stretch of the waveform between two
 * control points.
 *
 * ## Why
 *
 * We would use curve paths if:
 * 1. We often want to reuse shapes within a waveshape.
 * 2. We want to make a complex filter without the complexity of additional vertices added to a mesh
 * 3. We want to introduce raw noise to the waveform
 * 4. We want to introduce jitter to the control points
 *
 * ## How
 * The PathPanel is the UI element that allows us to create these static shapes, and
 * they are created in much the same way as other curves, except these curves are strictly 2D -- no morphing.
 *
 * But In addition to the basic shape defined by the control points, we can
 * adjust sliders for x-noise and y-noise.
 *
 * When the CurvePath is mapped to a TrilinearVertex's property, it then adds its output
 * to the existing value setting of that property. That's an easy way to add jitter
 * to a waveform - select a vertex, assign the phase property to a particular CurvePath.
 *
 * For applying a CurvePath to mutate a TrilinearVertex's properties, we use PathDeformingPositioner
 * For applying a CurvePath as a waveform piece, we use PathDeformingCurveSampler
 *
 * @see @class PathPanel
 * @see @class PathDeformingCurveSampler
 * @see @class PathDeformingPositioner
 **/
class ICurvePath {
public:
    virtual ~ICurvePath() = default;

    enum { tableSize = 0x2000 };

    struct NoiseContext {
        int noiseSeed;
        short vertOffset, phaseOffset;

        NoiseContext() : noiseSeed(0), vertOffset(0), phaseOffset(0) {}
    };

    virtual float getTableValue(int guideIndex, float progress, const NoiseContext& context) = 0;
    virtual void sampleDownAddNoise(int index, Buffer<float> dest, const NoiseContext& context) = 0;
    virtual Buffer<Float32> getTable(int index) = 0;
    virtual int getTableDensity(int index) = 0;
};