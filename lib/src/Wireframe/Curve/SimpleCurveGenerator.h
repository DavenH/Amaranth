#pragma once
#include "CurvePiece.h"
#include "CurveGenerator.h"
#include "Wireframe/State/RasterizerParameters.h"

using std::vector;

// Straightforward curve assembly: adjacent triplets become CurvePieces
class SimpleCurveGenerator : public CurveGenerator {
public:
    vector<CurvePiece> produceCurvePieces(const vector<Intercept>& controlPoints, const GeneratorParameters& config) override;

    static void applyScale(vector<Intercept>& points, ScalingType scaling);
    static void setResolutionIndices(vector<CurvePiece>& curves, float resScalingFactor, int paddingCount);
    static void adjustPathDeformerSharpness(vector<CurvePiece>& curves);

};
