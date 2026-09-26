#pragma once

#include <cstdint>
#include <vector>

#include "JuceHeader.h"

#include "ScalarSurfaceMaterial.h"

struct ScalarSurfaceRendererDiagnostics {
    uint64_t drawCount {};
    uint64_t uploadCount {};
    uint64_t allocationCount {};
    bool capabilityAvailable {};
    bool gpuValidationAttempted {};
    bool gpuValidationPassed {};
    int gpuValidationMaximumError {};
};

class ScalarSurfaceUploadState {
public:
    bool needsUpload(const ScalarSurfaceRenderData& data) const;
    void markUploaded(const ScalarSurfaceRenderData& data);
    void clear();

private:
    const float* source {};
    uint64_t revision {};
    int columns {};
    int rows {};
    float valueScale {};
    float valueOffset {};
};

class GLScalarSurfaceRenderer {
public:
    GLScalarSurfaceRenderer() = default;

    bool draw(const ScalarSurfaceRenderData& data);
    void clearResources();

    const ScalarSurfaceRendererDiagnostics& getDiagnostics() const { return diagnostics; }

private:
    bool compileProgram();
    bool ensureScalarPaletteTexture();
    bool ensureTexture(const ScalarSurfaceRenderData& data);
    bool uploadTexture(const ScalarSurfaceRenderData& data);
    bool textureMatches(const ScalarSurfaceRenderData& data) const;
    void validateGpuParity();
    void setMaterialUniforms(const ScalarSurfaceRenderData& data) const;

    static unsigned int compileShader(unsigned int type, const char* source);

    ScalarSurfaceUploadState uploadState;
    std::vector<float> packedHeightScales;
    unsigned int program {};
    unsigned int texture {};
    unsigned int scalarPaletteTexture {};
    bool compileAttempted {};
    bool usingFloatTexture {};
    bool textureCapabilityFailed {};

    ScalarSurfaceRendererDiagnostics diagnostics;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLScalarSurfaceRenderer)
};
