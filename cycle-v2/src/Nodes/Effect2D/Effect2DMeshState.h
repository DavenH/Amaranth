#pragma once

#include "../../Graph/NodeGraph.h"

#include <vector>

namespace CycleV2 {

struct Effect2DVertexState {
    float x {};
    float y {};
    float curve {};
};

class Effect2DMeshState {
public:
    static String parameterId();
    static std::vector<Effect2DVertexState> parse(const String& serialized);
    static String serialize(const std::vector<Effect2DVertexState>& vertices);
    static std::vector<Effect2DVertexState> fromParameters(const std::vector<NodeParameter>& parameters);
};

}
