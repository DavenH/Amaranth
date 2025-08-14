#pragma once

#include <Wireframe/Vertex/Intercept.h>

class CycleState
{
public:
    Intercept 			frontA, frontB, frontC, frontD, frontE;
    vector<Intercept> 	backIcpts;

    int 				callCount{};
    float 				advancement{};
    double 				spillover{};

    CycleState() {
        reset();
    }

    void reset() {
        callCount   = 0;
        advancement = 0.f;

        spillover = 0;

        frontE = Intercept(-0.35f, 0.f);
        frontD = Intercept(-0.25f, 0.f);
        frontC = Intercept(-0.15f, 0.f);
        frontB = Intercept(-0.1f, 0.f);
        frontA = Intercept(-0.05f, 0.f);

        backIcpts.clear();
    }
};
