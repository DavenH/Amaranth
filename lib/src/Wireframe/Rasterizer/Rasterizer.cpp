#include "Rasterizer.h"

#include "Wireframe/Generator/CurveGenerator.h"
#include "Wireframe/Positioner/PointPositioner.h"
#include "Wireframe/Sampler/CurveSampler.h"

template<typename InputType, typename OutputPointType>
SamplerOutput Rasterizer<InputType, OutputPointType>::runPipeline(InputType arg) {
    // Thought: these pure functions do a lot of copying. We could have a RasterizerData member, and
    // mutate the list references throughout the pipeline.
    // Pure functional is better for TDD of course...
    vector<Intercept> icpts = interpolator->interpolate(arg, config);

    for (auto& pos: positioners) {
        pos->adjustControlPoints(icpts, config.positionerParams);
    }

    // we could avoid copies, and also store curve state for later
    vector<CurvePiece> pieces = generator->produceCurvePieces(icpts, config.generator);

    // Store pieces? Sometimes we want to mutate just one curve, e.g. Automodeller.
    SamplerOutput output = sampler->buildFromCurves(pieces, config.sampling);

    return output;
}
