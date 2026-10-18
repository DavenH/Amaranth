#pragma once

#include <JuceHeader.h>
#include <Curve/Mesh/Vertex2.h>

#include <vector>

class EnvelopeMesh;

namespace CycleV2 {

enum class EnvelopePurpose : int;

class EnvelopeMeshState {
public:
    static void initialiseDefault(EnvelopeMesh& mesh);
    static void initialiseDefault(EnvelopeMesh& mesh, EnvelopePurpose purpose);
    static juce::String defaultSnapshot();
    static juce::String serialize(const EnvelopeMesh& mesh);
    static bool apply(const juce::String& serialized, EnvelopeMesh& mesh);
    static bool replacePointPath(EnvelopeMesh& mesh, const std::vector<Vertex2>& points);
    static std::vector<Vertex2> pointPath(const EnvelopeMesh& mesh);
};

}
