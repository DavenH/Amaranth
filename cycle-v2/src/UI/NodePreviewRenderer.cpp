#include "UI/NodePreviewRenderer.h"

#include "Graph/GraphRenderSemanticResolver.h"
#include "Graph/NodeParameterMap.h"
#include "Nodes/Delay/DelayPreviewPainter.h"
#include "Nodes/Equalizer/EqualizerPreviewPainter.h"
#include "Nodes/Reverb/ReverbPreviewPainter.h"
#include "Nodes/Trimesh/Rendering/TrimeshSurfaceRenderer.h"
#include "Nodes/Unison/UnisonPreviewPainter.h"
#include "UI/Preview/EffectPlotPalette.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace CycleV2 {

namespace {

const Colour kMutedText { 0xff8793a1 };
constexpr float kSignedLogDisplayScale = 0.1442695f;

float fastSin(float value) {
    return (float) dsp::FastMathApproximations::sin((double) value);
}

Rectangle<float> previewContentArea(Rectangle<float> area) {
    return area.reduced(jmin(area.getWidth(), area.getHeight()) * 0.12f);
}

Colour previewColourForRole(PreviewModuleRole role, const Node& node) {
    switch (role) {
        case PreviewModuleRole::SpectrumMagnitude:
            return colourForDomain(PortDomain::SpectralMagnitudeSignal);
        case PreviewModuleRole::SpectrumPhase:
            return colourForDomain(PortDomain::SpectralPhaseSignal);
        case PreviewModuleRole::Envelope:
            return colourForDomain(PortDomain::EnvelopeSignal);
        case PreviewModuleRole::OutputMeters:
        case PreviewModuleRole::Waveform:
        case PreviewModuleRole::Waveshaper:
        case PreviewModuleRole::ReverbSpectrogram:
        case PreviewModuleRole::EqualizerResponse:
            return role == PreviewModuleRole::EqualizerResponse
                    ? EffectPlotPalette::forEnabledState(
                            colourForDomain(PortDomain::TimeSignal),
                            NodeParameterMap(node).boolValue("enabled", true))
                    : colourForDomain(PortDomain::TimeSignal);
        case PreviewModuleRole::SignalSpy:
            return Colour(0xffd2d9e2);
        case PreviewModuleRole::MeshSurface:
            return colourForDomain(PortDomain::MeshField);
        default:
            break;
    }

    return node.outputs.empty()
            ? Colour(0xff9aa5b2)
            : colourForDomain(node.outputs.front().domain);
}

String nodeSignature(const Node& node, PortDomain domain) {
    String signature = String((int) node.kind) + ":" + String((int) domain);

    for (const auto& parameter : node.parameters) {
        signature += "|" + parameter.id + "=" + parameter.value;
    }

    for (const auto& output : node.outputs) {
        signature += "|out:" + output.id + ":" + String((int) output.domain);
    }

    return signature;
}

uint64_t previewContentHash(const NodePreviewResult& preview) {
    uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };

    mix((uint64_t) preview.primary.size());
    for (const float value : preview.primary) {
        uint32_t bits {};
        std::memcpy(&bits, &value, sizeof(bits));
        mix(bits);
    }

    return hash;
}

String runtimeSignature(const NodePreviewResult& preview) {
    return String((int) preview.role)
            + ":" + String((int) preview.domain)
            + ":" + String((int) preview.frequencySampling)
            + ":" + String(preview.frequencyMidiNote)
            + ":" + String((int) preview.gridColumns)
            + "x" + String((int) preview.gridRows)
            + ":" + String((int) preview.primary.size())
            + ":" + String::toHexString((int64) previewContentHash(preview));
}

void drawTrace(
        Graphics& graphics,
        Rectangle<float> area,
        const std::vector<float>& values,
        Colour colour,
        float zoom,
        bool fillBackground = true) {
    if (values.empty()) {
        return;
    }

    Path trace;
    for (size_t index = 0; index < values.size(); ++index) {
        const float unit = values.size() > 1
                ? (float) index / (float) (values.size() - 1)
                : 0.f;
        const float value = jlimit(0.f, 1.f, values[index]);
        const Point<float> point(
                area.getX() + unit * area.getWidth(),
                area.getBottom() - value * area.getHeight());

        if (index == 0) {
            trace.startNewSubPath(point);
        } else {
            trace.lineTo(point);
        }
    }

    if (fillBackground) {
        graphics.setColour(colour.withAlpha(0.12f));
        graphics.fillRect(area);
    }

    graphics.setColour(colour.withAlpha(0.92f));
    graphics.strokePath(
            trace,
            PathStrokeType(2.f * zoom, PathStrokeType::curved, PathStrokeType::rounded));
}


void drawMeters(
        Graphics& graphics,
        Rectangle<float> area,
        const NodePreviewResult& preview,
        Colour colour) {
    const float left = preview.primary.empty()
            ? 0.f
            : jlimit(0.f, 1.f, preview.primary.front());
    const float right = preview.secondary.empty()
            ? left
            : jlimit(0.f, 1.f, preview.secondary.front());
    Rectangle<float> meterArea = area.reduced(
            area.getWidth() * 0.20f,
            area.getHeight() * 0.08f);
    const float width = meterArea.getWidth() * 0.28f;
    const std::array<std::pair<Rectangle<float>, float>, 2> meters {{
            { meterArea.removeFromLeft(width), left },
            { meterArea.removeFromRight(width), right }
    }};

    for (const auto& meter : meters) {
        constexpr int segments = 12;
        const float gap = jmax(1.f, meter.first.getHeight() * 0.015f);
        const float segmentHeight = (meter.first.getHeight() - gap * (float) (segments - 1))
                / (float) segments;
        const int litSegments = jlimit(0, segments, roundToInt(meter.second * (float) segments));

        for (int index = 0; index < segments; ++index) {
            const int levelIndex = segments - 1 - index;
            const Rectangle<float> segment(
                    meter.first.getX(),
                    meter.first.getY() + (float) index * (segmentHeight + gap),
                    meter.first.getWidth(),
                    segmentHeight);
            const float normalized = (float) levelIndex / (float) (segments - 1);
            Colour segmentColour = colour;

            if (normalized > 0.78f) {
                segmentColour = Colour(0xffff705f);
            } else if (normalized > 0.58f) {
                segmentColour = Colour(0xfff4d35e);
            }

            const bool lit = levelIndex < litSegments;
            graphics.setColour(segmentColour.withAlpha(lit ? 0.82f : 0.14f));
            graphics.fillRoundedRectangle(segment, 1.4f);
        }
    }
}

std::vector<float> mappedSurface(
        const NodePreviewResult& preview,
        const std::vector<float>& values,
        const TrimeshRenderProfile& profile) {
    std::vector<float> surface = values;
    if (surface.empty()) {
        return surface;
    }

    const bool meshSurface = preview.role == PreviewModuleRole::MeshSurface;
    const bool spectral = preview.domain == PortDomain::SpectralMagnitudeSignal
            || preview.domain == PortDomain::SpectralPhaseSignal;
    if (meshSurface && spectral) {
        return profile.mapGridToDisplay(
                surface,
                preview.gridColumns,
                preview.gridRows,
                preview.frequencyMidiNote);
    }
    if (preview.domain == PortDomain::SpectralMagnitudeSignal) {
        return profile.mapSpectrum2DGridToDisplay(
                surface,
                preview.gridColumns,
                preview.gridRows,
                preview.frequencyMidiNote);
    } else if (preview.domain == PortDomain::SpectralPhaseSignal) {
        return profile.mapSpectrum2DGridToDisplay(
                surface,
                preview.gridColumns,
                preview.gridRows,
                preview.frequencyMidiNote);
    }

    Buffer<float> buffer(surface.data(), (int) surface.size());
    if (meshSurface) {
        profile.mapValuesToDisplay(buffer);
    } else if (preview.role == PreviewModuleRole::SignalSpy
            && preview.domain == PortDomain::TimeSignal) {
        std::vector<float> magnitude = surface;
        Buffer<float>(magnitude.data(), (int) magnitude.size())
                .abs()
                .clip(0.f, 1.f)
                .mul(31.f)
                .add(1.f)
                .ln()
                .mul(kSignedLogDisplayScale);
        for (size_t index = 0; index < surface.size(); ++index) {
            surface[index] = values[index] < 0.f
                    ? 0.5f - magnitude[index]
                    : 0.5f + magnitude[index];
        }
    } else if (preview.domain == PortDomain::TimeSignal) {
        float minimum {};
        float maximum {};
        int minimumIndex {};
        int maximumIndex {};
        buffer.getMin(minimum, minimumIndex);
        buffer.getMax(maximum, maximumIndex);
        const float peak = jmax(-minimum, maximum);
        if (peak > 0.f) {
            buffer.mul(0.48f / peak).add(0.5f).clip(0.f, 1.f);
        } else {
            buffer.set(0.5f);
        }
    } else {
        buffer.clip(0.f, 1.f);
    }

    return surface;
}

bool drawHeatmapImage(
        Graphics& graphics,
        Rectangle<float> area,
        const Image& image,
        bool highQuality = false) {
    if (!image.isValid()) {
        return false;
    }

    const Rectangle<float> content = area.reduced(
            jmin(area.getWidth(), area.getHeight()) * 0.024f);
    graphics.setImageResamplingQuality(
            highQuality
                    ? Graphics::highResamplingQuality
                    : Graphics::mediumResamplingQuality);
    graphics.setColour(EffectPlotPalette::insetBackground);
    graphics.fillRect(content);
    graphics.drawImage(image, content);
    return true;
}

bool drawHeatmap(
        Graphics& graphics,
        Rectangle<float> area,
        const NodePreviewResult& preview,
        const TrimeshRenderProfile& profile,
        bool highQuality = false) {
    return drawHeatmapImage(
            graphics,
            area,
            NodePreviewRenderer::createRuntimeHeatmapImage(preview, profile),
            highQuality);
}

void drawCurveFallback(
        Graphics& graphics,
        Rectangle<float> area,
        NodeKind kind,
        const std::vector<CurvePreviewVertex>& vertices,
        float zoom) {
    const Colour line { 0xffe2e8ef };
    const Colour dim { 0xff8b95a3 };
    Rectangle<float> graph = area.reduced(8.f, 7.f);

    if (kind == NodeKind::Waveshaper) {
        const float size = jmin(graph.getWidth(), graph.getHeight());
        graph = Rectangle<float>(size, size).withCentre(graph.getCentre());
    }

    graphics.setColour(Colour(0xff0d1117).withAlpha(0.34f));
    graphics.fillRect(graph);

    float maximumX = 1.f;
    if (kind == NodeKind::Envelope) {
        for (const auto& vertex : vertices) {
            maximumX = jmax(maximumX, vertex.x);
        }
    }

    Path curve;
    for (size_t index = 0; index < vertices.size(); ++index) {
        const auto& vertex = vertices[index];
        const Point<float> point(
                graph.getX() + graph.getWidth() * vertex.x / maximumX,
                graph.getBottom() - graph.getHeight() * vertex.y);
        if (index == 0) {
            curve.startNewSubPath(point);
        } else {
            curve.lineTo(point);
        }
    }

    Rectangle<float> frame = graph;
    if (kind == NodeKind::Waveshaper) {
        frame = graph.reduced(graph.getWidth() * 0.125f, graph.getHeight() * 0.125f);
    } else if (kind == NodeKind::ImpulseResponse) {
        frame = graph.withTrimmedLeft(graph.getWidth() * 0.0625f);
    }

    graphics.setColour(dim.withAlpha(0.44f));
    graphics.drawRect(frame, jmax(1.f, zoom));
    graphics.setColour(line.withAlpha(0.88f));
    graphics.strokePath(
            curve,
            PathStrokeType(jmax(1.4f, 2.f * zoom), PathStrokeType::curved, PathStrokeType::rounded));
}

Rectangle<float> fitAspect(Rectangle<float> area, float aspectRatio) {
    if (area.getWidth() <= 0.f || area.getHeight() <= 0.f || aspectRatio <= 0.f) {
        return area;
    }

    if (area.getWidth() / area.getHeight() > aspectRatio) {
        return area.withSizeKeepingCentre(area.getHeight() * aspectRatio, area.getHeight());
    }

    return area.withSizeKeepingCentre(area.getWidth(), area.getWidth() / aspectRatio);
}

float previewStrokeScale(Rectangle<float> icon) {
    return jmax(0.72f, jmin(icon.getWidth() / 150.f, icon.getHeight() / 82.f));
}

Rectangle<float> fftPreviewIconArea(Rectangle<float> area) {
    Rectangle<float> icon = area.reduced(area.getWidth() * 0.04f, area.getHeight() * 0.09f);

    if (icon.getWidth() / icon.getHeight() < 1.65f) {
        return fitAspect(icon, 1.65f);
    }

    return icon;
}

void drawFftSquareCycle(Graphics& graphics, Rectangle<float> area, float strokeScale) {
    Path path;
    const float left = area.getX() + area.getWidth() * 0.10f;
    const float middle = area.getCentreX();
    const float right = area.getRight() - area.getWidth() * 0.10f;
    const float top = area.getY() + area.getHeight() * 0.20f;
    const float bottom = area.getY() + area.getHeight() * 0.80f;

    path.startNewSubPath(left, bottom);
    path.lineTo(left, top);
    path.lineTo(middle, top);
    path.lineTo(middle, bottom);
    path.lineTo(right, bottom);
    path.lineTo(right, top);

    graphics.setColour(Colour(0xff58d4e8));
    graphics.strokePath(
            path,
            PathStrokeType(2.55f * strokeScale, PathStrokeType::mitered, PathStrokeType::rounded));
}

void drawFftHarmonicStack(Graphics& graphics, Rectangle<float> area, float strokeScale) {
    const struct Partial {
        int harmonic;
        float minimumWidth;
        Colour colour;
    } partials[] = {
            { 7, 116.f, Colour(0xff9b6dff) },
            { 5, 0.f, Colour(0xff6f8cff) },
            { 3, 0.f, Colour(0xff49bde2) },
            { 1, 0.f, Colour(0xff58d4e8) }
    };

    Rectangle<float> waveArea = area.reduced(area.getWidth() * 0.08f, area.getHeight() * 0.10f);
    constexpr float sineControl = 0.3642f;

    for (const auto& partial : partials) {
        if (area.getWidth() < partial.minimumWidth) {
            continue;
        }

        const int halfCycles = partial.harmonic * 2;
        const float halfWidth = waveArea.getWidth() / (float) halfCycles;
        const float amplitude = waveArea.getHeight() * 0.44f / (float) partial.harmonic;
        Path path;

        path.startNewSubPath(waveArea.getX(), waveArea.getCentreY());

        for (int index = 0; index < halfCycles; ++index) {
            const float x0 = waveArea.getX() + (float) index * halfWidth;
            const float x1 = x0 + halfWidth;
            const float controlY = waveArea.getCentreY()
                    + (index % 2 == 0 ? -amplitude : amplitude);

            path.cubicTo(
                    x0 + halfWidth * sineControl,
                    controlY,
                    x1 - halfWidth * sineControl,
                    controlY,
                    x1,
                    waveArea.getCentreY());
        }

        graphics.setColour(partial.colour.withAlpha(0.90f));
        graphics.strokePath(
                path,
                PathStrokeType(1.45f * strokeScale, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

void drawFftChevron(Graphics& graphics, Rectangle<float> icon, float strokeScale) {
    Path path;
    const Point<float> top(
            icon.getX() + icon.getWidth() * 0.476f,
            icon.getY() + icon.getHeight() * 0.39f);
    const Point<float> middle(
            icon.getX() + icon.getWidth() * 0.512f,
            icon.getY() + icon.getHeight() * 0.50f);
    const Point<float> bottom(
            icon.getX() + icon.getWidth() * 0.476f,
            icon.getY() + icon.getHeight() * 0.61f);

    path.startNewSubPath(top);
    path.lineTo(middle);
    path.lineTo(bottom);

    graphics.setColour(Colour(0xff596a78));
    graphics.strokePath(
            path,
            PathStrokeType(2.f * strokeScale, PathStrokeType::mitered, PathStrokeType::rounded));
}

void drawFftTransformPreview(Graphics& graphics, Rectangle<float> area, bool inverse) {
    const Rectangle<float> icon = fftPreviewIconArea(area);
    const float strokeScale = previewStrokeScale(icon);
    const Rectangle<float> left(
            icon.getX() + icon.getWidth() * 0.045f,
            icon.getY() + icon.getHeight() * 0.14f,
            icon.getWidth() * 0.405f,
            icon.getHeight() * 0.72f);
    const Rectangle<float> right(
            icon.getX() + icon.getWidth() * 0.55f,
            icon.getY() + icon.getHeight() * 0.14f,
            icon.getWidth() * 0.405f,
            icon.getHeight() * 0.72f);

    if (inverse) {
        drawFftHarmonicStack(graphics, left, strokeScale);
        drawFftChevron(graphics, icon, strokeScale);
        drawFftSquareCycle(graphics, right, strokeScale);
    } else {
        drawFftSquareCycle(graphics, left, strokeScale);
        drawFftChevron(graphics, icon, strokeScale);
        drawFftHarmonicStack(graphics, right, strokeScale);
    }
}

void drawMathOperationPreview(
        Graphics& graphics,
        Rectangle<float> area,
        bool multiply,
        float zoom) {
    const Colour colour = colourForDomain(PortDomain::ControlSignal);
    const Rectangle<float> icon = fitAspect(
            area.reduced(area.getWidth() * 0.20f, area.getHeight() * 0.12f),
            1.f);
    const Point<float> centre = icon.getCentre();
    const float radius = jmin(icon.getWidth(), icon.getHeight()) * 0.32f;
    const float stroke = jmax(2.3f * zoom, radius * 0.18f);
    Path mark;

    if (multiply) {
        mark.startNewSubPath(centre.x - radius, centre.y - radius);
        mark.lineTo(centre.x + radius, centre.y + radius);
        mark.startNewSubPath(centre.x + radius, centre.y - radius);
        mark.lineTo(centre.x - radius, centre.y + radius);
    } else {
        mark.startNewSubPath(centre.x - radius, centre.y);
        mark.lineTo(centre.x + radius, centre.y);
        mark.startNewSubPath(centre.x, centre.y - radius);
        mark.lineTo(centre.x, centre.y + radius);
    }

    graphics.setColour(Colour(0xff071015).withAlpha(0.46f));
    graphics.strokePath(
            mark,
            PathStrokeType(
                    stroke + 2.f * zoom,
                    PathStrokeType::mitered,
                    PathStrokeType::rounded));
    graphics.setColour(colour.withAlpha(0.94f));
    graphics.strokePath(
            mark,
            PathStrokeType(stroke, PathStrokeType::mitered, PathStrokeType::rounded));
}

void drawSpectralLayerPreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        PortDomain domain) {
    const float pan = jlimit(0.f, 1.f, NodeParameterMap(node).floatValue("pan", 0.5f));
    const float diameter = jmin(area.getWidth(), area.getHeight());
    const Rectangle<float> dial(diameter, diameter);
    const Rectangle<float> bounds = dial.withCentre(area.getCentre());
    const Point<float> centre = bounds.getCentre();
    const float stroke = jmax(1.f, diameter * 0.055f);
    const float radius = diameter * 0.31f;
    const float angle = MathConstants<float>::pi * (-0.75f + pan * 1.5f);
    const Point<float> indicator {
            centre.x + std::sin(angle) * radius,
            centre.y - std::cos(angle) * radius
    };
    const Colour colour = colourForDomain(domain);

    graphics.setColour(Colour(0xff11171d));
    graphics.fillEllipse(bounds);
    graphics.setColour(colour.withAlpha(0.88f));
    graphics.drawEllipse(bounds.reduced(stroke * 0.5f), stroke);
    graphics.drawLine(Line<float>(centre, indicator), stroke);
    graphics.fillEllipse(Rectangle<float>(stroke * 1.8f, stroke * 1.8f).withCentre(indicator));
}

}

NodePreviewRenderer::NodePreviewRenderer(NodePreviewResources& resourcesToUse) :
        resources(resourcesToUse) {
}

bool NodePreviewRenderer::requiresCurveModel(NodeKind kind) {
    return kind == NodeKind::Envelope
            || kind == NodeKind::ImpulseResponse
            || kind == NodeKind::Waveshaper;
}

Image NodePreviewRenderer::createRuntimeHeatmapImage(
        const NodePreviewResult& preview,
        bool desaturated) {
    return createRuntimeHeatmapImage(
            preview,
            TrimeshRenderProfile::fromDomain(preview.domain),
            desaturated);
}

Image NodePreviewRenderer::createRuntimeHeatmapImage(
        const NodePreviewResult& preview,
        const TrimeshRenderProfile& profile,
        bool desaturated) {
    const auto createImage = [&preview, &profile](const std::vector<float>& values) {
        TrimeshRenderData data;
        data.surface = mappedSurface(preview, values, profile);
        data.domain = preview.domain;
        data.columns = (int) preview.gridColumns;
        data.rows = (int) preview.gridRows;
        data.cyclic = preview.domain == PortDomain::TimeSignal;
        return TrimeshSurfaceRenderer::createHeatmapImage(
                data,
                profile);
    };

    Image image = createImage(preview.primary);
    if (desaturated) {
        image.desaturate();
    }
    return image;
}

Rectangle<float> NodePreviewRenderer::boundsFor(
        const Node& node,
        Rectangle<float> nodeBounds,
        float zoom) const {
    Rectangle<float> preview = nodeBounds.withTrimmedTop(42.f * zoom).reduced(8.f * zoom);

    if (node.kind == NodeKind::Fft || node.kind == NodeKind::Ifft) {
        return nodeBounds.withTrimmedTop(40.f * zoom).reduced(3.f * zoom, 5.f * zoom);
    }

    if (node.kind == NodeKind::Unison) {
        return nodeBounds.withTrimmedTop(42.f * zoom).reduced(3.f * zoom);
    }

    if (node.kind == NodeKind::Waveshaper) {
        const float size = jmin(preview.getWidth(), preview.getHeight());
        return Rectangle<float>(size, size).withCentre(preview.getCentre());
    }

    return preview;
}

void NodePreviewRenderer::paint(Graphics& graphics, const NodePreviewRenderRequest& request) {
    const float minimumDimension = request.node.kind == NodeKind::SpectralLayer ? 8.f : 20.f;
    if (request.area.getWidth() < minimumDimension || request.area.getHeight() < minimumDimension) {
        return;
    }

    if (request.node.kind == NodeKind::TrilinearMesh
            && paintAuthoritativeModel(graphics, request)) {
        return;
    }

    if (request.runtimeResult != nullptr
            && (request.runtimeResult->role == PreviewModuleRole::SignalSpy
                    || request.runtimeResult->role == PreviewModuleRole::MeshSurface)
            && request.cache
            && paintCachedHeatmap(graphics, request)) {
        return;
    }

    if (paintAuthoritativeModel(graphics, request)) {
        return;
    }

    if (request.runtimeResult != nullptr
            && request.runtimeResult->role == PreviewModuleRole::EqualizerResponse) {
        paintUncached(graphics, request);
        return;
    }

    if (!request.cache) {
        paintUncached(graphics, request);
        return;
    }

    const int width = roundToInt(request.area.getWidth());
    const int height = roundToInt(request.area.getHeight());
    CachedNodePreviewSprite& cached = resources.cachedSprite(request.node.id);
    String signature = nodeSignature(request.node, request.profile.getDomain());
    if (request.node.kind == NodeKind::TrilinearMesh) {
        signature += "|guide:" + resources.trimeshWidget(request.node).guideContextKey();
    }
    if (request.node.kind == NodeKind::Unison) {
        signature += "|previewNote:" + String(request.unisonContext.midiNote)
                + "|voiceDuration:" + String(request.unisonContext.voiceDurationSeconds, 6);
    }
    if (request.runtimeResult != nullptr) {
        signature += "|runtime:" + runtimeSignature(*request.runtimeResult);
    }

    if (!cached.image.isValid()
            || cached.width != width
            || cached.height != height
            || cached.domain != request.profile.getDomain()
            || cached.scalePolicy != request.profile.getScalePolicy()
            || cached.signature != signature) {
        cached.image = Image(Image::ARGB, width, height, true);
        cached.width = width;
        cached.height = height;
        cached.domain = request.profile.getDomain();
        cached.scalePolicy = request.profile.getScalePolicy();
        cached.signature = signature;
        Graphics sprite(cached.image);
        NodePreviewRenderRequest localRequest = request;
        localRequest.area = { 0.f, 0.f, (float) width, (float) height };
        localRequest.cache = false;
        paintUncached(sprite, localRequest);
    }

    graphics.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    graphics.drawImage(cached.image, request.area);
}

bool NodePreviewRenderer::renderOpenGL(
        const Node& node,
        Rectangle<float> area,
        float scaleFactor) {
    if (!requiresCurveModel(node.kind)) {
        return false;
    }

    resources.curveEditorWidget(node).renderPreviewSnapshotOpenGL(node, area, scaleFactor);
    return true;
}

uint64_t NodePreviewRenderer::nodePresentationFingerprint(const String& nodeId) const {
    return resources.nodePresentationFingerprint(nodeId);
}

bool NodePreviewRenderer::paintAuthoritativeModel(
        Graphics& graphics,
        const NodePreviewRenderRequest& request) {
    if (request.node.kind == NodeKind::TrilinearMesh) {
        resources.trimeshWidget(request.node).paintCompact(
                graphics,
                request.node,
                request.area,
                request.zoom,
                request.profile);
        return true;
    }

    if (!requiresCurveModel(request.node.kind)) {
        return false;
    }

    CurveEditorWidget& widget = resources.curveEditorWidget(request.node);
    if (!widget.paintPreviewSnapshot(graphics, request.area)) {
        drawCurveFallback(
                graphics,
                request.area,
                request.node.kind,
                widget.previewVertices(),
                request.zoom);
    }

    return true;
}

bool NodePreviewRenderer::paintRuntimeResult(
        Graphics& graphics,
        const NodePreviewRenderRequest& request) {
    if (request.runtimeResult == nullptr) {
        return false;
    }

    const NodePreviewResult& result = *request.runtimeResult;
    const Colour colour = previewColourForRole(result.role, request.node);
    if (result.role == PreviewModuleRole::OutputMeters) {
        drawMeters(graphics, request.area, result, colour);
        return true;
    }

    if (result.role == PreviewModuleRole::SignalSpy
            || result.role == PreviewModuleRole::MeshSurface) {
        return drawHeatmap(
                graphics,
                request.area,
                result,
                request.profile,
                request.highQuality);
    }

    if (result.role == PreviewModuleRole::ReverbSpectrogram) {
        return paintRuntimeHeatmap(graphics, request);
    }

    if (result.role == PreviewModuleRole::EqualizerResponse) {
        const Rectangle<float> background = request.area.reduced(
                jmin(request.area.getWidth(), request.area.getHeight()) * 0.04f);
        graphics.setColour(EffectPlotPalette::forEnabledState(
                EffectPlotPalette::insetBackground,
                NodeParameterMap(request.node).boolValue("enabled", true)));
        graphics.fillRoundedRectangle(background, 4.f);
        EqualizerPreviewPainter().paintResponse(
                graphics,
                background.reduced(8.f, 6.f),
                request.node,
                result.primary,
                true);
        return true;
    }

    if (result.primary.empty()) {
        return false;
    }

    const Rectangle<float> content = previewContentArea(request.area);
    drawTrace(graphics, content, result.primary, colour, request.zoom);
    if (!result.secondary.empty()) {
        drawTrace(
                graphics,
                content,
                result.secondary,
                colour.withAlpha(0.58f),
                request.zoom);
    }

    return true;
}

bool NodePreviewRenderer::paintRuntimeHeatmap(
        Graphics& graphics,
        const NodePreviewRenderRequest& request) {
    if (request.runtimeResult == nullptr) {
        return false;
    }

    const bool desaturated = request.runtimeResult->role == PreviewModuleRole::ReverbSpectrogram
            && !NodeParameterMap(request.node).boolValue("enabled", true);
    const String signature = runtimeSignature(*request.runtimeResult)
            + "|desaturated:" + String(desaturated ? 1 : 0)
            + "|scale:" + String((int) request.profile.getScalePolicy());
    CachedNodePreviewSprite& cached = resources.cachedSprite(request.node.id);
    if (!cached.runtimeHeatmap.isValid()
            || cached.runtimeHeatmapSignature != signature) {
        cached.runtimeHeatmap = createRuntimeHeatmapImage(
                *request.runtimeResult,
                request.profile,
                desaturated);
        cached.runtimeHeatmapSignature = signature;
    }

    return drawHeatmapImage(
            graphics,
            request.area,
            cached.runtimeHeatmap,
            request.highQuality);
}

void NodePreviewRenderer::paintUncached(
        Graphics& graphics,
        const NodePreviewRenderRequest& request) {
    if (paintRuntimeResult(graphics, request)) {
        return;
    }

    paintQualitative(graphics, request);
}

bool NodePreviewRenderer::paintCachedHeatmap(
        Graphics& graphics,
        const NodePreviewRenderRequest& request) {
    const NodePreviewResult& result = *request.runtimeResult;
    const int width = roundToInt(request.area.getWidth());
    const int height = roundToInt(request.area.getHeight());
    CachedNodePreviewSprite& cached = resources.cachedSprite(request.node.id);
    const String signature = runtimeSignature(result);

    if (!cached.image.isValid()
            || cached.width != width
            || cached.height != height
            || cached.domain != result.domain
            || cached.scalePolicy != request.profile.getScalePolicy()
            || cached.signature != signature) {
        cached.image = Image(Image::ARGB, width, height, true);
        cached.width = width;
        cached.height = height;
        cached.domain = result.domain;
        cached.scalePolicy = request.profile.getScalePolicy();
        cached.signature = signature;
        Graphics sprite(cached.image);
        if (!drawHeatmap(
                sprite,
                { 0.f, 0.f, (float) width, (float) height },
                result,
                request.profile,
                request.highQuality)) {
            cached.image = {};
            return false;
        }
    }

    graphics.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    graphics.drawImage(cached.image, request.area);
    return true;
}

void NodePreviewRenderer::paintQualitative(
        Graphics& graphics,
        const NodePreviewRenderRequest& request) {
    const NodeKind kind = request.node.kind;
    if (kind == NodeKind::Unison) {
        Node displayNode = request.node;
        if (displayNode.id.isEmpty()) {
            for (NodeParameter& parameter : displayNode.parameters) {
                if (parameter.id == "order") {
                    parameter.value = "5";
                }
            }
        }
        UnisonPreviewPainter().paint(
                graphics,
                request.area,
                displayNode,
                request.zoom,
                request.unisonContext);
        return;
    }
    if (kind == NodeKind::Reverb) {
        ReverbPreviewPainter().paint(graphics, request.area, request.node, request.zoom);
        return;
    }
    if (kind == NodeKind::Delay) {
        DelayPreviewPainter().paint(graphics, request.area, request.node, request.zoom);
        return;
    }
    if (kind == NodeKind::Equalizer) {
        Node displayNode = request.node;
        if (displayNode.id.isEmpty()) {
            for (NodeParameter& parameter : displayNode.parameters) {
                if (parameter.id == "band1Gain" || parameter.id == "band5Gain") {
                    parameter.value = "0.68";
                } else if (parameter.id == "band3Gain") {
                    parameter.value = "0.32";
                }
            }
        }
        EqualizerPreviewPainter().paint(
                graphics,
                request.area.reduced(2.f),
                displayNode,
                false);
        return;
    }

    if (kind == NodeKind::Fft || kind == NodeKind::Ifft) {
        drawFftTransformPreview(graphics, request.area, kind == NodeKind::Ifft);
        return;
    }

    if (kind == NodeKind::Add || kind == NodeKind::Multiply) {
        drawMathOperationPreview(graphics, request.area, kind == NodeKind::Multiply, request.zoom);
        return;
    }

    if (kind == NodeKind::SpectralLayer) {
        drawSpectralLayerPreview(
                graphics,
                request.area,
                request.node,
                request.profile.getDomain());
        return;
    }

    if (kind == NodeKind::Output) {
        const NodePreviewResult meters {
                request.node.id,
                PreviewModuleRole::OutputMeters,
                { 0.64f },
                { 0.58f }
        };
        drawMeters(
                graphics,
                request.area,
                meters,
                colourForDomain(PortDomain::TimeSignal));
        return;
    }

    if (kind == NodeKind::StereoSplit || kind == NodeKind::StereoJoin) {
        const bool split = kind == NodeKind::StereoSplit;
        const float y = request.area.getCentreY();
        const float left = request.area.getX() + request.area.getWidth() * 0.28f;
        const float right = request.area.getRight() - request.area.getWidth() * 0.28f;
        graphics.setColour(colourForDomain(PortDomain::TimeSignal).withAlpha(0.85f));
        graphics.drawLine(Line<float>({ left, y }, { right, y - request.area.getHeight() * 0.18f }), 2.f);
        graphics.drawLine(Line<float>({ left, y }, { right, y + request.area.getHeight() * 0.18f }), 2.f);
        graphics.setColour(kMutedText.withAlpha(0.72f));
        graphics.drawText(split ? "SPLIT" : "JOIN", request.area, Justification::centredBottom);
        return;
    }

    if (kind == NodeKind::ImageSource) {
        auto imageArea = request.area.reduced(
                request.area.getWidth() * 0.12f,
                request.area.getHeight() * 0.16f);
        graphics.setColour(colourForDomain(PortDomain::ControlSignal).withAlpha(0.12f));
        graphics.fillRect(imageArea);
        graphics.setColour(colourForDomain(PortDomain::TimeSignal).withAlpha(0.42f));
        graphics.fillRect(imageArea.removeFromLeft(imageArea.getWidth() * 0.46f).reduced(1.f));
        graphics.setColour(colourForDomain(PortDomain::SpectralMagnitudeSignal).withAlpha(0.36f));
        graphics.fillRect(imageArea.removeFromTop(imageArea.getHeight() * 0.48f).reduced(1.f));
        graphics.setColour(colourForDomain(PortDomain::SpectralPhaseSignal).withAlpha(0.34f));
        graphics.fillRect(imageArea.reduced(1.f));
        return;
    }

    Path curve;
    constexpr int steps = 42;
    for (int index = 0; index < steps; ++index) {
        const float unit = (float) index / (float) (steps - 1);
        const float phase = kind == NodeKind::WaveSource
                ? unit * MathConstants<float>::twoPi * 1.25f
                : unit * MathConstants<float>::twoPi * 1.35f
                        + request.node.id.hashCode() * 0.001f;
        const float value = kind == NodeKind::WaveSource
                ? 0.5f + fastSin(phase) * 0.34f
                : 0.5f + fastSin(phase) * 0.28f;
        const Point<float> point(
                request.area.getX() + unit * request.area.getWidth(),
                request.area.getY() + value * request.area.getHeight());

        if (index == 0) {
            curve.startNewSubPath(point);
        } else {
            curve.lineTo(point);
        }
    }

    const Colour colour = request.node.outputs.empty()
            ? Colour(0xff9aa5b2)
            : colourForDomain(request.node.outputs.front().domain);
    graphics.setColour(colour.withAlpha(kind == NodeKind::WaveSource ? 0.12f : 0.20f));
    graphics.fillRect(request.area);
    graphics.setColour(colour.withAlpha(0.95f));
    graphics.strokePath(
            curve,
            PathStrokeType(2.f * request.zoom, PathStrokeType::curved, PathStrokeType::rounded));
}

}
