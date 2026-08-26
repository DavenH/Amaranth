#include "Nodes/Curve/Model/FlatCurveMeshState.h"

#include <algorithm>

namespace CycleV2 {

using namespace juce;

std::vector<FlatCurveVertexState> FlatCurveMeshState::parse(const String& serialized) {
    std::vector<FlatCurveVertexState> vertices;
    StringArray vertexTokens;
    vertexTokens.addTokens(serialized, ";", "");

    for (const auto& vertexToken : vertexTokens) {
        StringArray fields;
        fields.addTokens(vertexToken, ",", "");

        if (fields.size() != 3) {
            continue;
        }

        vertices.push_back({
                jlimit(0.f, 1.f, fields[0].getFloatValue()),
                jlimit(0.f, 1.f, fields[1].getFloatValue()),
                jlimit(0.f, 1.f, fields[2].getFloatValue())
        });
    }

    std::sort(vertices.begin(), vertices.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.x < rhs.x;
    });
    return vertices;
}

String FlatCurveMeshState::serialize(const std::vector<FlatCurveVertexState>& vertices) {
    StringArray tokens;

    for (const auto& vertex : vertices) {
        tokens.add(
                String(jlimit(0.f, 1.f, vertex.x), 6)
                + "," + String(jlimit(0.f, 1.f, vertex.y), 6)
                + "," + String(jlimit(0.f, 1.f, vertex.curve), 6));
    }

    return tokens.joinIntoString(";");
}

}
