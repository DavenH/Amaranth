#pragma once

#include <array>
#include "JuceHeader.h"

#include "ScalarSurfaceMaterial.h"

namespace ScalarSurfaceUniforms {

namespace gl = juce::gl;

inline void setColour(unsigned int program, const char* name, juce::Colour colour) {
    gl::glUniform3f(
            gl::glGetUniformLocation(program, name),
            colour.getFloatRed(),
            colour.getFloatGreen(),
            colour.getFloatBlue());
}

inline void setFloat(unsigned int program, const char* name, float value) {
    gl::glUniform1f(gl::glGetUniformLocation(program, name), value);
}

inline void setPalette(unsigned int program, const ScalarSurfaceMaterial& material) {
    setColour(program, "negativeAnchor", material.negativeAnchor);
    setColour(program, "neutralAnchor", material.neutralAnchor);
    setColour(program, "positiveAnchor", material.positiveAnchor);
    setColour(program, "negativePearlTint", material.negativePearlTint);
    setColour(program, "neutralPearlTint", material.neutralPearlTint);
    setColour(program, "positivePearlTint", material.positivePearlTint);

    std::array<float, ScalarSurfaceMaterial::signedPaletteStopCount * 3> palette;
    for (int index = 0; index < ScalarSurfaceMaterial::signedPaletteStopCount; ++index) {
        const juce::Colour colour = material.signedPaletteStops[(size_t) index];
        palette[(size_t) index * 3] = colour.getFloatRed();
        palette[(size_t) index * 3 + 1] = colour.getFloatGreen();
        palette[(size_t) index * 3 + 2] = colour.getFloatBlue();
    }
    gl::glUniform3fv(
            gl::glGetUniformLocation(program, "signedPalette[0]"),
            ScalarSurfaceMaterial::signedPaletteStopCount,
            palette.data());
    gl::glUniform1fv(
            gl::glGetUniformLocation(program, "signedPalettePositions[0]"),
            ScalarSurfaceMaterial::signedPaletteStopCount,
            material.signedPalettePositions.data());
}

inline void setSampling(
        unsigned int program,
        const ScalarSurfaceRenderData& data) {
    gl::glUniform2f(
            gl::glGetUniformLocation(program, "textureStep"),
            1.f / (float) data.rows,
            1.f / (float) data.columns);
    gl::glUniform2f(
            gl::glGetUniformLocation(program, "textureToDomainScale"),
            (float) data.rows / (float) juce::jmax(1, data.rows - 1),
            (float) data.columns / (float) juce::jmax(1, data.columns - 1));
}

inline void setLighting(
        unsigned int program,
        const ScalarSurfaceRenderData& data,
        const ScalarSurfaceMaterial& material) {
    gl::glUniform3f(
            gl::glGetUniformLocation(program, "lightDirection"),
            material.lightX,
            material.lightY,
            material.lightZ);
    const float aspect = !data.bounds.isEmpty()
            ? data.bounds.getWidth() / data.bounds.getHeight()
            : (float) juce::jmax(1, data.columns - 1) / (float) juce::jmax(1, data.rows - 1);
    setFloat(program, "surfaceAspectRatio", aspect);
    setFloat(program, "reliefScale", material.reliefScale);
    setFloat(program, "ambientStrength", material.ambientStrength);
    setFloat(program, "diffuseStrength", material.diffuseStrength);
    setFloat(program, "specularStrength", material.specularStrength);
    setFloat(program, "pearlTintStrength", material.pearlTintStrength);
    setFloat(program, "obscuranceStrength", material.obscuranceStrength);
    setFloat(program, "obscuranceScale", material.obscuranceScale);
    setFloat(program, "exposureStrength", material.exposureStrength);
    setFloat(program, "exposureScale", material.exposureScale);
    setFloat(program, "exposureBias", material.exposureBias);
    gl::glUniform4fv(
            gl::glGetUniformLocation(program, "hillshadeWeights"),
            1,
            material.hillshadeWeights.data());
    gl::glUniform3fv(
            gl::glGetUniformLocation(program, "obscuranceBiases"),
            1,
            material.obscuranceBiases.data());
}

}
