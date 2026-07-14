#pragma once

#include <Curve/Mesh/Intercept.h>

#include <vector>

namespace Rasterization {

class VoiceCycleState {
public:
    VoiceCycleState() { reset(); }

    void reset() {
        callCount = 0;
        advancement = 0.f;
        spillover = 0.;

        frontE = Intercept(-0.35f, 0.f);
        frontD = Intercept(-0.25f, 0.f);
        frontC = Intercept(-0.15f, 0.f);
        frontB = Intercept(-0.1f, 0.f);
        frontA = Intercept(-0.05f, 0.f);
        backIcpts.clear();
    }

    Intercept frontA;
    Intercept frontB;
    Intercept frontC;
    Intercept frontD;
    Intercept frontE;
    std::vector<Intercept> backIcpts;

    int callCount {};
    float advancement {};
    double spillover {};
};

}
