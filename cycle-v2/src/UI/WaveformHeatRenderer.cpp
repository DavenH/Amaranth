#include "UI/WaveformHeatRenderer.h"
#include "UI/CanvasChromePalette.h"
#include "UI/WaveformColourStyles.h"

#include <algorithm>
#include <array>

namespace CycleV2 {

namespace {

constexpr auto shaderCode = R"glsl(
uniform sampler2D coverageTexture;
uniform sampler2D heatTexture;
uniform vec4 plotBounds;
uniform vec4 viewport;
uniform vec4 coverageViewport;
uniform vec4 coverageScale;
uniform vec4 heatScale;
uniform float filled;
uniform vec3 fillColour;
uniform float fillGridBacking;
void main() {
    vec2 position = (pixelPos - plotBounds.xy) / plotBounds.zw;
    vec2 source = viewport.xy + position * viewport.zw;
    vec2 mask = (source - coverageViewport.xy) / coverageViewport.zw;
    float inside = step(0.0, mask.x) * step(mask.x, 1.0) * step(0.0, mask.y) * step(mask.y, 1.0);
    vec2 maskUV = vec2(mask.x * coverageScale.x + coverageScale.z,
                      1.0 - mask.y * coverageScale.y - coverageScale.w);
    vec2 heatUV = vec2(source.x * heatScale.x + heatScale.z,
                      1.0 - source.y * heatScale.y - heatScale.w);
    vec4 coverage = texture2D(coverageTexture, maskUV);
    float stroke = mix(coverage.a, coverage.r, filled);
    float fill = coverage.a - stroke;
    vec3 strokeColour = mix(texture2D(heatTexture, heatUV).rgb, fillColour, filled);
    vec3 colour = strokeColour * stroke + fillColour * fill;
    float alpha = min(1.0, coverage.a + fillGridBacking * fill);
    gl_FragColor = vec4(colour, alpha) * inside * pixelAlpha;
}
)glsl";

// Context ownership releases textures on the GL thread, including context recreation.
// Eight visible/source pairs bound retained memory when views or sources are replaced.
class WaveformHeatResources final : public juce::ReferenceCountedObject {
public:
    struct Entry {
        juce::Image coverage;
        juce::Image heat;
        juce::OpenGLTexture coverageTexture;
        juce::OpenGLTexture heatTexture;
        uint64_t use {};
    };

    Entry& texturesFor(const juce::Image& coverage, const juce::Image& heat) {
        auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry.coverage == coverage || entry.heat == heat;
        });
        auto& entry = found != entries.end() ? *found
                : *std::min_element(entries.begin(), entries.end(), [](const auto& first, const auto& second) {
                    return first.use < second.use;
                });
        if (entry.coverage != coverage) {
            entry.coverageTexture.loadImage(coverage);
            entry.coverage = coverage;
        }
        if (entry.heat != heat) {
            entry.heatTexture.loadImage(heat);
            entry.heat = heat;
        }
        entry.use = ++use;
        return entry;
    }

    juce::OpenGLGraphicsContextCustomShader shader { shaderCode };

private:
    std::array<Entry, 8> entries;
    uint64_t use {};
};

void setTextureScale(juce::OpenGLShaderProgram& program, const char* name,
        const juce::Image& image, const juce::OpenGLTexture& texture) {
    program.setUniform(name, (float) (image.getWidth() - 1) / texture.getWidth(),
            (float) (image.getHeight() - 1) / texture.getHeight(),
            0.5f / texture.getWidth(), 0.5f / texture.getHeight());
}

}

bool paintWaveformHeatOpenGL(juce::Graphics& graphics, const juce::Image& coverage,
        const juce::Image& heat, const ZoomRect& coverageViewport, const ZoomRect& viewport,
        juce::Rectangle<float> bounds, juce::Rectangle<float> deviceBounds, bool filled) {
    auto* context = juce::OpenGLContext::getCurrentContext();
    if (context == nullptr || !coverage.isValid() || !heat.isValid() || deviceBounds.isEmpty()) {
        return false;
    }
    constexpr auto key = "cycle-v2-waveform-heat";
    auto* resources = static_cast<WaveformHeatResources*>(context->getAssociatedObject(key));
    if (resources == nullptr) {
        resources = new WaveformHeatResources();
        context->setAssociatedObject(key, resources);
    }
    if (resources->shader.getProgram(graphics.getInternalContext()) == nullptr) {
        return false;
    }
    using namespace juce::gl;
    GLint active {};
    std::array<GLint, 2> bindings {};
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    for (int unit = 0; unit < 2; ++unit) {
        context->extensions.glActiveTexture(GL_TEXTURE0 + unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bindings[(size_t) unit]);
    }
    auto& textures = resources->texturesFor(coverage, heat);
    resources->shader.onShaderActivated = [context, &textures, coverage, heat, deviceBounds, viewport, coverageViewport, filled](auto& program) {
        context->extensions.glActiveTexture(GL_TEXTURE0);
        textures.coverageTexture.bind();
        context->extensions.glActiveTexture(GL_TEXTURE1);
        textures.heatTexture.bind();
        program.setUniform("coverageTexture", 0);
        program.setUniform("heatTexture", 1);
        program.setUniform("filled", filled ? 1.f : 0.f);
        program.setUniform("fillGridBacking", (float) WaveformColourStyles::fillGridBacking);
        program.setUniform("fillColour", CanvasChromePalette::text.getFloatRed(),
                CanvasChromePalette::text.getFloatGreen(), CanvasChromePalette::text.getFloatBlue());
        program.setUniform("plotBounds", deviceBounds.getX(), deviceBounds.getY(), deviceBounds.getWidth(), deviceBounds.getHeight());
        program.setUniform("viewport", viewport.x, viewport.y, viewport.w, viewport.h);
        program.setUniform("coverageViewport", coverageViewport.x, coverageViewport.y, coverageViewport.w, coverageViewport.h);
        setTextureScale(program, "coverageScale", coverage, textures.coverageTexture);
        setTextureScale(program, "heatScale", heat, textures.heatTexture);
    };
    resources->shader.fillRect(graphics.getInternalContext(), bounds.toNearestInt());
    resources->shader.onShaderActivated = {};
    for (int unit = 0; unit < 2; ++unit) {
        context->extensions.glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, (GLuint) bindings[(size_t) unit]);
    }
    context->extensions.glActiveTexture((GLenum) active);
    return true;
}

}
