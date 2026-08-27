#include <catch2/catch_test_macros.hpp>
#include "JuceHeader.h"
using namespace juce;
#include "../src/Curve/Mesh/Vertex2.h"

#include <fstream>

#include "../src/Algo/AutoModeller.h"
#include "../src/Curve/Curve.h"

#include <array>

using std::fstream;
using std::vector;

namespace {

struct CurveTableScope {
    CurveTableScope() { Curve::calcTable(); }
    ~CurveTableScope() { Curve::deleteTable(); }
};

}

TEST_CASE("Auto Modeller basic functionality", "[pitch][dsp]") {
    fstream fin;
    fin.open("impulse_points.txt", std::ios::in);

    vector<Vertex2> path;
    for (int i = 0; i < 1024 && fin.good(); ++i) {
        Vertex2 v;
        fin >> v.x >> v.y;
        path.push_back(v);
    }

    AutoModeller modeller;
    vector<Intercept> reducedPath = modeller.modelToPath(path, 2.0, true);
}

TEST_CASE("Auto Modeller exposes the shared buffer-to-intercepts path", "[pitch][dsp]") {
    CurveTableScope curveTable;
    std::array<float, 128> samples {};
    samples[32] = 0.8f;
    samples[64] = -0.4f;

    AutoModeller modeller;
    const auto points = modeller.modelToIntercepts(
            { samples.data(), (int) samples.size() },
            false,
            0.0625f,
            0.1f);

    REQUIRE(points.size() >= 2);
}
