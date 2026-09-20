#include "GLScalarSurfaceRenderer.h"

#include <cstdlib>

namespace gl = juce::gl;

namespace {

constexpr const char* vertexShaderSource = R"glsl(
#version 120
varying vec2 surfaceTextureCoordinate;

void main() {
    gl_Position = ftransform();
    surfaceTextureCoordinate = gl_MultiTexCoord0.xy;
}
)glsl";

constexpr const char* fragmentShaderSource = R"glsl(
#version 120
uniform sampler2D scalarTexture;
uniform vec2 texelSize;
uniform vec3 negativeAnchor;
uniform vec3 neutralAnchor;
uniform vec3 positiveAnchor;
uniform vec2 lightDirection;
uniform float opacity;
uniform float opacityValueScale;
uniform float reliefGain;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float curvatureThreshold;
uniform float curvatureSoftness;
uniform float accentStrength;
uniform float valueScale;
uniform float valueOffset;
uniform int paletteKind;
uniform int opacityPower;

varying vec2 surfaceTextureCoordinate;

float scalarAt(vec2 coordinate) {
    return clamp(texture2D(scalarTexture, coordinate).r * valueScale + valueOffset, 0.0, 1.0);
}

vec3 paletteColour(float value) {
    if (paletteKind == 1) {
        float amount = value * value * (3.0 - 2.0 * value);
        return mix(neutralAnchor, positiveAnchor, amount);
    }

    float magnitude = value < 0.5 ? 1.0 - 2.0 * value : 2.0 * value - 1.0;
    float amount = magnitude * (2.0 - magnitude);
    return value < 0.5
            ? mix(neutralAnchor, negativeAnchor, amount)
            : mix(neutralAnchor, positiveAnchor, amount);
}

void main() {
    vec2 coordinate = surfaceTextureCoordinate;
    float centre = scalarAt(coordinate);
    float left = scalarAt(coordinate - vec2(0.0, texelSize.y));
    float right = scalarAt(coordinate + vec2(0.0, texelSize.y));
    float lower = scalarAt(coordinate - vec2(texelSize.x, 0.0));
    float upper = scalarAt(coordinate + vec2(texelSize.x, 0.0));
    vec2 slope = 0.5 * vec2(right - left, upper - lower);
    float curvature = left + right + lower + upper - 4.0 * centre;
    float directionalSlope = -reliefGain * dot(slope, lightDirection);
    float diffuse = clamp(directionalSlope, -1.0, 1.0) * diffuseStrength;
    float positiveSlope = max(0.0, directionalSlope);
    float specular = min(1.0, positiveSlope * positiveSlope) * specularStrength;
    float curvatureAmount = smoothstep(
            curvatureThreshold,
            curvatureThreshold + curvatureSoftness,
            abs(curvature));

    vec3 colour = paletteColour(centre);
    vec3 accent = abs(centre - 0.5) < 0.08
            ? vec3(0.725, 0.761, 0.796)
            : mix(centre < 0.5 ? negativeAnchor : positiveAnchor, vec3(1.0), 0.22);
    colour = mix(colour, accent, curvatureAmount * accentStrength);
    colour *= clamp(1.0 + diffuse + specular, 0.72, 1.28);
    float opacityValue = opacityPower == 2 ? centre * centre : centre;
    float surfaceOpacity = opacityValueScale > 0.0
            ? min(opacity, opacityValueScale * opacityValue)
            : opacity;
    gl_FragColor = vec4(clamp(colour, 0.0, 1.0), surfaceOpacity);
}
)glsl";

void clearGlErrors() {
    while (gl::glGetError() != gl::GL_NO_ERROR) {
    }
}

void setColourUniform(unsigned int program, const char* name, juce::Colour colour) {
    const int location = gl::glGetUniformLocation(program, name);
    gl::glUniform3f(
            location,
            colour.getFloatRed(),
            colour.getFloatGreen(),
            colour.getFloatBlue());
}

}

bool ScalarSurfaceUploadState::needsUpload(const ScalarSurfaceRenderData& data) const {
    return !data.hasStableRevision
            || source != data.values
            || revision != data.revision
            || columns != data.columns
            || rows != data.rows
            || valueScale != data.valueScale
            || valueOffset != data.valueOffset;
}

void ScalarSurfaceUploadState::markUploaded(const ScalarSurfaceRenderData& data) {
    source = data.values;
    revision = data.revision;
    columns = data.columns;
    rows = data.rows;
    valueScale = data.valueScale;
    valueOffset = data.valueOffset;
}

void ScalarSurfaceUploadState::clear() {
    source = nullptr;
    revision = 0;
    columns = 0;
    rows = 0;
    valueScale = 0.f;
    valueOffset = 0.f;
}

bool GLScalarSurfaceRenderer::draw(const ScalarSurfaceRenderData& data) {
    if (!data.isValid() || std::getenv("CYCLE_DISABLE_SCALAR_SURFACE_SHADER") != nullptr) {
        return false;
    }

    if (!compileProgram() || !ensureTexture(data)) {
        return false;
    }

    gl::glUseProgram(program);
    gl::glActiveTexture(gl::GL_TEXTURE0);
    gl::glEnable(gl::GL_TEXTURE_2D);
    gl::glBindTexture(gl::GL_TEXTURE_2D, texture);
    setMaterialUniforms(data);

    const auto bounds = data.bounds;
    gl::glBegin(gl::GL_QUADS);
    gl::glTexCoord2f(1.f, 0.f);
    gl::glVertex2f(bounds.getX(), bounds.getY());
    gl::glTexCoord2f(1.f, 1.f);
    gl::glVertex2f(bounds.getRight(), bounds.getY());
    gl::glTexCoord2f(0.f, 1.f);
    gl::glVertex2f(bounds.getRight(), bounds.getBottom());
    gl::glTexCoord2f(0.f, 0.f);
    gl::glVertex2f(bounds.getX(), bounds.getBottom());
    gl::glEnd();

    gl::glUseProgram(0);
    gl::glDisable(gl::GL_TEXTURE_2D);
    ++diagnostics.drawCount;
    return true;
}

void GLScalarSurfaceRenderer::clearResources() {
    if (texture != 0) {
        gl::glDeleteTextures(1, &texture);
    }
    if (program != 0) {
        gl::glDeleteProgram(program);
    }

    uploadState.clear();
    program = 0;
    texture = 0;
    compileAttempted = false;
    usingFloatTexture = false;
    textureCapabilityFailed = false;
    diagnostics.capabilityAvailable = false;
}

bool GLScalarSurfaceRenderer::compileProgram() {
    if (program != 0) {
        return true;
    }
    if (compileAttempted) {
        return false;
    }

    compileAttempted = true;
    const unsigned int vertexShader = compileShader(gl::GL_VERTEX_SHADER, vertexShaderSource);
    const unsigned int fragmentShader = compileShader(gl::GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            gl::glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            gl::glDeleteShader(fragmentShader);
        }
        return false;
    }

    program = gl::glCreateProgram();
    gl::glAttachShader(program, vertexShader);
    gl::glAttachShader(program, fragmentShader);
    gl::glLinkProgram(program);
    gl::glDeleteShader(vertexShader);
    gl::glDeleteShader(fragmentShader);

    int linked = 0;
    gl::glGetProgramiv(program, gl::GL_LINK_STATUS, &linked);
    if (linked == 0) {
        gl::glDeleteProgram(program);
        program = 0;
        return false;
    }

    diagnostics.capabilityAvailable = true;
    return true;
}

bool GLScalarSurfaceRenderer::ensureTexture(const ScalarSurfaceRenderData& data) {
    if (textureCapabilityFailed) {
        return false;
    }

    if (texture == 0) {
        gl::glGenTextures(1, &texture);
        ++diagnostics.allocationCount;
    }

    if (textureMatches(data)) {
        return true;
    }

    return uploadTexture(data);
}

bool GLScalarSurfaceRenderer::uploadTexture(const ScalarSurfaceRenderData& data) {
    gl::glBindTexture(gl::GL_TEXTURE_2D, texture);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    gl::glPixelStorei(gl::GL_UNPACK_ALIGNMENT, 1);

    clearGlErrors();
    gl::glTexImage2D(
            gl::GL_TEXTURE_2D,
            0,
            gl::GL_R32F,
            data.rows,
            data.columns,
            0,
            gl::GL_RED,
            gl::GL_FLOAT,
            data.values);
    usingFloatTexture = gl::glGetError() == gl::GL_NO_ERROR;

    if (!usingFloatTexture) {
        clearGlErrors();
        gl::glTexImage2D(
                gl::GL_TEXTURE_2D,
                0,
                gl::GL_LUMINANCE,
                data.rows,
                data.columns,
                0,
                gl::GL_LUMINANCE,
                gl::GL_FLOAT,
                data.values);
        usingFloatTexture = gl::glGetError() == gl::GL_NO_ERROR;
    }

    if (!usingFloatTexture) {
        textureCapabilityFailed = true;
        return false;
    }

    uploadState.markUploaded(data);
    ++diagnostics.uploadCount;
    return true;
}

bool GLScalarSurfaceRenderer::textureMatches(const ScalarSurfaceRenderData& data) const {
    return !uploadState.needsUpload(data);
}

void GLScalarSurfaceRenderer::setMaterialUniforms(const ScalarSurfaceRenderData& data) const {
    const ScalarSurfaceMaterial& material = data.material;
    gl::glUniform1i(gl::glGetUniformLocation(program, "scalarTexture"), 0);
    gl::glUniform2f(
            gl::glGetUniformLocation(program, "texelSize"),
            1.f / (float) data.rows,
            1.f / (float) data.columns);
    setColourUniform(program, "negativeAnchor", material.negativeAnchor);
    setColourUniform(program, "neutralAnchor", material.neutralAnchor);
    setColourUniform(program, "positiveAnchor", material.positiveAnchor);
    gl::glUniform2f(gl::glGetUniformLocation(program, "lightDirection"), material.lightX, material.lightY);
    gl::glUniform1f(gl::glGetUniformLocation(program, "opacity"), material.opacity);
    gl::glUniform1f(gl::glGetUniformLocation(program, "opacityValueScale"), material.opacityValueScale);
    gl::glUniform1f(gl::glGetUniformLocation(program, "reliefGain"), material.reliefGain);
    gl::glUniform1f(gl::glGetUniformLocation(program, "diffuseStrength"), material.diffuseStrength);
    gl::glUniform1f(gl::glGetUniformLocation(program, "specularStrength"), material.specularStrength);
    gl::glUniform1f(gl::glGetUniformLocation(program, "curvatureThreshold"), material.curvatureThreshold);
    gl::glUniform1f(gl::glGetUniformLocation(program, "curvatureSoftness"), material.curvatureSoftness);
    gl::glUniform1f(gl::glGetUniformLocation(program, "accentStrength"), material.accentStrength);
    gl::glUniform1f(gl::glGetUniformLocation(program, "valueScale"), data.valueScale);
    gl::glUniform1f(gl::glGetUniformLocation(program, "valueOffset"), data.valueOffset);
    gl::glUniform1i(
            gl::glGetUniformLocation(program, "paletteKind"),
            (int) material.palette);
    gl::glUniform1i(gl::glGetUniformLocation(program, "opacityPower"), material.opacityPower);
}

unsigned int GLScalarSurfaceRenderer::compileShader(unsigned int type, const char* source) {
    const unsigned int shader = gl::glCreateShader(type);
    if (shader == 0) {
        return 0;
    }

    gl::glShaderSource(shader, 1, &source, nullptr);
    gl::glCompileShader(shader);

    int compiled = 0;
    gl::glGetShaderiv(shader, gl::GL_COMPILE_STATUS, &compiled);
    if (compiled == 0) {
        gl::glDeleteShader(shader);
        return 0;
    }

    return shader;
}
