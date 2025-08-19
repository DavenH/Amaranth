#pragma once

#include "../Curve/CurvePiece.h"
#include "../Rasterizer/RasterizerParams.h"
#include "../Vertex/Intercept.h"

#include <vector>
using std::vector;

// Abstract interface for generating CurvePiece segments from control points
class CurveGenerator {
public:
    virtual ~CurveGenerator() = default;

    virtual vector<CurvePiece> produceCurvePieces(
        const vector<Intercept>& controlPoints,
        const GeneratorParameters& params
    ) = 0;
};

