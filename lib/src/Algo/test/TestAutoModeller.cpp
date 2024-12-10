#include <catch2/catch_test_macros.hpp>
#include "../../Array/ScopedAlloc.h"
#include "../../Curve/Vertex2.h"

#include <fstream>

#include "../AutoModeller.h"

using std::fstream;
using std::vector;

TEST_CASE("PitchTracker basic functionality", "[pitch][dsp]") {
    fstream fin;
    fin.open("impulse_points.txt", std::ios::in);

    vector<Vertex2> path;
    for(int i = 0; i < 1024 && fin.good(); ++i)
    {
        Vertex2 v;
        fin >> v.x >> v.y;
        path.push_back(v);
    }

    AutoModeller modeller;
    vector<Intercept> reducedPath = modeller.modelToPath(path, 2.0, true);
}

