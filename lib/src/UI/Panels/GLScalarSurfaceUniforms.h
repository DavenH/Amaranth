#pragma once

#include <array>
#include <cmath>

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

inline int pixelRadius(float radius, int dimension) {
    const int span = juce::jmax(1, dimension - 1);
    return juce::jmax(1, juce::roundToInt(radius * (float) span));
}

inline void setSampling(
        unsigned int program,
        const ScalarSurfaceRenderData& data,
        const ScalarSurfaceMaterial& material) {
    const int smallRow = pixelRadius(material.normalSampleRadius, data.rows);
    const int smallColumn = pixelRadius(material.normalSampleRadius, data.columns);
    const int largeRow = pixelRadius(material.largeSampleRadius, data.rows);
    const int largeColumn = pixelRadius(material.largeSampleRadius, data.columns);
    gl::glUniform2f(
            gl::glGetUniformLocation(program, "smallSampleTextureOffset"),
            (float) smallRow / (float) data.rows,
            (float) smallColumn / (float) data.columns);
    gl::glUniform2f(
            gl::glGetUniformLocation(program, "smallSampleDomainStep"),
            (float) smallRow / (float) juce::jmax(1, data.rows - 1),
            (float) smallColumn / (float) juce::jmax(1, data.columns - 1));
    gl::glUniform2f(
            gl::glGetUniformLocation(program, "largeSampleTextureOffset"),
            (float) largeRow / (float) data.rows,
            (float) largeColumn / (float) data.columns);
}

inline void setShadows(
        unsigned int program,
        const ScalarSurfaceRenderData& data,
        const ScalarSurfaceMaterial& material) {
    const float lightLength = std::sqrt(
            material.lightX * material.lightX + material.lightY * material.lightY);
    const float lightX = lightLength > 0.f ? material.lightX / lightLength : 0.f;
    const float lightY = lightLength > 0.f ? material.lightY / lightLength : 0.f;
    const auto setOffset = [&](const char* name, float radius) {
        const float columnOffset = (float) juce::roundToInt(
                lightX * radius * (float) juce::jmax(1, data.columns - 1));
        const float rowOffset = (float) juce::roundToInt(
                lightY * radius * (float) juce::jmax(1, data.rows - 1));
        gl::glUniform2f(
                gl::glGetUniformLocation(program, name),
                rowOffset / (float) data.rows,
                columnOffset / (float) data.columns);
    };
    setOffset("shadowOffset1", material.normalSampleRadius * 2.f);
    setOffset("shadowOffset2", material.largeSampleRadius);
    setOffset("shadowOffset3", material.largeSampleRadius * 2.f);
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
    setFloat(program, "shadowStrength", material.shadowStrength);
    setFloat(program, "shadowStart", material.shadowStart);
    setFloat(program, "shadowSoftness", material.shadowSoftness);
    setFloat(program, "cavityStrength", material.cavityStrength);
    setFloat(program, "curvatureThreshold", material.curvatureThreshold);
    setFloat(program, "curvatureSoftness", material.curvatureSoftness);
}

}
