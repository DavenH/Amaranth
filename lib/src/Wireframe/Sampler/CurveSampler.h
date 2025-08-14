#pragma once
#include "../Curve/CurvePiece.h"

using std::vector;

class CurveSampler {
public:
    virtual ~CurveSampler() = default;

    virtual vector<CurvePiece> produceCurvePieces(const vector<Intercept>& controlPoints) = 0;
    [[nodiscard]] virtual bool isSampleable() const = 0;
    virtual float sampleAt(double position) = 0;
    virtual void sampleToBuffer(Buffer<float>& buffer, double delta) = 0;

    void setLowresCurves(bool areLow)       { lowResCurves  = areLow; }
    void setLimits(float min, float max)    { xMinimum = min; xMaximum = max;   }


private:
    bool lowResCurves{};
   float xMaximum{}, xMinimum{};
};

