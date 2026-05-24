#pragma once

#include <JuceHeader.h>

#include <memory>

class Mesh;
class VertCube;

namespace CycleV2 {

class TrimeshMeshFactory {
public:
    static std::unique_ptr<Mesh> createDefaultMesh(const juce::String& name = "Cycle2Trimesh");
    static VertCube* addVoiceCube(
            Mesh& mesh,
            float lowPhase,
            float highPhase,
            float lowAmp,
            float highAmp,
            float sharpness);

private:
    static void configureVoiceCube(
            VertCube& cube,
            float lowPhase,
            float highPhase,
            float lowAmp,
            float highAmp,
            float sharpness);
};

}
