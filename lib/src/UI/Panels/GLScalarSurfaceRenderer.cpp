#include <Binary/Gradients.h>
#include <array>
#include <cstdlib>
#include <utility>
#include <vector>

#include "GLScalarSurfaceRenderer.h"
#include "GLScalarSurfaceUniforms.h"

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
uniform sampler2D scalarPaletteTexture;
uniform vec3 negativeAnchor;
uniform vec3 neutralAnchor;
uniform vec3 positiveAnchor;
uniform vec3 signedPalette[9];
uniform float signedPalettePositions[9];
uniform vec3 negativePearlTint;
uniform vec3 neutralPearlTint;
uniform vec3 positivePearlTint;
uniform vec3 negativeEdgeTint;
uniform vec3 positiveEdgeTint;
uniform vec3 directionalDetailColours[3];
uniform vec2 textureStep;
uniform vec2 textureToDomainScale;
uniform vec2 detailBoundaryStart;
uniform vec2 detailBoundaryEnd;
uniform vec4 hillshadeWeights;
uniform vec3 obscuranceBiases;
uniform vec3 lightDirection;
uniform float surfaceAspectRatio;
uniform float opacity;
uniform float opacityValueScale;
uniform float reliefScale;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float pearlTintStrength;
uniform float obscuranceStrength;
uniform float obscuranceScale;
uniform float exposureStrength;
uniform float exposureScale;
uniform float exposureBias;
uniform float detailReliefScale;
uniform float embossLimit;
uniform float embossStrength;
uniform float detailGradientKnee;
uniform float detailEnergyKnee;
uniform float edgeTintStrength;
uniform float neutralAccentWidth;
uniform int paletteKind;
uniform int reliefKind;
uniform int detailColourKind;
uniform int opacityPower;
uniform int specularPower;

varying vec2 surfaceTextureCoordinate;

vec4 heightScalesAt(vec2 coordinate) {
    return texture2D(scalarTexture, coordinate);
}

vec3 paletteColour(float value) {
    if (paletteKind == 1) {
        float paletteX = (floor(value * 511.0 + 0.5) + 0.5) / 512.0;
        return texture2D(scalarPaletteTexture, vec2(paletteX, 0.25)).rgb;
    }

    if (paletteKind == 3) {
        float paletteX = (floor(value * 511.0 + 0.5) + 0.5) / 512.0;
        return texture2D(scalarPaletteTexture, vec2(paletteX, 0.75)).rgb;
    }

    if (paletteKind == 0) {
        if (value < signedPalettePositions[1]) {
            return mix(signedPalette[0], signedPalette[1],
                    smoothstep(signedPalettePositions[0], signedPalettePositions[1], value));
        }
        if (value < signedPalettePositions[2]) {
            return mix(signedPalette[1], signedPalette[2],
                    smoothstep(signedPalettePositions[1], signedPalettePositions[2], value));
        }
        if (value < signedPalettePositions[3]) {
            return mix(signedPalette[2], signedPalette[3],
                    smoothstep(signedPalettePositions[2], signedPalettePositions[3], value));
        }
        if (value < signedPalettePositions[4]) {
            return mix(signedPalette[3], signedPalette[4],
                    smoothstep(signedPalettePositions[3], signedPalettePositions[4], value));
        }
        if (value < signedPalettePositions[5]) {
            return mix(signedPalette[4], signedPalette[5],
                    smoothstep(signedPalettePositions[4], signedPalettePositions[5], value));
        }
        if (value < signedPalettePositions[6]) {
            return mix(signedPalette[5], signedPalette[6],
                    smoothstep(signedPalettePositions[5], signedPalettePositions[6], value));
        }
        if (value < signedPalettePositions[7]) {
            return mix(signedPalette[6], signedPalette[7],
                    smoothstep(signedPalettePositions[6], signedPalettePositions[7], value));
        }
        return mix(signedPalette[7], signedPalette[8],
                smoothstep(signedPalettePositions[7], signedPalettePositions[8], value));
    }

    float magnitude = value < 0.5 ? 1.0 - 2.0 * value : 2.0 * value - 1.0;
    float amount = magnitude * (2.0 - magnitude);
    return value < 0.5
            ? mix(neutralAnchor, negativeAnchor, amount)
            : mix(neutralAnchor, positiveAnchor, amount);
}

vec3 srgbToLinear(vec3 colour) {
    vec3 low = colour / 12.92;
    vec3 high = pow((colour + 0.055) / 1.055, vec3(2.4));
    return mix(low, high, step(vec3(0.04045), colour));
}

vec3 linearToSrgb(vec3 colour) {
    colour = clamp(colour, 0.0, 1.0);
    vec3 low = 12.92 * colour;
    vec3 high = 1.055 * pow(colour, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), colour));
}

vec3 pearlColour(float value) {
    float magnitude = smoothstep(0.0, 1.0, abs(value - 0.5) * 2.0);
    vec3 semantic = value < 0.5 ? negativePearlTint : positivePearlTint;
    return mix(neutralPearlTint, semantic, magnitude);
}

vec3 directionalDetailColourLinear(vec2 gradient) {
    const float sine60 = 0.8660254;
    vec3 projection = vec3(
            gradient.x,
            0.5 * gradient.x + sine60 * gradient.y,
            -0.5 * gradient.x + sine60 * gradient.y);
    vec3 weights = projection * projection;
    weights *= weights;
    float totalWeight = max(1.e-20, weights.x + weights.y + weights.z);
    return (
            srgbToLinear(directionalDetailColours[0]) * weights.x
            + srgbToLinear(directionalDetailColours[1]) * weights.y
            + srgbToLinear(directionalDetailColours[2]) * weights.z)
            / totalWeight;
}

float saturatingDetailResponse(float value, float knee) {
    float positiveValue = max(0.0, value);
    return positiveValue / max(0.000001, positiveValue + knee);
}

void main() {
    vec2 coordinate = surfaceTextureCoordinate;
    vec4 centreScales = heightScalesAt(coordinate);
    float centre = centreScales.r;
    vec3 base = floor(paletteColour(centre) * 255.0 + 0.5) / 255.0;
    float opacityValue = opacityPower == 2 ? centre * centre : centre;
    float surfaceOpacity = opacityValueScale > 0.0
            ? min(opacity, opacityValueScale * opacityValue)
            : opacity;

    if (reliefKind == 2) {
        gl_FragColor = vec4(base, surfaceOpacity);
        return;
    }

    float leftCoordinate = coordinate.y < textureStep.y
            ? coordinate.y : coordinate.y - textureStep.y;
    float rightCoordinate = coordinate.y > 1.0 - textureStep.y
            ? coordinate.y : coordinate.y + textureStep.y;
    float lowerCoordinate = coordinate.x < textureStep.x
            ? coordinate.x : coordinate.x - textureStep.x;
    float upperCoordinate = coordinate.x > 1.0 - textureStep.x
            ? coordinate.x : coordinate.x + textureStep.x;
    vec4 left = heightScalesAt(vec2(coordinate.x, leftCoordinate));
    vec4 right = heightScalesAt(vec2(coordinate.x, rightCoordinate));
    vec4 lower = heightScalesAt(vec2(lowerCoordinate, coordinate.y));
    vec4 upper = heightScalesAt(vec2(upperCoordinate, coordinate.y));
    float xDistance = (rightCoordinate - leftCoordinate)
            * textureToDomainScale.y * surfaceAspectRatio;
    float yDistance = (upperCoordinate - lowerCoordinate)
            * textureToDomainScale.x;
    vec4 gradientX = (right - left) / xDistance;
    vec4 gradientY = (upper - lower) / yDistance;
    vec3 light = normalize(lightDirection);

    if (reliefKind == 0) {
        vec2 detailGradient = vec2(
                gradientX.r - gradientX.g,
                gradientY.r - gradientY.g);
        float boundaryX = smoothstep(
                detailBoundaryStart.x,
                detailBoundaryEnd.x,
                min(coordinate.x, 1.0 - coordinate.x));
        float boundaryY = smoothstep(
                detailBoundaryStart.y,
                detailBoundaryEnd.y,
                min(coordinate.y, 1.0 - coordinate.y));
        float boundaryFade = min(boundaryX, boundaryY);
        vec3 detailNormal = normalize(vec3(-detailGradient * detailReliefScale, 1.0));
        float emboss = clamp(
                (dot(detailNormal, light) - light.z) / max(embossLimit, 0.000001),
                -1.0,
                1.0) * boundaryFade;
        vec3 colour = srgbToLinear(base) * (1.0 + embossStrength * emboss);
        float gradientEnergy = dot(detailGradient, detailGradient);
        float gradientResponse = saturatingDetailResponse(
                gradientEnergy,
                detailGradientKnee * detailGradientKnee);
        float detailResponse = saturatingDetailResponse(
                abs(centreScales.r - centreScales.g),
                detailEnergyKnee);
        float semanticMagnitude = smoothstep(
                neutralAccentWidth,
                1.0,
                abs(2.0 * centre - 1.0));
        bool directionalDetail = detailColourKind == 1;
        if (directionalDetail) {
            semanticMagnitude = 1.0;
        }
        float slopeWeight = directionalDetail
                ? gradientResponse
                : 0.35 + 0.65 * gradientResponse;
        float edgeAmount = detailResponse * slopeWeight * semanticMagnitude
                * edgeTintStrength * boundaryFade;
        vec3 edgeTintLinear = directionalDetail
                ? directionalDetailColourLinear(detailGradient)
                : srgbToLinear(centre < 0.5 ? negativeEdgeTint : positiveEdgeTint);
        colour = mix(colour, edgeTintLinear, edgeAmount);
        gl_FragColor = vec4(linearToSrgb(colour), surfaceOpacity);
        return;
    }

    vec3 halfVector = normalize(light + vec3(0.0, 0.0, 1.0));
    vec3 normal0 = normalize(vec3(-vec2(gradientX.r, gradientY.r) * reliefScale, 1.0));
    vec3 normal1 = normalize(vec3(-vec2(gradientX.g, gradientY.g) * reliefScale, 1.0));
    vec3 normal2 = normalize(vec3(-vec2(gradientX.b, gradientY.b) * reliefScale, 1.0));
    vec3 normal3 = normalize(vec3(-vec2(gradientX.a, gradientY.a) * reliefScale, 1.0));
    vec4 diffuse = max(vec4(
            dot(normal0, light),
            dot(normal1, light),
            dot(normal2, light),
            dot(normal3, light)), 0.0);
    float hillshade = dot(diffuse, hillshadeWeights);
    float specular = pow(max(dot(normal2, halfVector), 0.0), float(specularPower));
    vec3 cavity = max(
            centreScales.gba - centreScales.rrr - obscuranceBiases,
            vec3(0.0));
    float obscurance = clamp(obscuranceScale * dot(cavity, hillshadeWeights.gba), 0.0, 1.0);
    float exposure = clamp(exposureScale
            * max(centreScales.r - centreScales.b - exposureBias, 0.0), 0.0, 1.0);
    vec3 colour = srgbToLinear(base);
    float illumination = max(0.16,
            ambientStrength + diffuseStrength * hillshade
                    - obscuranceStrength * obscurance
                    + exposureStrength * exposure);
    colour *= illumination;
    vec3 highlight = mix(srgbToLinear(base), srgbToLinear(pearlColour(centre)),
            pearlTintStrength);
    float highlightAmount = specularStrength * specular
            + exposureStrength * pearlTintStrength * exposure;
    colour = mix(colour, highlight, clamp(highlightAmount, 0.0, 1.0));
    gl_FragColor = vec4(linearToSrgb(colour), surfaceOpacity);
}
)glsl";

void clearGlErrors() {
    while (gl::glGetError() != gl::GL_NO_ERROR) {
    }
}

int channelError(juce::uint8 actual, juce::uint8 expected) {
    const int difference = (int) actual - (int) expected;
    return difference < 0 ? -difference : difference;
}

void restoreCapability(unsigned int capability, bool enabled) {
    if (enabled) {
        gl::glEnable(capability);
    } else {
        gl::glDisable(capability);
    }
}

class ScalarSurfaceValidationGlState {
public:
    ScalarSurfaceValidationGlState() {
        framebuffer = juce::OpenGLFrameBuffer::getCurrentFrameBufferTarget();
        gl::glGetIntegerv(gl::GL_VIEWPORT, viewport);
        gl::glGetIntegerv(gl::GL_ACTIVE_TEXTURE, &activeTexture);
        gl::glGetIntegerv(gl::GL_CURRENT_PROGRAM, &program);
        gl::glGetIntegerv(gl::GL_MATRIX_MODE, &matrixMode);
        gl::glGetFloatv(gl::GL_COLOR_CLEAR_VALUE, clearColour);
        gl::glActiveTexture(gl::GL_TEXTURE0);
        gl::glGetIntegerv(gl::GL_TEXTURE_BINDING_2D, &texture0);
        gl::glActiveTexture(gl::GL_TEXTURE1);
        gl::glGetIntegerv(gl::GL_TEXTURE_BINDING_2D, &texture1);
        gl::glActiveTexture((unsigned int) activeTexture);
        blendEnabled = gl::glIsEnabled(gl::GL_BLEND) != 0;
        depthEnabled = gl::glIsEnabled(gl::GL_DEPTH_TEST) != 0;
        scissorEnabled = gl::glIsEnabled(gl::GL_SCISSOR_TEST) != 0;
        textureEnabled = gl::glIsEnabled(gl::GL_TEXTURE_2D) != 0;
    }

    ~ScalarSurfaceValidationGlState() {
        gl::glUseProgram((unsigned int) program);
        if (matricesPushed) {
            gl::glMatrixMode(gl::GL_MODELVIEW);
            gl::glPopMatrix();
            gl::glMatrixMode(gl::GL_PROJECTION);
            gl::glPopMatrix();
        }
        gl::glMatrixMode((unsigned int) matrixMode);
        gl::glBindFramebuffer(gl::GL_FRAMEBUFFER, framebuffer);
        gl::glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        gl::glClearColor(clearColour[0], clearColour[1], clearColour[2], clearColour[3]);
        restoreCapability(gl::GL_BLEND, blendEnabled);
        restoreCapability(gl::GL_DEPTH_TEST, depthEnabled);
        restoreCapability(gl::GL_SCISSOR_TEST, scissorEnabled);
        restoreCapability(gl::GL_TEXTURE_2D, textureEnabled);
        gl::glActiveTexture(gl::GL_TEXTURE0);
        gl::glBindTexture(gl::GL_TEXTURE_2D, (unsigned int) texture0);
        gl::glActiveTexture(gl::GL_TEXTURE1);
        gl::glBindTexture(gl::GL_TEXTURE_2D, (unsigned int) texture1);
        gl::glActiveTexture((unsigned int) activeTexture);
    }

    void prepare(int width, int height, unsigned int shaderProgram) {
        gl::glDisable(gl::GL_BLEND);
        gl::glDisable(gl::GL_DEPTH_TEST);
        gl::glDisable(gl::GL_SCISSOR_TEST);
        gl::glEnable(gl::GL_TEXTURE_2D);
        gl::glViewport(0, 0, width, height);
        gl::glMatrixMode(gl::GL_PROJECTION);
        gl::glPushMatrix();
        gl::glLoadIdentity();
        gl::glMatrixMode(gl::GL_MODELVIEW);
        gl::glPushMatrix();
        gl::glLoadIdentity();
        gl::glUseProgram(shaderProgram);
        matricesPushed = true;
    }

private:
    int viewport[4] {};
    int activeTexture {};
    int texture0 {};
    int texture1 {};
    int program {};
    int matrixMode {};
    float clearColour[4] {};
    unsigned int framebuffer {};
    bool blendEnabled {};
    bool depthEnabled {};
    bool scissorEnabled {};
    bool textureEnabled {};
    bool matricesPushed {};
};

void drawValidationQuad() {
    gl::glBegin(gl::GL_QUADS);
    gl::glTexCoord2f(0.f, 0.f);
    gl::glVertex2f(-1.f, -1.f);
    gl::glTexCoord2f(0.f, 1.f);
    gl::glVertex2f(1.f, -1.f);
    gl::glTexCoord2f(1.f, 1.f);
    gl::glVertex2f(1.f, 1.f);
    gl::glTexCoord2f(1.f, 0.f);
    gl::glVertex2f(-1.f, 1.f);
    gl::glEnd();
}

int maximumValidationError(
        const juce::PixelARGB* pixels,
        const ScalarSurfaceHeightScales& scales,
        const ScalarSurfaceMaterial& material,
        float surfaceAspectRatio) {
    int maximumError = 0;
    for (int column = 0; column < scales.columns; ++column) {
        for (int row = 0; row < scales.rows; ++row) {
            const int index = column * scales.rows + row;
            const juce::Colour expected = ScalarSurfaceMaterialEvaluator::colourFor(
                    scales.packedValues[(size_t) index * 4],
                    ScalarSurfaceMaterialEvaluator::derivativesAt(
                            scales,
                            column,
                            row,
                            material,
                            surfaceAspectRatio),
                    material);
            const juce::PixelARGB& actual = pixels[row * scales.columns + column];
            maximumError = juce::jmax(
                    maximumError,
                    channelError(actual.getRed(), expected.getRed()));
            maximumError = juce::jmax(
                    maximumError,
                    channelError(actual.getGreen(), expected.getGreen()));
            maximumError = juce::jmax(
                    maximumError,
                    channelError(actual.getBlue(), expected.getBlue()));
            maximumError = juce::jmax(
                    maximumError,
                    channelError(actual.getAlpha(), expected.getAlpha()));
        }
    }
    return maximumError;
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

    if (!compileProgram() || !ensureScalarPaletteTexture() || !ensureTexture(data)) {
        return false;
    }

    gl::glActiveTexture(gl::GL_TEXTURE1);
    gl::glBindTexture(gl::GL_TEXTURE_2D, scalarPaletteTexture);
    gl::glActiveTexture(gl::GL_TEXTURE0);
    validateGpuParity();

    gl::glUseProgram(program);
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
    if (scalarPaletteTexture != 0) {
        gl::glDeleteTextures(1, &scalarPaletteTexture);
    }
    if (program != 0) {
        gl::glDeleteProgram(program);
    }

    uploadState.clear();
    packedHeightScales.clear();
    program = 0;
    texture = 0;
    scalarPaletteTexture = 0;
    compileAttempted = false;
    usingFloatTexture = false;
    textureCapabilityFailed = false;
    diagnostics.capabilityAvailable = false;
    diagnostics.gpuValidationAttempted = false;
    diagnostics.gpuValidationPassed = false;
    diagnostics.gpuValidationMaximumError = 0;
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

bool GLScalarSurfaceRenderer::ensureScalarPaletteTexture() {
    if (scalarPaletteTexture != 0) {
        return true;
    }

    const juce::Image magnitudeGradient = juce::PNGImageFormat::loadFrom(
            Gradients::burntalum_png,
            Gradients::burntalum_pngSize);
    const juce::Image blueGradient = juce::PNGImageFormat::loadFrom(
            Gradients::blue_png,
            Gradients::blue_pngSize);
    if (!magnitudeGradient.isValid()
            || !blueGradient.isValid()
            || magnitudeGradient.getWidth() != blueGradient.getWidth()) {
        return false;
    }

    std::vector<juce::uint8> pixels((size_t) magnitudeGradient.getWidth() * 2 * 4);
    for (int row = 0; row < 2; ++row) {
        const juce::Image& gradient = row == 0 ? magnitudeGradient : blueGradient;
        for (int x = 0; x < gradient.getWidth(); ++x) {
            const juce::Colour colour = gradient.getPixelAt(x, 0);
            const size_t offset = ((size_t) row * (size_t) gradient.getWidth()
                    + (size_t) x) * 4;
            pixels[offset] = colour.getRed();
            pixels[offset + 1] = colour.getGreen();
            pixels[offset + 2] = colour.getBlue();
            pixels[offset + 3] = 255;
        }
    }

    gl::glGenTextures(1, &scalarPaletteTexture);
    gl::glActiveTexture(gl::GL_TEXTURE1);
    gl::glBindTexture(gl::GL_TEXTURE_2D, scalarPaletteTexture);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    gl::glPixelStorei(gl::GL_UNPACK_ALIGNMENT, 1);
    clearGlErrors();
    gl::glTexImage2D(
            gl::GL_TEXTURE_2D,
            0,
            gl::GL_RGBA,
            magnitudeGradient.getWidth(),
            2,
            0,
            gl::GL_RGBA,
            gl::GL_UNSIGNED_BYTE,
            pixels.data());
    const bool succeeded = gl::glGetError() == gl::GL_NO_ERROR;
    gl::glActiveTexture(gl::GL_TEXTURE0);
    if (!succeeded) {
        gl::glDeleteTextures(1, &scalarPaletteTexture);
        scalarPaletteTexture = 0;
    }
    return succeeded;
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
    ScalarSurfaceHeightScales scales = ScalarSurfaceMaterialEvaluator::createHeightScales(
            data.values,
            data.valueCount,
            data.columns,
            data.rows,
            data.material,
            data.valueScale,
            data.valueOffset);
    if (!scales.isValid()) {
        return false;
    }
    packedHeightScales = std::move(scales.packedValues);

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
            gl::GL_RGBA32F,
            data.rows,
            data.columns,
            0,
            gl::GL_RGBA,
            gl::GL_FLOAT,
            packedHeightScales.data());
    usingFloatTexture = gl::glGetError() == gl::GL_NO_ERROR;

    if (!usingFloatTexture) {
        clearGlErrors();
        gl::glTexImage2D(
                gl::GL_TEXTURE_2D,
                0,
                gl::GL_RGBA,
                data.rows,
                data.columns,
                0,
                gl::GL_RGBA,
                gl::GL_FLOAT,
                packedHeightScales.data());
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

void GLScalarSurfaceRenderer::validateGpuParity() {
    if (diagnostics.gpuValidationAttempted
            || std::getenv("CYCLE_VALIDATE_SCALAR_SURFACE_SHADER") == nullptr) {
        return;
    }

    diagnostics.gpuValidationAttempted = true;
    juce::OpenGLContext* context = juce::OpenGLContext::getCurrentContext();
    constexpr int columns = 5;
    constexpr int rows = 5;
    // Driver texture sampling differs slightly from the CPU edge clamps after
    // the wider smoothed derivative stencil and stronger signed relief.
    constexpr int tolerance = 8;
    const std::array<float, columns * rows> values {
            0.08f, 0.14f, 0.22f, 0.14f, 0.08f,
            0.18f, 0.30f, 0.42f, 0.30f, 0.18f,
            0.34f, 0.48f, 0.72f, 0.48f, 0.34f,
            0.58f, 0.70f, 0.86f, 0.70f, 0.58f,
            0.78f, 0.88f, 0.96f, 0.88f, 0.78f
    };
    const std::array<ScalarSurfaceMaterial, 5> materials {
            ScalarSurfaceMaterial::signedAmplitude(),
            ScalarSurfaceMaterial::blueDepth(),
            ScalarSurfaceMaterial::blueDepthDirectionalDetail(),
            ScalarSurfaceMaterial::unipolarMagnitude(),
            ScalarSurfaceMaterial::bipolarPhase()
    };
    if (context == nullptr) {
        DBG("ScalarSurfaceGpuValidation failed: no current OpenGL context");
        return;
    }

    ScalarSurfaceValidationGlState savedState;
    juce::OpenGLFrameBuffer framebuffer;
    if (!framebuffer.initialise(*context, columns, rows)) {
        DBG("ScalarSurfaceGpuValidation failed: framebuffer unavailable");
        return;
    }

    unsigned int validationTexture = 0;
    gl::glGenTextures(1, &validationTexture);
    gl::glActiveTexture(gl::GL_TEXTURE0);
    gl::glBindTexture(gl::GL_TEXTURE_2D, validationTexture);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    savedState.prepare(columns, rows, program);

    int maximumError = 0;
    bool readSucceeded = true;
    std::array<juce::PixelARGB, columns * rows> pixels;
    for (const ScalarSurfaceMaterial& material: materials) {
        const ScalarSurfaceHeightScales scales =
                ScalarSurfaceMaterialEvaluator::createHeightScales(
                        values.data(), (int) values.size(), columns, rows, material);
        gl::glBindTexture(gl::GL_TEXTURE_2D, validationTexture);
        gl::glTexImage2D(
                gl::GL_TEXTURE_2D,
                0,
                gl::GL_RGBA32F,
                rows,
                columns,
                0,
                gl::GL_RGBA,
                gl::GL_FLOAT,
                scales.packedValues.data());
        framebuffer.makeCurrentAndClear();
        gl::glBindTexture(gl::GL_TEXTURE_2D, validationTexture);

        ScalarSurfaceRenderData data;
        data.values = values.data();
        data.valueCount = (int) values.size();
        data.material = material;
        data.columns = columns;
        data.rows = rows;
        data.bounds = { 0.f, 0.f, (float) columns, (float) rows };
        setMaterialUniforms(data);

        drawValidationQuad();
        gl::glFinish();

        readSucceeded = framebuffer.readPixels(
                pixels.data(), { 0, 0, columns, rows }) && readSucceeded;
        maximumError = juce::jmax(
                maximumError,
                maximumValidationError(
                        pixels.data(), scales, material, 1.f));
    }

    gl::glDeleteTextures(1, &validationTexture);

    diagnostics.gpuValidationMaximumError = maximumError;
    diagnostics.gpuValidationPassed = readSucceeded && maximumError <= tolerance;
    DBG(juce::String("ScalarSurfaceGpuValidation ")
            + juce::String(diagnostics.gpuValidationPassed ? "passed" : "failed")
            + " maximumChannelError=" + juce::String(maximumError));
}

void GLScalarSurfaceRenderer::setMaterialUniforms(const ScalarSurfaceRenderData& data) const {
    const ScalarSurfaceMaterial& material = data.material;
    gl::glUniform1i(gl::glGetUniformLocation(program, "scalarTexture"), 0);
    gl::glUniform1i(gl::glGetUniformLocation(program, "scalarPaletteTexture"), 1);
    ScalarSurfaceUniforms::setPalette(program, material);
    ScalarSurfaceUniforms::setSampling(program, data, material);
    ScalarSurfaceUniforms::setLighting(program, data, material);
    ScalarSurfaceUniforms::setFloat(program, "opacity", material.opacity);
    ScalarSurfaceUniforms::setFloat(program, "opacityValueScale", material.opacityValueScale);
    gl::glUniform1i(
            gl::glGetUniformLocation(program, "paletteKind"),
            (int) material.palette);
    gl::glUniform1i(
            gl::glGetUniformLocation(program, "reliefKind"),
            (int) material.relief);
    gl::glUniform1i(
            gl::glGetUniformLocation(program, "detailColourKind"),
            (int) material.detailColour);
    gl::glUniform1i(gl::glGetUniformLocation(program, "opacityPower"), material.opacityPower);
    gl::glUniform1i(gl::glGetUniformLocation(program, "specularPower"), material.specularPower);
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
