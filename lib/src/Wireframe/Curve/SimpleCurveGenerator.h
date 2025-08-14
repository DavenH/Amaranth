#pragma once
#include "CurvePiece.h"

class SimpleCurveGenerator {
    std::vector<CurvePiece> produceCurvePieces(const std::vector<Intercept>& controlPoints);
};
