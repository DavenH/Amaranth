#include "SimpleCurveGenerator.h"

#include <algorithm>

using std::vector;

std::vector<CurvePiece> SimpleCurveGenerator::produceCurvePieces(
    const std::vector<Intercept>& controlPoints,
    const GeneratorParameters& config
) {
    vector<Intercept> cps = controlPoints;

    // cannot generate CurvePieces unless there are at least 3 control points.
    // If there are just 2 defined points, Padding/Chaining point positioner
    // should have added enough control points either side to make this >= 3
    if (cps.size() < 3) {
        return {};
    }

    std::sort(cps.begin(), cps.end());
    applyScale(cps, config.scaling);

    vector<CurvePiece> pieces;
    pieces.reserve(jmax(1ul, cps.size() - 2));

    for (size_t i = 0; i + 2 < cps.size(); ++i) {
        pieces.emplace_back(cps[i + 0], cps[i + 1], cps[i + 2]);
    }

    if (config.lowResolution && pieces.size() > 8) {
        for (auto & curve : pieces) {
            curve.resIndex = CurvePiece::resolutions - 1;
            curve.setShouldInterpolate(false);
        }
    } else {
        for(auto & piece : pieces) {
            piece.setShouldInterpolate(! config.lowResolution && config.interpolateCurves);
        }

        float baseFactor = config.lowResolution ? 0.4f : config.integralSampling ? 0.05f : 0.1f;
        float resolutionScalingFactor = baseFactor / float(CurvePiece::resolution);

        setResolutionIndices(pieces, resolutionScalingFactor, config.paddingCount);
    }

    adjustPathDeformerSharpness(pieces);

    return pieces;
}

void SimpleCurveGenerator::applyScale(vector<Intercept>& points, ScalingType scalingType) {
    for (auto intercept: points) {
        jassert(! isnanf(intercept.y));
        jassert(! isnanf(intercept.x));

        // can be NaN, short circuit here so it doesn't propagate
        if(isnanf(intercept.y)) {
            intercept.y = 0.5f;
        }

        switch (scalingType) {
            case Unipolar:      break;
            case Bipolar:       intercept.y = 2 * intercept.y - 1;  break;
            case HalfBipolar:   intercept.y -= 0.5f;                break;
            default: break;
        }
    }
}

void SimpleCurveGenerator::setResolutionIndices(vector<CurvePiece>& curves, float resScalingFactor, int paddingCount) {
    float dx;
    int res;
    size_t lastIdx = curves.size() - 1;

    for (int i = 1; i < curves.size() - 1; ++i) {
        dx = (curves[i + 1].c.x - curves[i - 1].a.x);

        for (int j = 0; j < CurvePiece::resolutions; ++j) {
            res = CurvePiece::resolution >> j;

            if (dx < resScalingFactor * static_cast<float>(res)) {
                curves[i].resIndex = j;
            }
        }
    }

    curves.front().resIndex = curves[lastIdx - 2 * (paddingCount - 1)].resIndex;
    curves.back().resIndex = curves[2 * paddingCount - 1].resIndex;
}

/*
 * Set curve after an amp-vs-phase path to full sharpness
 * so that waveX is continuous. At < 1 sharpness, the trailing
 * x-values of the curve create a discontinuity
 */
void SimpleCurveGenerator::adjustPathDeformerSharpness(vector<CurvePiece>& curves) {
    for (int i = 0; i < (int) curves.size(); ++i) {
        CurvePiece& curve = curves[i];

        if (i < (int) curves.size() - 1 && curve.b.cube != nullptr) {
            if (curve.b.cube->getCompPath() >= 0) {
                CurvePiece& next = curves[i + 1];

                if (next.b.cube == nullptr || next.b.cube->getCompPath() < 0) {
                    curve.c.shp = 1;
                    next.b.shp = 1;
                    next.updateCurrentIndex();

                    if (i < (int) curves.size() - 2) {
                        curves[i + 2].a.shp = 1;
                    }
                }
            }
        }
    }
}