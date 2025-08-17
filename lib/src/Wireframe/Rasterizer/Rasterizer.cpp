#include "Rasterizer.h"

#include "Wireframe/Curve/CurveGenerator.h"
#include "Wireframe/Positioner/PointPositioner.h"
#include "Wireframe/Sampler/CurveSampler.h"

template<typename InputType, typename OutputPointType>
RasterizerData Rasterizer<InputType, OutputPointType>::runPipeline(InputType arg) {
    vector<Intercept> icpts = interpolator->interpolate(arg, config.interpolation);

    for (auto& pos: positioners) {
        pos->adjustControlPoints(icpts, config.curve);
    }

    vector<CurvePiece> pieces = generator->produceCurvePieces(icpts, config.generator);
    sampler->sampleToBuffer()
    return ;
}
