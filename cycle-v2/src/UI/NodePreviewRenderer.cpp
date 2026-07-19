#include "NodePreviewRenderer.h"

#include "NodeParameterValue.h"
#include "../Graph/GraphRenderSemanticResolver.h"
#include "../Nodes/Effects/EffectPreviewRenderer.h"
#include "../Nodes/Trimesh/TrimeshSurfaceRenderer.h"

#include <Util/Arithmetic.h>

#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace CycleV2 {

namespace {

const Colour kMutedText { 0xff8793a1 };
constexpr float kSpectralFrequencyTension = 50.f;

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
            return colourForDomain(PortDomain::TimeSignal);
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

void remapSpectralRows(std::vector<float>& surface, size_t columns, size_t rows) {
    if (columns == 0 || rows < 2 || surface.size() < columns * rows) {
        return;
    }

    const std::vector<float> source = surface;
    std::vector<float> rowMap(rows);
    Buffer<float> mappedRows(rowMap.data(), (int) rowMap.size());
    mappedRows.ramp(0.f, 1.f / (float) (rowMap.size() - 1));
    Arithmetic::applyInvLogMapping(mappedRows, kSpectralFrequencyTension);

    for (size_t column = 0; column < columns; ++column) {
        const size_t columnOffset = column * rows;
        for (size_t row = 0; row < rows; ++row) {
            const float position = jlimit(
                    0.f,
                    (float) (rows - 1),
                    rowMap[row] * (float) (rows - 1));
            const size_t rowA = (size_t) position;
            const size_t rowB = std::min(rowA + 1, rows - 1);
            const float amount = position - (float) rowA;
            surface[columnOffset + row] = source[columnOffset + rowA]
                    + amount * (source[columnOffset + rowB] - source[columnOffset + rowA]);
        }
    }
}

void unwrapPhase(std::vector<float>& surface, size_t columns, size_t rows) {
    if (columns < 2 || rows == 0 || surface.size() < columns * rows) {
        return;
    }

    for (size_t row = 0; row < rows; ++row) {
        float offset = 0.f;
        float previous = surface[row];
        for (size_t column = 1; column < columns; ++column) {
            const size_t index = column * rows + row;
            const float current = surface[index];
            const float delta = current + offset - previous;

            if (delta > MathConstants<float>::pi) {
                offset -= MathConstants<float>::twoPi;
            } else if (delta < -MathConstants<float>::pi) {
                offset += MathConstants<float>::twoPi;
            }

            surface[index] = current + offset;
            previous = surface[index];
        }
    }
}

std::vector<float> mappedSurface(const NodePreviewResult& preview) {
    std::vector<float> surface = preview.primary;
    if (surface.empty()) {
        return surface;
    }

    Buffer<float> buffer(surface.data(), (int) surface.size());
    if (preview.domain == PortDomain::SpectralMagnitudeSignal) {
        remapSpectralRows(surface, preview.gridColumns, preview.gridRows);
        buffer.abs().mul(16.f).add(1.f).ln().mul(1.f / 2.833213344f).clip(0.f, 1.f);
    } else if (preview.domain == PortDomain::SpectralPhaseSignal) {
        unwrapPhase(surface, preview.gridColumns, preview.gridRows);
        remapSpectralRows(surface, preview.gridColumns, preview.gridRows);
        float minimum = std::numeric_limits<float>::max();
        float maximum = std::numeric_limits<float>::lowest();
        int minimumIndex {};
        int maximumIndex {};
        buffer.getMin(minimum, minimumIndex);
        buffer.getMax(maximum, maximumIndex);
        const float realMaximum = jmax(std::abs(minimum), std::abs(maximum));

        if (realMaximum <= 0.f) {
            buffer.set(0.5f);
        } else {
            const float exponent = std::ceil(std::log2(realMaximum) + 0.5f);
            buffer.mul(std::pow(2.f, -exponent)).add(0.5f).clip(0.f, 1.f);
        }
    } else if (preview.domain == PortDomain::TimeSignal) {
        buffer.mul(0.5f).add(0.5f).clip(0.f, 1.f);
    } else {
        buffer.clip(0.f, 1.f);
    }

    return surface;
}

bool drawHeatmapImage(Graphics& graphics, Rectangle<float> area, const Image& image) {
    if (!image.isValid()) {
        return false;
    }

    const Rectangle<float> content = area.reduced(
            jmin(area.getWidth(), area.getHeight()) * 0.024f);
    graphics.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    graphics.setColour(Colour(0xff07090d));
    graphics.fillRect(content);
    graphics.drawImage(image, content);
    return true;
}

bool drawHeatmap(Graphics& graphics, Rectangle<float> area, const NodePreviewResult& preview) {
    return drawHeatmapImage(
            graphics,
            area,
            NodePreviewRenderer::createRuntimeHeatmapImage(preview));
}

void drawEffect2DFallback(
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
    } else if (kind == NodeKind::GuideCurve) {
        frame = graph.withTrimmedLeft(graph.getWidth() * 0.05f)
                .withTrimmedRight(graph.getWidth() * 0.05f);
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

}

NodePreviewRenderer::NodePreviewRenderer(NodePreviewResources& resourcesToUse) :
        resources(resourcesToUse) {
}

bool NodePreviewRenderer::requiresEffect2DModel(NodeKind kind) {
    return kind == NodeKind::Envelope
            || kind == NodeKind::GuideCurve
            || kind == NodeKind::ImpulseResponse
            || kind == NodeKind::Waveshaper;
}

Image NodePreviewRenderer::createRuntimeHeatmapImage(
        const NodePreviewResult& preview,
        bool desaturated) {
    TrimeshRenderData data;
    data.surface = mappedSurface(preview);
    data.domain = preview.domain;
    data.columns = (int) preview.gridColumns;
    data.rows = (int) preview.gridRows;
    data.cyclic = preview.domain == PortDomain::TimeSignal;

    Image image = TrimeshSurfaceRenderer::createHeatmapImage(
            data,
            TrimeshRenderProfile::fromDomain(preview.domain));
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

    if (node.kind == NodeKind::Waveshaper) {
        const float size = jmin(preview.getWidth(), preview.getHeight());
        return Rectangle<float>(size, size).withCentre(preview.getCentre());
    }

    return preview;
}

void NodePreviewRenderer::paint(Graphics& graphics, const NodePreviewRenderRequest& request) {
    if (request.area.getWidth() < 20.f || request.area.getHeight() < 20.f) {
        return;
    }

    if (paintAuthoritativeModel(graphics, request)) {
        return;
    }

    if (request.runtimeResult != nullptr
            && request.runtimeResult->role == PreviewModuleRole::SignalSpy
            && request.cache
            && paintCachedHeatmap(graphics, request)) {
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
    if (request.runtimeResult != nullptr) {
        signature += "|runtime:" + runtimeSignature(*request.runtimeResult);
    }

    if (!cached.image.isValid()
            || cached.width != width
            || cached.height != height
            || cached.domain != request.profile.getDomain()
            || cached.signature != signature) {
        cached.image = Image(Image::ARGB, width, height, true);
        cached.width = width;
        cached.height = height;
        cached.domain = request.profile.getDomain();
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
    if (!requiresEffect2DModel(node.kind)) {
        return false;
    }

    resources.effect2DWidget(node).renderPreviewSnapshotOpenGL(node, area, scaleFactor);
    return true;
}

bool NodePreviewRenderer::paintAuthoritativeModel(
        Graphics& graphics,
        const NodePreviewRenderRequest& request) {
    if (request.node.kind == NodeKind::TrilinearMesh) {
        resources.trimeshWidget(request.node.id).paintCompact(
                graphics,
                request.node,
                request.area,
                request.zoom,
                request.profile);
        return true;
    }

    if (!requiresEffect2DModel(request.node.kind)) {
        return false;
    }

    Effect2DWidget& widget = resources.effect2DWidget(request.node);
    if (!widget.paintPreviewSnapshot(graphics, request.area)) {
        drawEffect2DFallback(
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

    if (result.role == PreviewModuleRole::SignalSpy) {
        return drawHeatmap(graphics, request.area, result);
    }

    if (result.role == PreviewModuleRole::ReverbSpectrogram) {
        return paintRuntimeHeatmap(graphics, request);
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
            && nodeParameterValue(request.node, "enabled", "1").getIntValue() == 0;
    const String signature = runtimeSignature(*request.runtimeResult)
            + "|desaturated:" + String(desaturated ? 1 : 0);
    CachedNodePreviewSprite& cached = resources.cachedSprite(request.node.id);
    if (!cached.runtimeHeatmap.isValid()
            || cached.runtimeHeatmapSignature != signature) {
        cached.runtimeHeatmap = createRuntimeHeatmapImage(
                *request.runtimeResult,
                desaturated);
        cached.runtimeHeatmapSignature = signature;
    }

    return drawHeatmapImage(graphics, request.area, cached.runtimeHeatmap);
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
            || cached.signature != signature) {
        cached.image = Image(Image::ARGB, width, height, true);
        cached.width = width;
        cached.height = height;
        cached.domain = result.domain;
        cached.signature = signature;
        Graphics sprite(cached.image);
        if (!drawHeatmap(sprite, { 0.f, 0.f, (float) width, (float) height }, result)) {
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
    if (paintEffectCompactPreview(graphics, request.area, request.node, request.zoom)) {
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
