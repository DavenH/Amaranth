#include "NodeCanvas.h"
#include "NodeViewModule.h"
#include "TransformCompactEditor.h"
#include "VoiceContextCompactEditor.h"

#include "../Runtime/GraphAudioExecutor.h"

#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>

namespace CycleV2 {

namespace NodeCanvasInvalidation {

constexpr uint32_t CanvasRepaint = 1u << 0;

}

namespace {

const Colour kCanvasBackground { 0xff101318 };
const Colour kCanvasGridMajor  { 0x2f5b6370 };
const Colour kCanvasGridMinor  { 0x182f363f };
const Colour kNodeBackground   { 0xff171d24 };
const Colour kNodeHeader       { 0xff202833 };
const Colour kNodeBorder       { 0xff3d4a58 };
const Colour kText             { 0xffe2e8ef };
const Colour kMutedText        { 0xff8793a1 };
constexpr float kCableReferenceZoom = 0.58f;
constexpr float kExpandedEditorScale = 0.90f;
constexpr float kExpandedEditorMinMargin = 18.f;
constexpr float kExpandedEditorHeaderHeight = 34.f;
constexpr float kNodeSnapBaseDistance = 9.f;
constexpr float kNodeSnapExtraPerMatch = 2.5f;
constexpr float kNodeSnapMaxDistance = 20.f;
constexpr bool kUseGlCanvasUnderlay = true;
constexpr bool kUseGlCanvasEdges = false;
constexpr bool kUseGlNodeShells = false;

struct SnapCandidate {
    float target {};
    int matches { 1 };
};


float portScaleForZoom(float zoom) {
    return zoom / kCableReferenceZoom;
}

float absoluteFloat(float value) {
    return value >= 0.f ? value : -value;
}

float snapDistanceForMatches(int matches) {
    return jmin(kNodeSnapMaxDistance, kNodeSnapBaseDistance + (float) jmax(0, matches - 1) * kNodeSnapExtraPerMatch);
}

float fastSin(float value) {
    return (float) juce::dsp::FastMathApproximations::sin((double) value);
}

float fastCos(float value) {
    return (float) juce::dsp::FastMathApproximations::cos((double) value);
}

int portIndexOnSide(const Node& node, const Port& port) {
    int index = 0;

    auto scan = [&](const std::vector<Port>& ports) {
        for (const auto& candidate : ports) {
            if (candidate.side != port.side) {
                continue;
            }

            if (candidate.id == port.id && candidate.input == port.input) {
                return true;
            }

            ++index;
        }

        return false;
    };

    if (scan(node.inputs)) {
        return index;
    }

    scan(node.outputs);
    return index;
}

int portCountOnSide(const Node& node, PortSide side) {
    int count = 0;

    auto scan = [&](const std::vector<Port>& ports) {
        for (const auto& port : ports) {
            if (port.side == side) {
                ++count;
            }
        }
    };

    scan(node.inputs);
    scan(node.outputs);
    return count;
}

float sidePortY(const Node& node, const Port& port) {
    const auto& ports = port.input ? node.inputs : node.outputs;
    int index = 0;

    for (int i = 0; i < (int) ports.size(); ++i) {
        if (ports[(size_t) i].id == port.id) {
            index = portIndexOnSide(node, port);
            break;
        }
    }

    return node.bounds.getY() + 58.f + (float) index * 34.f;
}

float sidePortYForBounds(const Node& node, const Port& port, Rectangle<float> bounds) {
    return bounds.getY() + sidePortY(node, port) - node.bounds.getY();
}

Point<float> portWorldCentreForBounds(const Node& node, const Port& port, Rectangle<float> bounds) {
    switch (port.side) {
        case PortSide::Top:
        case PortSide::Bottom: {
            const int index = portIndexOnSide(node, port);
            const int count = jmax(1, portCountOnSide(node, port.side));
            const float x = bounds.getX() + bounds.getWidth() * ((float) index + 1.f) / ((float) count + 1.f);
            const float y = port.side == PortSide::Top ? bounds.getY() : bounds.getBottom();
            return { x, y };
        }

        case PortSide::Right:
            return { bounds.getRight(), sidePortYForBounds(node, port, bounds) };

        case PortSide::Left:
        default:
            return { bounds.getX(), sidePortYForBounds(node, port, bounds) };
    }
}

var rectangleToVar(Rectangle<float> bounds) {
    auto* object = new DynamicObject();
    object->setProperty("x", bounds.getX());
    object->setProperty("y", bounds.getY());
    object->setProperty("width", bounds.getWidth());
    object->setProperty("height", bounds.getHeight());
    return object;
}

var componentDiagnosticsToVar(const String& id, const Component* component) {
    auto* object = new DynamicObject();
    object->setProperty("id", id);
    object->setProperty("created", component != nullptr);

    if (component == nullptr) {
        return object;
    }

    object->setProperty("visible", component->isVisible());
    object->setProperty("showing", component->isShowing());
    object->setProperty("enabled", component->isEnabled());
    object->setProperty("bounds", rectangleToVar(component->getBounds().toFloat()));
    object->setProperty("screenBounds", rectangleToVar(component->getScreenBounds().toFloat()));
    object->setProperty("hasParent", component->getParentComponent() != nullptr);
    object->setProperty("width", component->getWidth());
    object->setProperty("height", component->getHeight());
    object->setProperty("nonEmpty", !component->getLocalBounds().isEmpty());
    return object;
}

bool nodeKindForAutomationId(const String& id, NodeKind& kind) {
    if (id == "genericProcessor" || id == "processor") {
        kind = NodeKind::GenericProcessor;
    } else if (id == "voiceContext" || id == "voice") {
        kind = NodeKind::VoiceContext;
    } else if (id == "waveSource" || id == "wave") {
        kind = NodeKind::WaveSource;
    } else if (id == "imageSource" || id == "image") {
        kind = NodeKind::ImageSource;
    } else if (id == "trilinearMesh" || id == "mesh") {
        kind = NodeKind::TrilinearMesh;
    } else if (id == "fft") {
        kind = NodeKind::Fft;
    } else if (id == "ifft") {
        kind = NodeKind::Ifft;
    } else if (id == "envelope" || id == "env") {
        kind = NodeKind::Envelope;
    } else if (id == "add") {
        kind = NodeKind::Add;
    } else if (id == "multiply") {
        kind = NodeKind::Multiply;
    } else if (id == "guideCurve" || id == "guide") {
        kind = NodeKind::GuideCurve;
    } else if (id == "impulseResponse" || id == "ir") {
        kind = NodeKind::ImpulseResponse;
    } else if (id == "waveshaper") {
        kind = NodeKind::Waveshaper;
    } else if (id == "reverb") {
        kind = NodeKind::Reverb;
    } else if (id == "delay") {
        kind = NodeKind::Delay;
    } else if (id == "spy") {
        kind = NodeKind::Spy;
    } else if (id == "stereoSplit" || id == "split") {
        kind = NodeKind::StereoSplit;
    } else if (id == "stereoJoin" || id == "join") {
        kind = NodeKind::StereoJoin;
    } else if (id == "output" || id == "out") {
        kind = NodeKind::Output;
    } else {
        return false;
    }

    return true;
}

void addSnapCandidate(std::vector<SnapCandidate>& candidates, float target) {
    constexpr float mergeDistance = 0.5f;

    for (auto& candidate : candidates) {
        if (absoluteFloat(candidate.target - target) <= mergeDistance) {
            candidate.target = (candidate.target * (float) candidate.matches + target) / (float) (candidate.matches + 1);
            ++candidate.matches;
            return;
        }
    }

    candidates.push_back({ target, 1 });
}

bool containsString(const std::vector<String>& values, const String& value) {
    for (const auto& candidate : values) {
        if (candidate == value) {
            return true;
        }
    }

    return false;
}

void drawPaletteGroupIcon(Graphics& g, const char* title, Rectangle<float> area, bool active) {
    const float stroke = active ? 2.1f : 1.65f;
    const Colour fg = Colour(0xffd2d9e2).withAlpha(active ? 0.96f : 0.76f);
    const Colour dim = kMutedText.withAlpha(active ? 0.68f : 0.44f);
    area = area.reduced(4.f);

    g.setColour(fg);

    if (std::strcmp(title, "Context") == 0) {
        const float y = area.getCentreY();
        const float x0 = area.getX() + area.getWidth() * 0.18f;
        const float x1 = area.getRight() - area.getWidth() * 0.18f;
        g.drawLine(x0, y, x1, y, stroke);

        for (int i = 0; i < 4; ++i) {
            const float t = (float) i / 3.f;
            const float x = x0 + (x1 - x0) * t;
            Rectangle<float> dot(x - 2.2f, y - 2.2f, 4.4f, 4.4f);
            g.fillEllipse(dot);
        }

        g.setColour(dim);
        g.drawEllipse(area.withSizeKeepingCentre(23.f, 23.f), 1.1f);
        return;
    }

    if (std::strcmp(title, "Transform") == 0) {
        Path square;
        const Rectangle<float> left(
                area.getX(),
                area.getY() + area.getHeight() * 0.22f,
                area.getWidth() * 0.34f,
                area.getHeight() * 0.56f);
        const Rectangle<float> right(
                area.getX() + area.getWidth() * 0.40f,
                area.getY() + area.getHeight() * 0.14f,
                area.getWidth() * 0.60f,
                area.getHeight() * 0.72f);
        square.startNewSubPath(left.getX(), left.getBottom());
        square.lineTo(left.getX(), left.getY());
        square.lineTo(left.getCentreX(), left.getY());
        square.lineTo(left.getCentreX(), left.getBottom());
        square.lineTo(left.getRight(), left.getBottom());
        g.strokePath(square, PathStrokeType(stroke, PathStrokeType::mitered, PathStrokeType::rounded));

        Path wave;
        for (int i = 0; i < 32; ++i) {
            const float t = (float) i / 31.f;
            const Point<float> p(
                    right.getX() + t * right.getWidth(),
                    right.getCentreY() + fastSin(t * MathConstants<float>::twoPi * 1.35f) * right.getHeight() * 0.30f);
            if (i == 0) {
                wave.startNewSubPath(p);
            } else {
                wave.lineTo(p);
            }
        }

        g.setColour(dim);
        g.drawLine(left.getRight() + 3.f, left.getCentreY(), right.getX() - 3.f, right.getCentreY(), 1.1f);
        g.setColour(fg);
        g.strokePath(wave, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
        return;
    }

    if (std::strcmp(title, "Math") == 0) {
        const Point<float> left(area.getX() + area.getWidth() * 0.34f, area.getCentreY());
        const Point<float> right(area.getX() + area.getWidth() * 0.68f, area.getCentreY());
        const float r = 6.f;
        g.drawLine(left.x - r, left.y, left.x + r, left.y, stroke);
        g.drawLine(left.x, left.y - r, left.x, left.y + r, stroke);
        g.drawLine(right.x - r, right.y - r, right.x + r, right.y + r, stroke);
        g.drawLine(right.x + r, right.y - r, right.x - r, right.y + r, stroke);
        return;
    }

    if (std::strcmp(title, "Source") == 0) {
        Path wave;
        Rectangle<float> waveArea = area.withTrimmedBottom(area.getHeight() * 0.45f);
        for (int i = 0; i < 20; ++i) {
            const float t = (float) i / 19.f;
            const Point<float> p(
                    waveArea.getX() + t * waveArea.getWidth(),
                    waveArea.getCentreY() + fastSin(t * MathConstants<float>::twoPi * 1.2f) * waveArea.getHeight() * 0.28f);
            if (i == 0) {
                wave.startNewSubPath(p);
            } else {
                wave.lineTo(p);
            }
        }

        g.strokePath(wave, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(dim);
        Rectangle<float> image(area.getX() + 5.f, area.getCentreY() + 3.f, 10.f, 8.f);
        Rectangle<float> mesh(area.getRight() - 17.f, area.getCentreY() + 2.f, 12.f, 10.f);
        g.drawRect(image, 1.f);
        g.drawRoundedRectangle(mesh, 2.f, 1.f);
        return;
    }

    if (std::strcmp(title, "Control") == 0) {
        Rectangle<float> graph = area.reduced(3.f, 6.f);
        Path env;
        env.startNewSubPath(graph.getX(), graph.getBottom());
        env.lineTo(graph.getX() + graph.getWidth() * 0.22f, graph.getY());
        env.lineTo(graph.getX() + graph.getWidth() * 0.58f, graph.getY());
        env.lineTo(graph.getRight(), graph.getBottom());
        g.strokePath(env, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(dim);
        g.drawLine(graph.getX(), graph.getBottom(), graph.getRight(), graph.getBottom(), 1.f);
        return;
    }

    if (std::strcmp(title, "FX") == 0) {
        Rectangle<float> arcArea = area.reduced(3.f);
        for (int i = 0; i < 3; ++i) {
            const float inset = (float) i * 5.f;
            g.setColour(fg.withAlpha(active ? 0.88f - (float) i * 0.18f : 0.62f - (float) i * 0.13f));
            g.drawEllipse(arcArea.reduced(inset), jmax(0.8f, stroke - (float) i * 0.25f));
        }

        g.setColour(dim);
        g.drawLine(area.getX() + 5.f, area.getCentreY(), area.getRight() - 5.f, area.getCentreY(), 1.f);
        return;
    }

    if (std::strcmp(title, "Channel") == 0) {
        const float left = area.getX() + 5.f;
        const float centre = area.getCentreX();
        const float right = area.getRight() - 5.f;
        const float y = area.getCentreY();
        g.drawLine(left, y, centre, y, stroke);
        g.drawLine(centre, y, right, y - 7.f, stroke);
        g.drawLine(centre, y, right, y + 7.f, stroke);
        g.setColour(dim);
        g.drawLine(left, y - 8.f, centre, y - 8.f, 1.f);
        g.drawLine(left, y + 8.f, centre, y + 8.f, 1.f);
    }
}

void addNodeSnapCandidates(
        std::vector<SnapCandidate>& xCandidates,
        std::vector<SnapCandidate>& yCandidates,
        const Node& node) {
    for (const auto& port : node.inputs) {
        const Point<float> centre = portWorldCentreForBounds(node, port, node.bounds);
        addSnapCandidate(xCandidates, centre.x);
        addSnapCandidate(yCandidates, centre.y);
    }

    for (const auto& port : node.outputs) {
        const Point<float> centre = portWorldCentreForBounds(node, port, node.bounds);
        addSnapCandidate(xCandidates, centre.x);
        addSnapCandidate(yCandidates, centre.y);
    }
}

void addDraggedNodeAnchors(
        std::vector<float>& xAnchors,
        std::vector<float>& yAnchors,
        const Node& node,
        Rectangle<float> bounds) {
    for (const auto& port : node.inputs) {
        const Point<float> centre = portWorldCentreForBounds(node, port, bounds);
        xAnchors.push_back(centre.x);
        yAnchors.push_back(centre.y);
    }

    for (const auto& port : node.outputs) {
        const Point<float> centre = portWorldCentreForBounds(node, port, bounds);
        xAnchors.push_back(centre.x);
        yAnchors.push_back(centre.y);
    }
}

bool bestSnapDelta(
        const std::vector<float>& anchors,
        const std::vector<SnapCandidate>& candidates,
        float& delta,
        float& guide) {
    float bestDistance = std::numeric_limits<float>::max();
    int bestMatches = 0;

    for (float anchor : anchors) {
        for (const auto& candidate : candidates) {
            const float distance = candidate.target - anchor;
            const float magnitude = absoluteFloat(distance);
            const float threshold = snapDistanceForMatches(candidate.matches);

            if (magnitude <= threshold
                    && (magnitude < bestDistance
                        || (magnitude == bestDistance && candidate.matches > bestMatches))) {
                bestDistance = magnitude;
                bestMatches = candidate.matches;
                delta = distance;
                guide = candidate.target;
            }
        }
    }

    return bestMatches > 0;
}

String portDisplayLabel(const Port& port) {
    const String channel = labelForChannelLayout(port.channelLayout);
    return channel.isEmpty() ? port.label : port.label + " " + channel;
}

Colour portDisplayColour(const Node& node, const Port& port) {
    if (node.kind == NodeKind::Add || node.kind == NodeKind::Multiply) {
        return colourForDomain(PortDomain::ControlSignal);
    }

    return colourForDomain(port.domain);
}

bool isOperationNode(NodeKind kind) {
    return NodeViewModuleRegistry::instance().moduleFor(kind).capabilities().operationLayoutControl;
}

bool hasOutputSideButton(NodeKind kind) {
    return NodeViewModuleRegistry::instance().moduleFor(kind).capabilities().outputSideControl;
}

bool isEffect2DNode(NodeKind kind) {
    return kind == NodeKind::Envelope
        || kind == NodeKind::GuideCurve
        || kind == NodeKind::ImpulseResponse
        || kind == NodeKind::Waveshaper;
}

bool isPreviewableNode(NodeKind kind) {
    return NodeViewModuleRegistry::instance().moduleFor(kind).capabilities().previewable;
}

enum class OperationPortLayout {
    Side,
    Uptack,
    Vertical,
    Tee
};

OperationPortLayout operationPortLayoutFor(const Node& node) {
    if (node.inputs.size() < 2) {
        return OperationPortLayout::Side;
    }

    const PortSide first = node.inputs[0].side;
    const PortSide second = node.inputs[1].side;

    if (first == PortSide::Left && second == PortSide::Bottom) {
        return OperationPortLayout::Tee;
    }

    if (first == PortSide::Top && second == PortSide::Bottom) {
        return OperationPortLayout::Vertical;
    }

    if (first == PortSide::Left && second == PortSide::Top) {
        return OperationPortLayout::Uptack;
    }

    return OperationPortLayout::Side;
}

OperationPortLayout nextOperationPortLayout(OperationPortLayout layout) {
    switch (layout) {
        case OperationPortLayout::Side:     return OperationPortLayout::Uptack;
        case OperationPortLayout::Uptack:   return OperationPortLayout::Vertical;
        case OperationPortLayout::Vertical: return OperationPortLayout::Tee;
        case OperationPortLayout::Tee:      return OperationPortLayout::Side;
    }

    return OperationPortLayout::Side;
}

Rectangle<float> operationLayoutButtonBounds(const Rectangle<float>& nodeBounds, float zoom) {
    const float size = 27.f * zoom;
    return Rectangle<float>(size, size)
            .withCentre({ nodeBounds.getRight() - 21.f * zoom, nodeBounds.getY() + 21.f * zoom });
}

PortSide nextMeshOutputSide(const Node& node) {
    const PortSide side = node.outputs.empty() ? PortSide::Right : node.outputs.front().side;

    switch (side) {
        case PortSide::Right:     return PortSide::Bottom;
        case PortSide::Bottom:    return PortSide::Top;
        case PortSide::Top:       return PortSide::Right;
        case PortSide::Left:      return PortSide::Right;
    }

    return PortSide::Right;
}

Rectangle<float> expandedEditorBounds(Rectangle<float> componentBounds) {
    const Rectangle<float> available = componentBounds.reduced(kExpandedEditorMinMargin);
    const float width = jmin(available.getWidth(), jmax(420.f, componentBounds.getWidth() * kExpandedEditorScale));
    const float height = jmin(available.getHeight(), jmax(300.f, componentBounds.getHeight() * kExpandedEditorScale));
    return Rectangle<float>(width, height).withCentre(available.getCentre());
}

Rectangle<float> expandedEditorBoundsForNode(Rectangle<float> componentBounds, const Node* node) {
    if (node != nullptr) {
        return NodeViewModuleRegistry::instance().moduleFor(node->kind).expandedEditorBounds(
                componentBounds,
                kExpandedEditorMinMargin);
    }

    return expandedEditorBounds(componentBounds);
}

Rectangle<float> expandedEditorCloseButton(Rectangle<float> panel) {
    return Rectangle<float>(22.f, 22.f).withCentre({ panel.getRight() - 22.f, panel.getY() + kExpandedEditorHeaderHeight * 0.5f });
}

Rectangle<float> expandedEditorContentBounds(Rectangle<float> componentBounds) {
    Rectangle<float> panel = expandedEditorBounds(componentBounds);
    panel.removeFromTop(kExpandedEditorHeaderHeight);
    return panel.reduced(8.f, 8.f);
}

bool expandedEditorBlocksCanvas(const Node* node) {
    return node != nullptr
            && NodeViewModuleRegistry::instance().moduleFor(node->kind)
                    .capabilities().expandedEditorBlocksCanvas;
}

bool hasHostedExpandedEditor(const Node& node) {
    return NodeViewModuleRegistry::instance().moduleFor(node.kind).editorFactory() != nullptr;
}

void drawOperationLayoutIcon(Graphics& g, Rectangle<float> button, OperationPortLayout layout, Colour colour) {
    const auto area = button.reduced(button.getWidth() * 0.24f);
    const float stroke = jmax(1.f, button.getWidth() * 0.085f);
    const float dot = jmax(2.f, button.getWidth() * 0.14f);

    auto dotAt = [&](Point<float> p, bool filled) {
        Rectangle<float> r(p.x - dot * 0.5f, p.y - dot * 0.5f, dot, dot);
        if (filled) {
            g.fillEllipse(r);
        } else {
            g.drawEllipse(r, stroke);
        }
    };

    Point<float> a;
    Point<float> b;
    Point<float> out(area.getRight(), area.getCentreY());

    switch (layout) {
        case OperationPortLayout::Vertical:
            a = { area.getCentreX(), area.getY() };
            b = { area.getCentreX(), area.getBottom() };
            break;

        case OperationPortLayout::Tee:
            a = { area.getX(), area.getCentreY() };
            b = { area.getCentreX(), area.getBottom() };
            break;

        case OperationPortLayout::Uptack:
            a = { area.getX(), area.getCentreY() };
            b = { area.getCentreX(), area.getY() };
            break;

        case OperationPortLayout::Side:
        default:
            a = { area.getX(), area.getY() + area.getHeight() * 0.30f };
            b = { area.getX(), area.getBottom() - area.getHeight() * 0.30f };
            break;
    }

    g.setColour(colour.withAlpha(0.78f));
    Path path;
    path.startNewSubPath(a);
    path.lineTo(area.getCentre());
    path.lineTo(b);
    path.startNewSubPath(area.getCentre());
    path.lineTo(out);
    g.strokePath(path, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour(colour);
    dotAt(a, false);
    dotAt(b, false);
    dotAt(out, true);
}

void drawOutputSideIcon(Graphics& g, Rectangle<float> button, PortSide side, Colour colour) {
    const Rectangle<float> area = button.reduced(button.getWidth() * 0.24f);
    const float stroke = jmax(1.f, button.getWidth() * 0.085f);
    const float dot = jmax(2.f, button.getWidth() * 0.14f);
    const Point<float> centre = area.getCentre();
    Point<float> out;

    switch (side) {
        case PortSide::Top:
            out = { centre.x, area.getY() };
            break;

        case PortSide::Bottom:
            out = { centre.x, area.getBottom() };
            break;

        case PortSide::Right:
        case PortSide::Left:
        default:
            out = { area.getRight(), centre.y };
            break;
    }

    g.setColour(colour.withAlpha(0.78f));
    g.drawEllipse(Rectangle<float>(dot, dot).withCentre(centre), stroke);
    g.drawLine(Line<float>(centre, out), stroke);
    g.setColour(colour);
    g.fillEllipse(Rectangle<float>(dot, dot).withCentre(out));
}

GraphDocument createStartupDocument() {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File defaultGraph = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("resources")
            .getChildFile("default.cyclegraph");
    return GraphDocument::openOrDefault(defaultGraph, NodeGraph::createDemoGraph());
  #else
    return GraphDocument(NodeGraph::createDemoGraph());
  #endif
}

}

NodeCanvas::NodeCanvas() :
        document(createStartupDocument())
    ,   commands(document)
    ,   automation(document, commands)
    ,   graph(document.graph())
    ,   compileResult(presentation.compileResult())
    ,   runtimeTrace(presentation.runtimeTrace())
    ,   previewResult(presentation.previewResult())
    ,   editorCommands(*this, document, commands, *this, *this)
    ,   previewResources(editorCommands)
    ,   previewRenderer(previewResources)
    ,   editorHost(*this, editorCommands, *this, *this)
    ,   automationInspector({ *this, document, presentation, viewport, editorHost })
    ,   renderInvalidation(*this) {
    refreshCompiledState();

    setOpaque(true);
    setWantsKeyboardFocus(true);
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(false);
    openGLContext.attachTo(*this);
    canvasOpenGlAttached = true;
    startTimerHz(30);
}

NodeCanvas::~NodeCanvas() {
    stopTimer();
    detachExpandedEditorHosts();
    setCanvasOpenGlAttached(false);
}

void NodeCanvas::paint(Graphics& g) {
    const Node* expandedNode = findNode(expandedNodeId);

    drawGrid(g);
    {
        Graphics::ScopedSaveState canvasClip(g);

        if (expandedEditorBlocksCanvas(expandedNode)) {
            g.excludeClipRegion(expandedEditorBoundsForNode(
                    getLocalBounds().toFloat(), expandedNode).toNearestInt());
        }

        drawSnapGuides(g);
        drawEdges(g);
        drawConnectionPreview(g);
        drawNodes(g);
        drawMiniMap(g);
        drawEdgeLegend(g);
        drawNodePalette(g);
        drawHoverConsole(g);
    }

    if (expandedNode != nullptr) {
        if (!hasHostedExpandedEditor(*expandedNode)) {
            drawExpandedEditor(g, *expandedNode);
        }
    }
}

void NodeCanvas::resized() {
    viewport.setBounds(getLocalBounds().toFloat());
    updateExpandedEditorHost(findNode(expandedNodeId));
    requestCanvasRepaint();
}

void NodeCanvas::visibilityChanged() {
    renderInvalidation.notifyAvailabilityChanged();
}

void NodeCanvas::mouseMove(const MouseEvent& event) {
    lastMousePosition = event.position;
    palette.updateHover(event.position);
    setMouseCursor(MouseCursor::NormalCursor);
    requestCanvasRepaint();
}

void NodeCanvas::mouseDown(const MouseEvent& event) {
    grabKeyboardFocus();
    palette.updateHover(event.position);
    editStatusMessage = {};
    dragStartPan = viewport.getPan();
    lastMousePosition = event.position;
    draggingNode = false;
    connectingCable = false;
    nodeDragUndoPushed = false;
    nodeDragMoved = false;
    spliceTargetEdgeIndex = -1;
    activeSnapHasX = false;
    activeSnapHasY = false;
    draggingTrimeshMorph = false;
    trimeshMorphUndoPushed = false;
    draggingTrimeshVertexParameter = false;
    trimeshVertexParameterUndoPushed = false;
    activeTrimeshVertexIndex = -1;
    expandedEditorDragCaptured = false;

    if (expandedNodeId.isNotEmpty()) {
        const Node* expandedNode = findNode(expandedNodeId);
        const auto panel = expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode);
        auto closeButton = expandedEditorCloseButton(panel);
        if (expandedNode != nullptr
                && (expandedNode->kind == NodeKind::VoiceContext
                    || expandedNode->kind == NodeKind::Fft
                    || expandedNode->kind == NodeKind::Ifft)) {
            closeButton = Rectangle<float>(18.f, 18.f).withCentre({ panel.getRight() - 18.f, panel.getY() + 15.f });
        }

        if (expandedNode != nullptr && hasHostedExpandedEditor(*expandedNode)) {
            requestCanvasRepaint();
            return;
        }

        if (closeButton.contains(event.position)) {
            expandedNodeId = {};
            requestCanvasRepaint();
            return;
        }

        if (expandedNode != nullptr
                && expandedNode->kind == NodeKind::VoiceContext
                && panel.contains(event.position)
                && handleVoiceContextEditorClick(*expandedNode, panel, event.position)) {
            return;
        }

        if (expandedNode != nullptr
                && (expandedNode->kind == NodeKind::Fft || expandedNode->kind == NodeKind::Ifft)
                && panel.contains(event.position)
                && handleTransformEditorClick(*expandedNode, panel, event.position)) {
            return;
        }

        expandedEditorDragCaptured = panel.contains(event.position);

        requestCanvasRepaint();
        return;
    }

    NodeKind paletteKind;
    if (palette.findKindAt(event.position, paletteKind)) {
        auto result = commands.addNode(
                paletteKind,
                paletteCreationWorldPosition(paletteKind, event.position));

        if (result.succeeded()) {
            selectedNodeId = result.nodeId;
            expandedNodeId = {};
            selectedEdgeIndex = -1;
            if (const Node* node = findNode(result.nodeId)) {
                dragStartNodeBounds = node->bounds;

                if (isOperationNode(node->kind) && !node->inputs.empty()) {
                    const Point<float> mouseWorld = toWorld(event.position);
                    const Point<float> inputCentre = portWorldCentreForBounds(*node, node->inputs.front(), node->bounds);
                    dragStartNodeBounds = dragStartNodeBounds.translated(
                            mouseWorld.x - inputCentre.x,
                            mouseWorld.y - inputCentre.y);
                }

                draggingNode = true;
                nodeDragUndoPushed = false;
            }
            connectingCable = false;
            refreshCompiledState();
            editStatusMessage = "Node added";
            palette.close();
            requestCanvasRepaint();
        }

        return;
    }

    String layoutNodeId;
    if (findOperationLayoutButtonAt(event.position, layoutNodeId)) {
        if (cycleOperationPortLayout(layoutNodeId)) {
            editStatusMessage = "Port layout cycled";
            requestCanvasRepaint();
        }

        return;
    }

    String outputSideNodeId;
    if (findMeshOutputSideButtonAt(event.position, outputSideNodeId)) {
        if (cycleMeshOutputSide(outputSideNodeId)) {
            editStatusMessage = "Output side cycled";
            requestCanvasRepaint();
        }

        return;
    }

    String voiceNodeId;
    if (findVoiceDomainButtonAt(event.position, voiceNodeId)) {
        if (cycleVoiceDomain(voiceNodeId)) {
            requestCanvasRepaint();
        }

        return;
    }

    PortAddress hitPort;
    if (findPortAt(event.position, hitPort)) {
        connectingPort = hitPort;
        connectingPoint = event.position;
        connectingCable = true;
        draggingNode = false;
        selectedNodeId = hitPort.nodeId;
        selectedEdgeIndex = -1;
        requestCanvasRepaint();
        return;
    }

    const Node* hitNode = findNodeAt(toWorld(event.position));

    if (hitNode != nullptr) {
        selectedNodeId = hitNode->id;
        selectedEdgeIndex = -1;
        draggingNode = true;
        dragStartNodeBounds = hitNode->bounds;
        nodeDragUndoPushed = false;

        if (event.getNumberOfClicks() >= 2 && isPreviewableNode(hitNode->kind)) {
            expandedNodeId = expandedNodeId == hitNode->id ? String() : hitNode->id;
        }

        requestCanvasRepaint();
        return;
    }

    selectedEdgeIndex = findEdgeAt(event.position);

    if (selectedEdgeIndex >= 0) {
        if (event.mods.isCtrlDown()) {
            auto result = commands.removeEdgeAt((size_t) selectedEdgeIndex);

            if (result.succeeded()) {
                selectedEdgeIndex = -1;
                refreshCompiledState();
                editStatusMessage = "Edge cut";
            }

            requestCanvasRepaint();
            return;
        }

        selectedNodeId = {};
        draggingNode = false;
        requestCanvasRepaint();
        return;
    }

    selectedNodeId = {};
    selectedEdgeIndex = -1;
    draggingNode = false;
    expandedNodeId = {};

    requestCanvasRepaint();
}

void NodeCanvas::mouseDrag(const MouseEvent& event) {
    lastMousePosition = event.position;

    if (expandedEditorDragCaptured || expandedNodeId.isNotEmpty()) {
    } else if (connectingCable) {
        PortAddress hitPort;
        connectingPoint = findConnectablePortAt(event.position, connectingPort, hitPort)
                ? getPortLocation(hitPort).centre
                : event.position;
    } else if (draggingNode) {
        const Node* node = findNode(selectedNodeId);

        if (node != nullptr) {
            if (!nodeDragUndoPushed) {
                commands.beginCompoundEdit();
                nodeDragUndoPushed = true;
            }

            const auto dragOffset = event.getOffsetFromDragStart().toFloat();
            nodeDragMoved = dragOffset.getDistanceFromOrigin() > 3.f;
            const auto offset = dragOffset / viewport.getZoom();
            commands.resizeNode(
                    node->id,
                    snappedNodeBounds(*node, dragStartNodeBounds.translated(offset.x, offset.y)));
            spliceTargetEdgeIndex = nodeDragMoved
                    ? findSpliceTargetEdgeAt(event.position, selectedNodeId)
                    : -1;
        }
    } else {
        activeSnapHasX = false;
        activeSnapHasY = false;
        viewport.setTransform(
                dragStartPan + event.getOffsetFromDragStart().toFloat(),
                viewport.getZoom());
    }

    requestCanvasRepaint();
}

void NodeCanvas::mouseUp(const MouseEvent& event) {
    lastMousePosition = event.position;

    if (!connectingCable) {
        if (draggingNode && nodeDragMoved && spliceSelectedNodeIntoEdgeAt(event.position)) {
            commands.commitCompoundEdit();
            draggingNode = false;
            nodeDragUndoPushed = false;
            nodeDragMoved = false;
            spliceTargetEdgeIndex = -1;
            activeSnapHasX = false;
            activeSnapHasY = false;
            editorCommands.endTrimeshMorphEdit();
            editorCommands.endTrimeshVertexParameterEdit();
            expandedEditorDragCaptured = false;
            requestCanvasRepaint();
            return;
        }

        draggingNode = false;
        commands.commitCompoundEdit();
        nodeDragUndoPushed = false;
        nodeDragMoved = false;
        spliceTargetEdgeIndex = -1;
        activeSnapHasX = false;
        activeSnapHasY = false;
        editorCommands.endTrimeshMorphEdit();
        editorCommands.endTrimeshVertexParameterEdit();
        expandedEditorDragCaptured = false;
        requestCanvasRepaint();
        return;
    }

    PortAddress destPort;
    connectingCable = false;
    draggingNode = false;
    nodeDragUndoPushed = false;
    nodeDragMoved = false;
    spliceTargetEdgeIndex = -1;
    activeSnapHasX = false;
    activeSnapHasY = false;
    editorCommands.endTrimeshMorphEdit();
    editorCommands.endTrimeshVertexParameterEdit();
    expandedEditorDragCaptured = false;

    if (findConnectablePortAt(event.position, connectingPort, destPort)) {
        auto result = commands.connect(connectingPort, destPort);

        if (result.succeeded()) {
            refreshCompiledState();
            editStatusMessage = "Connected";
        } else if (result.code == GraphEditCode::ValidationRejected) {
            editStatusMessage = "Incompatible connection";
        } else {
            editStatusMessage = "Connection cancelled";
        }
    }

    requestCanvasRepaint();
}

void NodeCanvas::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) {
    const Node* expandedNode = findNode(expandedNodeId);
    if (expandedNode != nullptr && expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode).contains(event.position)) {
        requestCanvasRepaint();
        return;
    }

    constexpr float panScale = 720.f;
    viewport.panBy(Point<float>(wheel.deltaX * panScale, wheel.deltaY * panScale));
    requestCanvasRepaint();
}

void NodeCanvas::mouseMagnify(const MouseEvent& event, float scaleFactor) {
    viewport.zoomAround(event.position, scaleFactor);
    requestCanvasRepaint();
}

bool NodeCanvas::keyPressed(const KeyPress& key) {
    const bool commandDown = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    const int keyCode = key.getKeyCode();
    const juce_wchar keyChar = CharacterFunctions::toLowerCase(key.getTextCharacter());

    if (commandDown && (keyChar == 'z' || keyCode == 'z' || keyCode == 'Z')) {
        return key.getModifiers().isShiftDown() ? redo() : undo();
    }

    if (commandDown && (keyChar == 'y' || keyCode == 'y' || keyCode == 'Y')) {
        return redo();
    }

    if (key == KeyPress::escapeKey) {
        return clearSelection();
    }

    if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey) {
        if (selectedEdgeIndex >= 0) {
            auto result = commands.removeEdgeAt((size_t) selectedEdgeIndex);

            if (!result.succeeded()) {
                return false;
            }

            selectedEdgeIndex = -1;
            refreshCompiledState();
            editStatusMessage = "Edge deleted";
            requestCanvasRepaint();
            return true;
        }

        if (selectedNodeId.isEmpty()) {
            return false;
        }

        auto result = commands.removeNode(selectedNodeId);

        if (!result.succeeded()) {
            return false;
        }

        expandedNodeId = {};
        selectedNodeId = {};
        refreshCompiledState();
        editStatusMessage = "Node deleted";
        requestCanvasRepaint();
        return true;
    }

    return false;
}

void NodeCanvas::newOpenGLContextCreated() {
    renderer.initialize();
}

void NodeCanvas::renderOpenGL() {
    if (kUseGlCanvasUnderlay) {
        renderer.renderBackground(getWidth(), getHeight(), (float) openGLContext.getRenderingScale(),
                viewport.getZoom(), viewport.getPan());

        if (kUseGlCanvasEdges) {
            drawGlEdges();
        }
        if (kUseGlNodeShells) {
            drawGlNodeShells();
        }
        drawGlEffect2DPreviews();
        drawGlExpandedEditor();
    } else {
        OpenGLHelpers::clear(kCanvasBackground);
    }
}

void NodeCanvas::openGLContextClosing() {
    previewResources.releaseOpenGLResources();

    renderer.shutdown();
}

void NodeCanvas::timerCallback() {
    if (compiledStateRefreshPending
            && (int32) (Time::getMillisecondCounter() - compiledStateRefreshDueMs) >= 0) {
        flushScheduledCompiledStateRefresh();
    }

    for (const auto& node : graph.getNodes()) {
        if (isEffect2DNode(node.kind)) {
            effect2DWidgetFor(node).syncFromNode(node);
        }
    }

    updateExpandedEditorHost(findNode(expandedNodeId));

    const auto mouse = getMouseXYRelative().toFloat();
    const int previousPaletteSectionIndex = palette.activeSection();

    if (getLocalBounds().toFloat().contains(mouse)) {
        palette.updateHover(mouse);
    }

    if (getLocalBounds().toFloat().contains(mouse)
            && (mouse != lastMousePosition || previousPaletteSectionIndex != palette.activeSection())) {
        lastMousePosition = mouse;
        requestCanvasRepaint();
    }
}

void NodeCanvas::drawGrid(Graphics& g) {
    const float zoom = viewport.getZoom();
    const Point<float> pan = viewport.getPan();
    if (kUseGlCanvasUnderlay) {
        return;
    }

    g.fillAll(kCanvasBackground);

    const float minorStep = 32.f * zoom;
    const float majorStep = minorStep * 4.f;
    const auto bounds = getLocalBounds().toFloat();

    auto drawGridLines = [&](float step, Colour colour, float thickness) {
        g.setColour(colour);

        float startX = std::fmod(pan.x, step);
        if (startX > 0.f) {
            startX -= step;
        }

        for (float x = startX; x < bounds.getRight(); x += step) {
            g.drawVerticalLine(roundToInt(x), bounds.getY(), bounds.getBottom());
        }

        float startY = std::fmod(pan.y, step);
        if (startY > 0.f) {
            startY -= step;
        }

        for (float y = startY; y < bounds.getBottom(); y += step) {
            g.drawHorizontalLine(roundToInt(y), bounds.getX(), bounds.getRight());
        }

        ignoreUnused(thickness);
    };

    drawGridLines(minorStep, kCanvasGridMinor, 1.f);
    drawGridLines(majorStep, kCanvasGridMajor, 1.f);

    ColourGradient glow(Colour(0x40162531), 0.f, 0.f,
                        Colour(0x00162531), 0.f, (float) getHeight(), false);
    g.setGradientFill(glow);
    g.fillRect(getLocalBounds());
}

void NodeCanvas::drawGlEdges() {
    const float zoom = viewport.getZoom();
    const auto& edges = graph.getEdges();
    const auto visibleArea = getLocalBounds().toFloat().expanded(160.f);
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());

    for (const auto& sceneEdge : scene.edges) {
        if (sceneEdge.edgeIndex < 0 || sceneEdge.edgeIndex >= (int) edges.size()) {
            continue;
        }

        const auto& edge = edges[(size_t) sceneEdge.edgeIndex];
        const Rectangle<float> cableBounds = NodeCableRenderer::visibleBounds(sceneEdge, zoom);

        if (!cableBounds.intersects(visibleArea)) {
            continue;
        }

        Colour colour = colourForDomain(displayDomainForEdge(edge));
        const bool invalid = edgeHasValidationIssue(edge);

        if (invalid) {
            colour = Colour(0xffff5a5f);
        }

        renderer.renderCable(
                sceneEdge.cablePath,
                sceneEdge.source,
                sceneEdge.destination,
                colour,
                NodeCableRenderer::scaleForZoom(zoom),
                sceneEdge.edgeIndex == selectedEdgeIndex || sceneEdge.edgeIndex == spliceTargetEdgeIndex,
                edge.attachment,
                invalid,
                sceneEdge.destinationPortLike);
    }
}

void NodeCanvas::drawGlNodeShells() {
    const float zoom = viewport.getZoom();
    const auto visibleArea = getLocalBounds().toFloat().expanded(120.f);
    const Node* expandedNode = findNode(expandedNodeId);
    const bool hasExpandedEditor = expandedEditorBlocksCanvas(expandedNode);
    const Rectangle<float> expandedPanel = expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode);

    for (const auto& node : graph.getNodes()) {
        const Rectangle<float> bounds = toScreen(node.bounds);

        if (!bounds.intersects(visibleArea)) {
            continue;
        }

        if (hasExpandedEditor && expandedPanel.intersects(bounds)) {
            continue;
        }

        renderer.renderNodeShell(
                bounds,
                42.f * zoom,
                8.f * portScaleForZoom(zoom),
                kNodeBackground,
                kNodeHeader);
    }
}

void NodeCanvas::drawGlExpandedEditor() {
    editorHost.renderOpenGL((float) openGLContext.getRenderingScale());
}

void NodeCanvas::drawGlEffect2DPreviews() {
    const float zoom = viewport.getZoom();
    const auto visibleArea = getLocalBounds().toFloat().expanded(120.f);
    const Node* expandedNode = findNode(expandedNodeId);
    const bool hasExpandedEditor = expandedEditorBlocksCanvas(expandedNode);
    const Rectangle<float> expandedPanel = expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode);
    const float scaleFactor = (float) openGLContext.getRenderingScale();

    for (const auto& node : graph.getNodes()) {
        const Rectangle<float> nodeBounds = toScreen(node.bounds);

        if (!nodeBounds.intersects(visibleArea)) {
            continue;
        }

        if (hasExpandedEditor && expandedPanel.intersects(nodeBounds)) {
            continue;
        }

        const Rectangle<float> preview = previewRenderer.boundsFor(node, nodeBounds, zoom);
        previewRenderer.renderOpenGL(node, preview, scaleFactor);
    }
}

void NodeCanvas::drawEdges(Graphics& g) {
    const float zoom = viewport.getZoom();
    if (kUseGlCanvasEdges) {
        return;
    }

    const auto& edges = graph.getEdges();
    const auto visibleArea = getLocalBounds().toFloat().expanded(160.f);
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());

    for (const auto& sceneEdge : scene.edges) {
        if (sceneEdge.edgeIndex < 0 || sceneEdge.edgeIndex >= (int) edges.size()) {
            continue;
        }

        const auto& edge = edges[(size_t) sceneEdge.edgeIndex];
        const Rectangle<float> cableBounds = NodeCableRenderer::visibleBounds(sceneEdge, zoom);

        if (!cableBounds.intersects(visibleArea)) {
            continue;
        }

        Colour colour = colourForDomain(displayDomainForEdge(edge));
        const bool invalid = edgeHasValidationIssue(edge);

        if (invalid) {
            colour = Colour(0xffff5a5f);
        }

        NodeCableRenderer::paint(g, sceneEdge, {
                colour,
                edge.attachment,
                invalid,
                sceneEdge.edgeIndex == selectedEdgeIndex,
                sceneEdge.edgeIndex == spliceTargetEdgeIndex
        }, zoom);
    }
}

void NodeCanvas::drawConnectionPreview(Graphics& g) {
    const float zoom = viewport.getZoom();
    if (!connectingCable) {
        return;
    }

    const Node* node = findNode(connectingPort.nodeId);

    if (node == nullptr) {
        return;
    }

    const Port* port = findPort(*node, connectingPort.portId, connectingPort.input);

    if (port == nullptr) {
        return;
    }

    const auto start = getPortLocation(connectingPort).centre;
    const auto source = connectingPort.input ? connectingPoint : start;
    const auto dest = connectingPort.input ? start : connectingPoint;
    const PortSide sourceSide = connectingPort.input ? PortSide::Right : port->side;
    const PortSide destSide = connectingPort.input ? port->side : PortSide::Left;
    NodeCableRenderer::paintPending(g, {
            NodeCanvasScene::cablePath(source, dest, sourceSide, destSide, zoom),
            source,
            dest,
            colourForDomain(port->domain)
    }, zoom);
}

void NodeCanvas::drawNodes(Graphics& g) {
    const auto visibleArea = getLocalBounds().toFloat().expanded(120.f);

    for (const auto& node : graph.getNodes()) {
        const Rectangle<float> nodeBounds = toScreen(node.bounds);

        if (!nodeBounds.intersects(visibleArea)) {
            continue;
        }

        drawNode(g, node);
    }
}

void NodeCanvas::drawSnapGuides(Graphics& g) {
    if (!draggingNode || (!activeSnapHasX && !activeSnapHasY)) {
        return;
    }

    const Rectangle<float> area = getLocalBounds().toFloat();
    const float stroke = 0.75f;

    if (activeSnapHasX) {
        const float x = toScreen(Point<float>(activeSnapWorldX, 0.f)).x;
        g.setColour(colourForDomain(PortDomain::TimeSignal).withAlpha(0.18f));
        g.drawLine(Line<float>({ x, area.getY() }, { x, area.getBottom() }), stroke);
    }

    if (activeSnapHasY) {
        const float y = toScreen(Point<float>(0.f, activeSnapWorldY)).y;
        g.setColour(colourForDomain(PortDomain::PitchSignal).withAlpha(0.16f));
        g.drawLine(Line<float>({ area.getX(), y }, { area.getRight(), y }), stroke);
    }
}

void NodeCanvas::drawNode(Graphics& g, const Node& node) {
    const float zoom = viewport.getZoom();
    Rectangle<float> bounds = toScreen(node.bounds);
    const float uiScale = portScaleForZoom(zoom);
    const float corner = 8.f * uiScale;

    if (!kUseGlNodeShells) {
        g.setColour(kNodeBackground);
        g.fillRoundedRectangle(bounds, corner);
    }

    auto header = bounds.removeFromTop(42.f * zoom);
    if (!kUseGlNodeShells) {
        g.setColour(kNodeHeader);
        g.fillRoundedRectangle(header, corner);
        g.fillRect(header.withTrimmedTop(header.getHeight() - corner));
    }

    g.setColour(kNodeBorder);
    g.drawRoundedRectangle(toScreen(node.bounds), corner, 1.2f);

    if (node.id == selectedNodeId) {
        g.setColour(Colours::white.withAlpha(0.86f));
        g.drawRoundedRectangle(toScreen(node.bounds).expanded(2.f), corner + 2.f, 2.f);
    }

    g.setFont(FontOptions(15.f * zoom, Font::bold));
    g.setColour(kText);
    g.drawText(node.title, header.reduced(13.f * zoom, 4.f * zoom), Justification::centredLeft);

    if (isOperationNode(node.kind)) {
        const auto button = operationLayoutButtonBounds(toScreen(node.bounds), zoom);
        g.setColour(Colour(0xff0f141a).withAlpha(0.78f));
        g.fillEllipse(button);
        g.setColour(kMutedText.withAlpha(0.82f));
        g.drawEllipse(button, 1.f * uiScale);
        drawOperationLayoutIcon(g, button, nextOperationPortLayout(operationPortLayoutFor(node)), kMutedText);
    }

    if (hasOutputSideButton(node.kind)) {
        const auto button = operationLayoutButtonBounds(toScreen(node.bounds), zoom);
        g.setColour(Colour(0xff0f141a).withAlpha(0.78f));
        g.fillEllipse(button);
        g.setColour(kMutedText.withAlpha(0.82f));
        g.drawEllipse(button, 1.f * uiScale);
        drawOutputSideIcon(g, button, nextMeshOutputSide(node), kMutedText);
    }

    auto nodeBounds = toScreen(node.bounds);
    auto preview = previewRenderer.boundsFor(node, nodeBounds, zoom);

    if (node.kind != NodeKind::VoiceContext) {
        drawPreview(g, node, preview);
    }

    if (node.kind == NodeKind::VoiceContext) {
        VoiceContextCompactEditor::paintNodeSelector(g, nodeBounds, zoom, node);
    }

    auto drawPort = [&](const Node& portNode, const Port& port) {
        auto location = getPortLocation(portNode, port);
        Colour colour = portDisplayColour(portNode, port);
        const float portScale = portScaleForZoom(zoom);

        g.setColour(colour.withAlpha(0.22f));
        g.fillEllipse(location.bounds.expanded(2.4f * portScale));

        if (port.input) {
            g.setColour(colour);
            g.fillEllipse(location.bounds);
        } else {
            g.setColour(kCanvasBackground.withAlpha(0.92f));
            g.fillEllipse(location.bounds);
            g.setColour(colour);
            g.drawEllipse(location.bounds, 2.f * portScale);
        }
    };

    for (const auto& port : node.inputs) {
        drawPort(node, port);
    }

    for (const auto& port : node.outputs) {
        drawPort(node, port);
    }
}

void NodeCanvas::drawPreview(Graphics& g, const Node& node, Rectangle<float> area) {
    previewRenderer.paint(g, {
            node,
            findPreviewResult(node.id),
            area,
            renderProfileForNodeOutput(node, "out"),
            viewport.getZoom(),
            true
    });
}

TrimeshWidget& NodeCanvas::trimeshWidgetFor(const String& nodeId) {
    return previewResources.trimeshWidget(nodeId);
}

Effect2DWidget& NodeCanvas::effect2DWidgetFor(const Node& node) {
    return previewResources.effect2DWidget(node);
}

void NodeCanvas::drawExpandedEditor(Graphics& g, const Node& node) {
    Rectangle<float> panel = expandedEditorBoundsForNode(getLocalBounds().toFloat(), &node);
    const Rectangle<float> outerPanel = panel;
    const bool isTrimeshEditor = node.kind == NodeKind::TrilinearMesh;
    const bool isVoiceEditor = node.kind == NodeKind::VoiceContext;
    const bool isTransformEditor = node.kind == NodeKind::Fft || node.kind == NodeKind::Ifft;
    const bool isCompactEditor = isVoiceEditor || isTransformEditor;

    if (isTrimeshEditor) {
        const Rectangle<float> content = expandedEditorContentBounds(getLocalBounds().toFloat());
        const Rectangle<int> gridHole = TrimeshWidget::expandedGridPanelContentBounds(content).toNearestInt();
        const Rectangle<int> waveHole = TrimeshWidget::expandedWavePanelContentBounds(content).toNearestInt();

        g.saveState();
        g.excludeClipRegion(gridHole);
        g.excludeClipRegion(waveHole);
        g.setColour(Colours::black.withAlpha(0.38f));
        g.fillRoundedRectangle(panel.translated(0.f, 10.f), 8.f);
        g.setColour(Colour(0xff141a21));
        g.fillRoundedRectangle(panel, 8.f);
        g.restoreState();
    } else {
        g.setColour(Colours::black.withAlpha(0.38f));
        g.fillRoundedRectangle(panel.translated(0.f, 10.f), 8.f);
        g.setColour(Colour(0xff141a21));
        g.fillRoundedRectangle(panel, 8.f);
    }

    auto header = panel.removeFromTop(isCompactEditor ? 30.f : kExpandedEditorHeaderHeight);
    if (!isTrimeshEditor) {
        g.setColour(Colour(0xff202833));
        g.fillRoundedRectangle(header, 8.f);
        g.fillRect(header.withTrimmedTop(header.getHeight() - 8.f));
    } else {
        g.setColour(Colour(0xff202833));
        g.fillRoundedRectangle(header, 8.f);
        g.fillRect(header.withTrimmedTop(header.getHeight() - 8.f));
    }

    g.setColour(kText);
    g.setFont(FontOptions(isCompactEditor ? 13.2f : 14.f, Font::bold));
    g.drawText(node.title, header.reduced(13.f, 4.f), Justification::centredLeft);

    if (!isCompactEditor && node.kind != NodeKind::Spy) {
        g.setColour(kMutedText);
        g.setFont(FontOptions(10.f));
        g.drawText(labelForNodeKind(node.kind), header.reduced(13.f, 4.f), Justification::centredRight);
    }

    Rectangle<float> closeButton = expandedEditorCloseButton(outerPanel);
    if (isCompactEditor) {
        closeButton = Rectangle<float>(18.f, 18.f).withCentre({ outerPanel.getRight() - 18.f, header.getCentreY() });
    }
    g.setColour(Colour(0xff0e1318));
    g.fillEllipse(closeButton);
    g.setColour(Colour(0xff354050).withAlpha(isCompactEditor ? 0.62f : 1.f));
    g.drawEllipse(closeButton, isCompactEditor ? 0.8f : 1.f);
    g.setColour(kText.withAlpha(isCompactEditor ? 0.82f : 1.f));
    const float crossInset = isCompactEditor ? 5.5f : 7.f;
    g.drawLine(closeButton.getX() + crossInset, closeButton.getY() + crossInset,
               closeButton.getRight() - crossInset, closeButton.getBottom() - crossInset, isCompactEditor ? 1.2f : 1.4f);
    g.drawLine(closeButton.getRight() - crossInset, closeButton.getY() + crossInset,
               closeButton.getX() + crossInset, closeButton.getBottom() - crossInset, isCompactEditor ? 1.2f : 1.4f);

    g.setColour(Colour(0xffa7b0bd).withAlpha(isCompactEditor ? 0.36f : 0.62f));
    g.drawRoundedRectangle(outerPanel.reduced(0.75f), 8.f, isCompactEditor ? 1.1f : 1.3f);

    auto content = isTrimeshEditor ? panel.reduced(10.f, 8.f) : panel.reduced(18.f, 16.f);

    if (isTrimeshEditor) {
        trimeshWidgetFor(node.id).paintExpanded(g, node, content);
        return;
    }

    if (node.kind == NodeKind::VoiceContext) {
        VoiceContextCompactEditor::paintExpanded(g, outerPanel, node);
        return;
    }

    if (isTransformEditor) {
        TransformCompactEditor::paint(g, outerPanel, node);
        return;
    }

    if (node.kind == NodeKind::Spy) {
        const Rectangle<float> preview = content.reduced(2.f);

        if (const NodePreviewResult* result = findPreviewResult(node.id)) {
            previewRenderer.paint(g, {
                    node,
                    result,
                    preview,
                    TrimeshRenderProfile::fromDomain(result->domain),
                    viewport.getZoom(),
                    true
            });
            return;
        }

        g.setColour(Colours::black.withAlpha(0.40f));
        g.fillRect(preview);
        return;
    }

    auto preview = content.removeFromTop(jmin(360.f, content.getHeight() * 0.66f));
    drawPreview(g, node, preview);
    content.removeFromTop(14.f);

    auto left = content.removeFromLeft(content.getWidth() * 0.48f);
    auto right = content.withTrimmedLeft(12.f);

    auto drawPorts = [&](Rectangle<float> area, const std::vector<Port>& ports, const String& title) {
        g.setColour(kMutedText);
        g.setFont(FontOptions(9.f, Font::bold));
        g.drawText(title, area.removeFromTop(16.f), Justification::centredLeft);

        for (const auto& port : ports) {
            Rectangle<float> row = area.removeFromTop(18.f);
            g.setColour(colourForDomain(port.domain));
            g.fillEllipse(row.getX(), row.getCentreY() - 3.f, 6.f, 6.f);
            g.setColour(kText);
            g.setFont(FontOptions(9.5f));
            g.drawText(portDisplayLabel(port),
                       row.withTrimmedLeft(12.f), Justification::centredLeft);
        }
    };

    drawPorts(left, node.inputs, "Inputs");
    drawPorts(right, node.outputs, "Outputs");
}

void NodeCanvas::setCanvasOpenGlAttached(bool shouldAttach) {
    if (canvasOpenGlAttached == shouldAttach) {
        return;
    }

    if (shouldAttach) {
        openGLContext.attachTo(*this);
    } else {
        openGLContext.detach();
    }

    canvasOpenGlAttached = shouldAttach;
}

void NodeCanvas::updateExpandedEditorHost(const Node* node) {
    hideExpandedEditorHostsExcept(node != nullptr ? node->id : String());
    const Rectangle<int> bounds = node != nullptr
            ? expandedEditorBoundsForNode(getLocalBounds().toFloat(), node).toNearestInt()
            : Rectangle<int>();
    editorHost.bind(node, bounds, document.revision());
    openGLContext.triggerRepaint();
    return;

}

void NodeCanvas::hideExpandedEditorHosts() {
    hideExpandedEditorHostsExcept({});
}

void NodeCanvas::hideExpandedEditorHostsExcept(const String& nodeId) {
    previewResources.hideExpandedHostsExcept(nodeId);
}

void NodeCanvas::detachExpandedEditorHosts() {
    previewResources.detachTrimeshHosts(*this);

    editorHost.detach();
}

void NodeCanvas::drawMiniMap(Graphics& g) {
    const float zoom = viewport.getZoom();
    const Point<float> pan = viewport.getPan();
    Rectangle<float> map((float) getWidth() - 180.f, 18.f, 154.f, 92.f);
    g.setColour(Colour(0xaa0b0e13));
    g.fillRoundedRectangle(map, 7.f);
    g.setColour(Colour(0xff354050));
    g.drawRoundedRectangle(map, 7.f, 1.f);

    Rectangle<float> graphBounds;
    for (const auto& node : graph.getNodes()) {
        graphBounds = graphBounds.isEmpty() ? node.bounds : graphBounds.getUnion(node.bounds);
    }

    graphBounds = graphBounds.expanded(120.f);
    const float scale = jmin(map.getWidth() / graphBounds.getWidth(),
                             map.getHeight() / graphBounds.getHeight());
    const Rectangle<float> graphInMap(
            map.getCentreX() - graphBounds.getWidth() * scale * 0.5f,
            map.getCentreY() - graphBounds.getHeight() * scale * 0.5f,
            graphBounds.getWidth() * scale,
            graphBounds.getHeight() * scale);

    auto project = [&](Rectangle<float> r) {
        const float x = graphInMap.getX() + (r.getX() - graphBounds.getX()) * scale;
        const float y = graphInMap.getY() + (r.getY() - graphBounds.getY()) * scale;
        const float w = r.getWidth() * scale;
        const float h = r.getHeight() * scale;
        return Rectangle<float>(x, y, w, h);
    };

    for (const auto& node : graph.getNodes()) {
        g.setColour(Colour(0xff778596).withAlpha(0.62f));
        g.fillRoundedRectangle(project(node.bounds), 2.f);
    }

    Rectangle<float> viewWorld((-pan.x) / zoom, (-pan.y) / zoom,
                               (float) getWidth() / zoom, (float) getHeight() / zoom);
    const Rectangle<float> viewInMap = project(viewWorld).getIntersection(graphInMap);
    g.setColour(Colour(0xff35d6d2).withAlpha(0.24f));
    g.fillRoundedRectangle(viewInMap, 3.f);
    g.setColour(Colour(0xff35d6d2).withAlpha(0.85f));
    g.drawRoundedRectangle(viewInMap, 3.f, 1.f);

    if (editStatusMessage.isNotEmpty()) {
        Rectangle<float> status(map.getX(), map.getBottom() + 8.f, map.getWidth(), 24.f);
        g.setColour(Colour(0xaa0b0e13));
        g.fillRoundedRectangle(status, 5.f);
        g.setColour(kMutedText);
        g.setFont(FontOptions(10.f));
        g.drawText(editStatusMessage, status.reduced(8.f, 0.f), Justification::centredLeft);
    }
}

void NodeCanvas::drawGraphStatus(Graphics& g) {
    Rectangle<float> status(18.f, (float) getHeight() - 42.f, 420.f, 24.f);

    g.setColour(Colour(0xaa0b0e13));
    g.fillRoundedRectangle(status, 5.f);
    g.setColour(Colour(0xff354050));
    g.drawRoundedRectangle(status, 5.f, 1.f);

    const bool valid = compileResult.succeeded();
    const String graphState = valid
            ? "Graph valid"
            : "Issues " + String((int) compileResult.validationIssues.size()
                                  + (int) compileResult.compileIssues.size());
    const String text = graphState
            + "  /  Nodes " + String((int) graph.getNodes().size())
            + "  /  Edges " + String((int) graph.getEdges().size())
            + "  /  Attach " + String(attachmentCount());

    g.setColour(valid ? Colour(0xff74e28a) : Colour(0xffff6b5a));
    g.fillEllipse(status.getX() + 9.f, status.getCentreY() - 3.f, 6.f, 6.f);
    g.setColour(kMutedText);
    g.setFont(FontOptions(10.f));
    g.drawText(text, status.withTrimmedLeft(23.f).reduced(0.f, 1.f), Justification::centredLeft);
}

void NodeCanvas::drawEdgeLegend(Graphics& g) {
    struct LegendEntry {
        PortDomain domain;
        const char* label;
        bool attachment;
    };

    const LegendEntry entries[] = {
            { PortDomain::TimeSignal, "Time", false },
            { PortDomain::SpectralMagnitudeSignal, "Magnitude", false },
            { PortDomain::SpectralPhaseSignal, "Phase", false },
            { PortDomain::EnvelopeSignal, "Envelope", false },
            { PortDomain::EnvelopeSignal, "Attachment", true },
            { PortDomain::ControlSignal, "Universal", false }
    };

    constexpr float legendWidth = 116.f;
    Rectangle<float> legend((float) getWidth() - legendWidth - 18.f, (float) getHeight() - 174.f, legendWidth, 138.f);

    g.setColour(Colour(0xaa0b0e13));
    g.fillRoundedRectangle(legend, 5.f);
    g.setColour(Colour(0xff354050));
    g.drawRoundedRectangle(legend, 5.f, 1.f);

    float y = legend.getY() + 17.f;
    const Font legendFont(FontOptions(9.f));
    constexpr float lineWidth = 17.f;
    constexpr float labelGap = 7.f;
    constexpr float rowHeight = 20.f;

    g.setFont(legendFont);

    for (const auto& entry : entries) {
        const Colour colour = colourForDomain(entry.domain);
        const float x = legend.getX() + 12.f;
        const float labelX = x + lineWidth + labelGap;
        Path path;
        path.startNewSubPath(x, y);
        path.lineTo(x + lineWidth, y);

        if (entry.attachment) {
            Path dashed;
            PathStrokeType stroke(2.f, PathStrokeType::curved, PathStrokeType::rounded);
            Array<float> dashes { 5.f, 4.f };
            stroke.createDashedStroke(dashed, path, dashes.getRawDataPointer(), dashes.size());
            g.setColour(colour.withAlpha(0.90f));
            g.strokePath(dashed, stroke);
        } else {
            g.setColour(colour.withAlpha(0.90f));
            g.strokePath(path, PathStrokeType(2.f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        g.setColour(kMutedText);
        g.drawText(entry.label, Rectangle<float>(labelX, y - rowHeight * 0.5f, legend.getRight() - labelX - 8.f, rowHeight),
                   Justification::centredLeft);
        y += rowHeight;
    }
}

void NodeCanvas::drawHoverConsole(Graphics& g) {
    const String text = hoverTextFor(lastMousePosition);

    if (text.isEmpty()) {
        return;
    }

    Rectangle<float> console(18.f, (float) getHeight() - 42.f, jmin(560.f, (float) getWidth() - 220.f), 24.f);

    if (console.getWidth() < 180.f) {
        return;
    }

    auto textBounds = console.reduced(10.f, 1.f);
    g.setFont(FontOptions(10.f));
    g.setColour(Colour(0xff0b0e13).withAlpha(0.76f));
    g.drawText(text, textBounds.translated(0.f, 1.f), Justification::centredLeft);
    g.setColour(kMutedText);
    g.drawText(text, textBounds, Justification::centredLeft);
}

void NodeCanvas::drawNodePalette(Graphics& g) {
    const int activeSectionIndex = palette.activeSection();

    auto drawEntryIcon = [&](NodeKind kind, Rectangle<float> area) {
        Node previewNode = GraphNodeFactory().createNode(kind, {}, {});
        const PortDomain domain = previewNode.outputs.empty()
                ? (previewNode.inputs.empty() ? PortDomain::ControlSignal : previewNode.inputs.front().domain)
                : previewNode.outputs.front().domain;
        previewRenderer.paint(g, {
                previewNode,
                nullptr,
                area,
                TrimeshRenderProfile::fromDomain(domain),
                viewport.getZoom(),
                false
        });
    };

    for (int sectionIndex = 0; sectionIndex < palette.sectionCount(); ++sectionIndex) {
        const auto& section = palette.section(sectionIndex);
        const bool active = sectionIndex == activeSectionIndex;
        const Rectangle<float> button = palette.groupBounds(sectionIndex);

        g.setColour(Colour(active ? 0xff1d2631 : 0xff151b24).withAlpha(active ? 0.94f : 0.82f));
        g.fillRoundedRectangle(button, 7.f);
        g.setColour(Colour(active ? 0xff8290a2 : 0xff435061).withAlpha(active ? 0.86f : 0.72f));
        g.drawRoundedRectangle(button, 7.f, active ? 1.6f : 1.f);

        Rectangle<float> content = button;
        const Rectangle<float> label = content.removeFromBottom(18.f);
        drawPaletteGroupIcon(g, section.title, content.reduced(8.f, 4.f), active);
        g.setFont(FontOptions(9.4f, Font::bold));
        g.setColour(kText.withAlpha(active ? 0.92f : 0.70f));
        g.drawText(String(section.shortLabel), label.reduced(3.f, 0.f), Justification::centred);
    }

    if (activeSectionIndex < 0) {
        return;
    }

    const auto& activeSection = palette.section(activeSectionIndex);
    const Colour sectionColour = Colour(0xff8290a2);

    for (int entryIndex = 0; entryIndex < activeSection.entryCount; ++entryIndex) {
        const auto& entry = activeSection.entries[entryIndex];
        const Rectangle<float> row = palette.entryBounds(activeSectionIndex, entryIndex);
        const bool hover = row.contains(lastMousePosition);
        g.setColour(Colour(hover ? 0xff202935 : 0xff161d26).withAlpha(hover ? 0.94f : 0.82f));
        g.fillRoundedRectangle(row, 6.f);
        g.setColour(sectionColour.withAlpha(hover ? 0.76f : 0.42f));
        g.drawRoundedRectangle(row, 6.f, hover ? 1.4f : 1.f);

        const Rectangle<float> icon(row.getX() + 7.f, row.getY() + 8.f, 39.f, row.getHeight() - 16.f);
        drawEntryIcon(entry.kind, icon);

        g.setColour(kText.withAlpha(hover ? 0.96f : 0.82f));
        g.setFont(FontOptions(11.2f, Font::bold));
        g.drawText(
                String::fromUTF8(entry.label),
                row.withTrimmedLeft(53.f).reduced(0.f, 2.f),
                Justification::centredLeft);
    }
}

Point<float> NodeCanvas::toScreen(Point<float> p) const {
    return viewport.toScreen(p);
}

Point<float> NodeCanvas::toWorld(Point<float> p) const {
    return viewport.toWorld(p);
}

Rectangle<float> NodeCanvas::toScreen(Rectangle<float> r) const {
    return viewport.toScreen(r);
}

Rectangle<float> NodeCanvas::snappedNodeBounds(const Node& node, Rectangle<float> proposed) {
    std::vector<SnapCandidate> xCandidates;
    std::vector<SnapCandidate> yCandidates;
    std::vector<float> xAnchors;
    std::vector<float> yAnchors;

    activeSnapHasX = false;
    activeSnapHasY = false;

    for (const auto& candidateNode : graph.getNodes()) {
        if (candidateNode.id == node.id) {
            continue;
        }

        addNodeSnapCandidates(xCandidates, yCandidates, candidateNode);
    }

    addDraggedNodeAnchors(xAnchors, yAnchors, node, proposed);

    float deltaX {};
    float deltaY {};
    if (bestSnapDelta(xAnchors, xCandidates, deltaX, activeSnapWorldX)) {
        proposed = proposed.translated(deltaX, 0.f);
        activeSnapHasX = true;
    }

    xAnchors.clear();
    yAnchors.clear();
    addDraggedNodeAnchors(xAnchors, yAnchors, node, proposed);

    if (bestSnapDelta(yAnchors, yCandidates, deltaY, activeSnapWorldY)) {
        proposed = proposed.translated(0.f, deltaY);
        activeSnapHasY = true;
    }

    return proposed;
}

NodeCanvas::PortLocation NodeCanvas::getPortLocation(const Node& node, const Port& port) const {
    const float size = 8.8f * portScaleForZoom(viewport.getZoom());
    Point<float> centre = toScreen(NodeCanvasScene::portWorldCentre(node, port));
    Rectangle<float> bounds(centre.x - size * 0.5f, centre.y - size * 0.5f, size, size);
    return { bounds, centre };
}

NodeCanvas::PortLocation NodeCanvas::getPortLocation(const PortAddress& address) const {
    const Node* node = findNode(address.nodeId);

    if (node == nullptr) {
        return {};
    }

    const Port* port = findPort(*node, address.portId, address.input);

    if (port == nullptr) {
        return {};
    }

    return getPortLocation(*node, *port);
}

bool NodeCanvas::findPortAt(Point<float> screenPosition, PortAddress& result) const {
    const auto hit = hitTester.hitTest(sceneBuilder.build(
            graph, viewport, presentation.revision(), document.revision()), screenPosition);
    if (hit.has_value() && hit->isPort()) {
        result = hit->portAddress();
        return true;
    }

    return false;
}

bool NodeCanvas::findConnectablePortAt(
        Point<float> screenPosition,
        const PortAddress& source,
        PortAddress& result) const {
    constexpr float snapExpansion = 50.f;
    float bestDistance = std::numeric_limits<float>::max();
    bool found {};

    auto testPort = [&](const Node& node, const Port& port) {
        const auto location = getPortLocation(node, port);

        if (!location.bounds.expanded(snapExpansion).contains(screenPosition)) {
            return;
        }

        const PortAddress candidate { node.id, port.id, port.input };

        if (!canConnectPorts(source, candidate)) {
            return;
        }

        const float distance = screenPosition.getDistanceFrom(location.centre);

        if (distance < bestDistance) {
            bestDistance = distance;
            result = candidate;
            found = true;
        }
    };

    for (const auto& node : graph.getNodes()) {
        for (const auto& port : node.inputs) {
            testPort(node, port);
        }

        for (const auto& port : node.outputs) {
            testPort(node, port);
        }
    }

    return found;
}

bool NodeCanvas::findOperationLayoutButtonAt(Point<float> screenPosition, String& nodeId) const {
    const float zoom = viewport.getZoom();
    const auto& nodes = graph.getNodes();

    for (int i = (int) nodes.size() - 1; i >= 0; --i) {
        const auto& node = nodes[(size_t) i];

        if (!isOperationNode(node.kind)) {
            continue;
        }

        if (operationLayoutButtonBounds(toScreen(node.bounds), zoom).expanded(4.f * zoom).contains(screenPosition)) {
            nodeId = node.id;
            return true;
        }
    }

    return false;
}

bool NodeCanvas::findMeshOutputSideButtonAt(Point<float> screenPosition, String& nodeId) const {
    const float zoom = viewport.getZoom();
    const auto& nodes = graph.getNodes();

    for (int i = (int) nodes.size() - 1; i >= 0; --i) {
        const auto& node = nodes[(size_t) i];

        if (!hasOutputSideButton(node.kind)) {
            continue;
        }

        if (operationLayoutButtonBounds(toScreen(node.bounds), zoom).expanded(4.f * zoom).contains(screenPosition)) {
            nodeId = node.id;
            return true;
        }
    }

    return false;
}

bool NodeCanvas::findVoiceDomainButtonAt(Point<float> screenPosition, String& nodeId) const {
    const float zoom = viewport.getZoom();
    const auto& nodes = graph.getNodes();

    for (int i = (int) nodes.size() - 1; i >= 0; --i) {
        const auto& node = nodes[(size_t) i];

        if (node.kind != NodeKind::VoiceContext) {
            continue;
        }

        if (VoiceContextCompactEditor::hitNodeSelector(
                toScreen(node.bounds),
                zoom,
                screenPosition)) {
            nodeId = node.id;
            return true;
        }
    }

    return false;
}

int NodeCanvas::findEdgeAt(Point<float> screenPosition) const {
    const auto hit = hitTester.hitTest(sceneBuilder.build(
            graph, viewport, presentation.revision(), document.revision()), screenPosition);
    return hit.has_value() && hit->kind == NodeSceneTargetKind::Edge
            ? hit->edgeIndex
            : -1;
}

int NodeCanvas::findSpliceTargetEdgeAt(Point<float> screenPosition, const String& nodeId) const {
    const auto& edges = graph.getEdges();
    const Node* node = findNode(nodeId);
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());

    for (auto sceneEdge = scene.edges.rbegin(); sceneEdge != scene.edges.rend(); ++sceneEdge) {
        const int edgeIndex = sceneEdge->edgeIndex;

        if (edgeIndex < 0 || edgeIndex >= (int) edges.size()) {
            continue;
        }

        const auto& edge = edges[(size_t) edgeIndex];

        if (edge.sourceNodeId == nodeId || edge.destNodeId == nodeId) {
            continue;
        }

        if (sceneEdge->hitPath.contains(screenPosition)) {
            NodeGraph candidate = graph;
            const auto result = node != nullptr && node->kind == NodeKind::Spy
                    ? GraphEditor().attachSpyToEdge(candidate, (size_t) edgeIndex, nodeId)
                    : GraphEditor().spliceNodeIntoEdge(candidate, (size_t) edgeIndex, nodeId);

            if (!result.succeeded()) {
                continue;
            }

            return edgeIndex;
        }
    }

    return -1;
}

const Node* NodeCanvas::findNode(const String& id) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

const Node* NodeCanvas::findNodeAt(Point<float> worldPosition) const {
    const auto& nodes = graph.getNodes();

    for (int i = (int) nodes.size() - 1; i >= 0; --i) {
        const auto& node = nodes[(size_t) i];

        if (node.bounds.contains(worldPosition)) {
            return &node;
        }
    }

    return nullptr;
}

const Port* NodeCanvas::findPort(const Node& node, const String& portId, bool inputPort) const {
    const auto& ports = inputPort ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == portId) {
            return &port;
        }
    }

    return nullptr;
}

const RuntimeNodeTrace* NodeCanvas::findRuntimeTrace(const String& nodeId) const {
    for (const auto& node : runtimeTrace.nodes) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

const NodePreviewResult* NodeCanvas::findPreviewResult(const String& nodeId) const {
    for (const auto& node : previewResult.nodes) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

PortDomain NodeCanvas::displayDomainForEdge(const Edge& edge) const {
    if (edge.attachment) {
        return edge.domain;
    }

    return GraphValidator().resolvedDomainForEdge(graph, edge);
}

PortDomain NodeCanvas::displayDomainForNodeOutput(const Node& node, const String& portId) const {
    if (compileResult.succeeded()) {
        for (const auto& step : compileResult.plan.steps) {
            if (step.nodeId != node.id) {
                continue;
            }

            for (const auto& output : step.outputs) {
                if (output.portId == portId) {
                    return output.domain;
                }
            }
        }
    }

    for (const auto& edge : graph.getEdges()) {
        if (!edge.attachment && edge.sourceNodeId == node.id && edge.sourcePortId == portId) {
            return displayDomainForEdge(edge);
        }
    }

    if (const Port* port = findPort(node, portId, false)) {
        return port->domain;
    }

    return node.outputs.empty() ? PortDomain::ControlSignal : node.outputs.front().domain;
}

TrimeshRenderProfile NodeCanvas::renderProfileForNodeOutput(const Node& node, const String& portId) const {
    NodeRenderSemantic semantic = GraphRenderSemanticResolver().semanticForNodeOutput(graph, node.id, portId);

    if (semantic.domain == PortDomain::ControlSignal) {
        semantic.domain = displayDomainForNodeOutput(node, portId);
    }

    return TrimeshRenderProfile::fromSemantic(semantic);
}

bool NodeCanvas::edgeHasValidationIssue(const Edge& edge) const {
    return GraphValidator().edgeHasValidationIssue(graph, edge);
}

GraphValidationIssue NodeCanvas::validationIssueForEdge(const Edge& edge) const {
    return GraphValidator().validationIssueForEdge(graph, edge);
}

int NodeCanvas::executionIndexForNode(const String& nodeId) const {
    if (!compileResult.succeeded()) {
        return -1;
    }

    const auto& nodeOrder = compileResult.plan.nodeOrder;
    for (int i = 0; i < (int) nodeOrder.size(); ++i) {
        if (nodeOrder[(size_t) i] == nodeId) {
            return i;
        }
    }

    return -1;
}

int NodeCanvas::attachmentCount() const {
    int count = 0;

    for (const auto& edge : graph.getEdges()) {
        if (edge.attachment) {
            ++count;
        }
    }

    return count;
}

String NodeCanvas::hoverTextFor(Point<float> screenPosition) const {
    NodeKind paletteKind;

    if (palette.findKindAt(screenPosition, paletteKind)) {
        Node node = GraphNodeFactory().createNode(paletteKind, {}, {});
        return "Create " + node.title + "  /  " + node.subtitle;
    }

    const int paletteSectionIndex = palette.findSectionAt(screenPosition);

    if (paletteSectionIndex >= 0) {
        return "Node group  /  " + String(palette.section(paletteSectionIndex).title);
    }

    String layoutNodeId;
    if (findOperationLayoutButtonAt(screenPosition, layoutNodeId)) {
        return "Cycle operation port layout  /  side, uptack, vertical, T";
    }

    String outputSideNodeId;
    if (findMeshOutputSideButtonAt(screenPosition, outputSideNodeId)) {
        return "Cycle output side  /  east, south, north";
    }

    String voiceNodeId;
    if (findVoiceDomainButtonAt(screenPosition, voiceNodeId)) {
        const Node* node = findNode(voiceNodeId);

        if (node != nullptr) {
            return "Voice start domain  /  " + VoiceContextCompactEditor::domainLabel(*node)
                    + "  /  click to switch";
        }
    }

    PortAddress portAddress;

    if (findPortAt(screenPosition, portAddress)) {
        return textForPort(portAddress);
    }

    const Node* node = findNodeAt(toWorld(screenPosition));

    if (node != nullptr) {
        return textForNode(*node);
    }

    const int edgeIndex = findEdgeAt(screenPosition);

    if (edgeIndex >= 0 && edgeIndex < (int) graph.getEdges().size()) {
        const auto& edge = graph.getEdges()[(size_t) edgeIndex];
        const auto issue = validationIssueForEdge(edge);

        if (issue.message.isNotEmpty()) {
            return "Invalid edge  /  " + issue.message
                    + "  /  " + edge.sourceNodeId + "." + edge.sourcePortId
                    + " -> " + edge.destNodeId + "." + edge.destPortId;
        }

        return String(edge.attachment ? "Attachment" : "Signal")
                + " edge  /  " + labelForDomain(displayDomainForEdge(edge))
                + "  /  " + edge.sourceNodeId + "." + edge.sourcePortId
                + " -> " + edge.destNodeId + "." + edge.destPortId;
    }

    return {};
}

String NodeCanvas::textForPort(const PortAddress& address) const {
    const Node* node = findNode(address.nodeId);

    if (node == nullptr) {
        return {};
    }

    const Port* port = findPort(*node, address.portId, address.input);

    if (port == nullptr) {
        return {};
    }

    String text = node->title + "  /  " + (address.input ? "Input" : "Output")
            + " port " + portDisplayLabel(*port)
            + "  /  " + labelForDomain(port->domain);

    if (port->purpose == PortPurpose::ScratchAttachment) {
        text += " scratch attachment";
    }

    if (port->channelLayout != ChannelLayout::Mono) {
        text += "  /  " + labelForChannelLayout(port->channelLayout);
    }

    return text;
}

String NodeCanvas::textForNode(const Node& node) const {
    String text = node.title + "  /  " + node.subtitle
            + "  /  inputs " + String((int) node.inputs.size())
            + "  /  outputs " + String((int) node.outputs.size());

    const RuntimeNodeTrace* trace = findRuntimeTrace(node.id);

    if (trace != nullptr && !trace->signalOutputs.empty()) {
        text += "  /  emits ";

        for (size_t i = 0; i < trace->signalOutputs.size(); ++i) {
            if (i > 0) {
                text += ", ";
            }

            text += trace->signalOutputs[i].portId
                    + "="
                    + labelForDomain(trace->signalOutputs[i].domain);
        }
    }

    return text;
}

Point<float> NodeCanvas::viewportCentreWorld() const {
    viewport.setBounds(getLocalBounds().toFloat());
    return viewport.centreWorld();
}

Point<float> NodeCanvas::paletteCreationWorldPosition(NodeKind kind, Point<float> paletteClickPosition) const {
    const float paletteRight = palette.railBounds().getRight();
    const float x = jmin((float) getWidth() - 280.f, paletteRight + 32.f);
    Point<float> position = toWorld({ x, paletteClickPosition.y });

    if (isOperationNode(kind)) {
        const Node node = GraphNodeFactory().createNode(kind, {}, position);

        if (!node.inputs.empty()) {
            position.y -= portWorldCentreForBounds(node, node.inputs.front(), node.bounds).y - node.bounds.getY();
        }
    }

    return position;
}

void NodeCanvas::refreshCompiledState() {
    compiledStateRefreshPending = false;
    previewResources.clearCachedSprites();
    presentation.refresh(graph, document.revision(), document.lastChange());
}

NodeCanvasAutomationPresentation NodeCanvas::automationPresentationState() const {
    return {
            selectedNodeId,
            expandedNodeId,
            editStatusMessage,
            selectedEdgeIndex
    };
}

void NodeCanvas::scheduleCompiledStateRefresh() {
    constexpr uint32 refreshDelayMs = 55;

    if (compiledStateRefreshPending) {
        return;
    }

    compiledStateRefreshPending = true;
    compiledStateRefreshDueMs = Time::getMillisecondCounter() + refreshDelayMs;
}

void NodeCanvas::flushScheduledCompiledStateRefresh() {
    if (!compiledStateRefreshPending) {
        return;
    }

    refreshCompiledState();
    openGLContext.triggerRepaint();
    requestCanvasRepaint();
}

var NodeCanvas::exportAutomationState() const {
    return automationInspector.exportState(automationPresentationState());
}

String NodeCanvas::exportGraphXml() const {
    return automationInspector.exportGraphXml();
}

bool NodeCanvas::openNodeEditorForAutomation(const String& nodeId) {
    const Node* node = findNode(nodeId);

    if (node == nullptr || !isPreviewableNode(node->kind)) {
        return false;
    }

    selectedNodeId = node->id;
    selectedEdgeIndex = -1;
    expandedNodeId = node->id;
    updateExpandedEditorHost(node);
    editStatusMessage = "Opened editor: " + node->id;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::addNodeForAutomation(const String& kindId, Point<float> position, String& nodeId) {
    NodeKind kind {};

    if (!nodeKindForAutomationId(kindId, kind)) {
        return false;
    }

    auto result = automation.addNode(kind, position);

    if (!result.succeeded()) {
        return false;
    }

    refreshCompiledState();
    selectedNodeId = result.nodeId;
    selectedEdgeIndex = -1;
    expandedNodeId = {};
    nodeId = result.nodeId;
    editStatusMessage = "Node added: " + nodeId;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::moveNodeForAutomation(const String& nodeId, Point<float> position) {
    const Node* node = findNode(nodeId);

    if (node == nullptr) {
        return false;
    }

    if (!automation.moveNode(nodeId, position).succeeded()) {
        return false;
    }
    selectedNodeId = nodeId;
    selectedEdgeIndex = -1;
    editStatusMessage = "Node moved: " + nodeId;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::connectPortsForAutomation(
        const String& sourceNodeId,
        const String& sourcePortId,
        const String& destNodeId,
        const String& destPortId) {
    auto result = automation.connect(
            { sourceNodeId, sourcePortId, false },
            { destNodeId, destPortId, true });

    if (!result.succeeded()) {
        return false;
    }

    refreshCompiledState();
    selectedNodeId = destNodeId;
    selectedEdgeIndex = -1;
    editStatusMessage = "Connected " + sourceNodeId + "." + sourcePortId
            + " -> " + destNodeId + "." + destPortId;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::deleteNodeForAutomation(const String& nodeId) {
    auto result = automation.deleteNode(nodeId);

    if (!result.succeeded()) {
        return false;
    }

    refreshCompiledState();
    selectedNodeId = {};
    selectedEdgeIndex = -1;
    expandedNodeId = expandedNodeId == nodeId ? String() : expandedNodeId;
    editStatusMessage = "Node deleted: " + nodeId;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::deleteEdgeForAutomation(int edgeIndex) {
    if (edgeIndex < 0) {
        return false;
    }

    auto result = automation.deleteEdge(edgeIndex);

    if (!result.succeeded()) {
        return false;
    }

    refreshCompiledState();
    selectedEdgeIndex = -1;
    editStatusMessage = "Edge deleted: " + String(edgeIndex);
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::setNodeParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        const String& value) {
    if (findNode(nodeId) == nullptr || parameterId.isEmpty()) {
        return false;
    }

    auto result = automation.setParameter(
            nodeId,
            parameterId,
            label.isEmpty() ? parameterId : label,
            value);

    if (!result.succeeded()) {
        return false;
    }

    refreshCompiledState();
    selectedNodeId = nodeId;
    selectedEdgeIndex = -1;
    editStatusMessage = "Parameter set: " + nodeId + "." + parameterId;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::setMorphSliderForAutomation(const String& nodeId, const String& axis, float value) {
    const Node* node = findNode(nodeId);

    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }

    if (axis != "yellow" && axis != "red" && axis != "blue") {
        return false;
    }

    if (expandedNodeId != nodeId && !openNodeEditorForAutomation(nodeId)) {
        return false;
    }

    if (!editorCommands.beginTrimeshMorphEdit(nodeId, axis, jlimit(0.f, 1.f, value))) {
        return false;
    }

    editorCommands.endTrimeshMorphEdit();
    return true;
}

bool NodeCanvas::setPrimaryAxisForAutomation(const String& nodeId, const String& axis) {
    if (expandedNodeId != nodeId && !openNodeEditorForAutomation(nodeId)) {
        return false;
    }

    return editorCommands.setTrimeshPrimaryAxisValue(nodeId, axis);
}

bool NodeCanvas::toggleLinkForAutomation(const String& nodeId, const String& axis) {
    if (expandedNodeId != nodeId && !openNodeEditorForAutomation(nodeId)) {
        return false;
    }

    return editorCommands.toggleTrimeshLinkAxisValue(nodeId, axis);
}

bool NodeCanvas::selectVertexForAutomation(const String& nodeId, int vertexIndex) {
    if (expandedNodeId != nodeId && !openNodeEditorForAutomation(nodeId)) {
        return false;
    }

    return editorCommands.selectTrimeshVertexIndex(nodeId, vertexIndex);
}

bool NodeCanvas::setVertexParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        float value) {
    if (expandedNodeId != nodeId && !openNodeEditorForAutomation(nodeId)) {
        return false;
    }

    if (!editorCommands.beginTrimeshVertexParameterEdit(
            nodeId, parameterId, jlimit(0.f, 1.f, value))) {
        return false;
    }

    editorCommands.endTrimeshVertexParameterEdit();
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::getNodeParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        String& value) const {
    return automation.getParameter(nodeId, parameterId, value);
}

var NodeCanvas::inspectNodeControlsForAutomation(const String& nodeId) const {
    return automationInspector.inspectNodeControls(nodeId, automationPresentationState());
}

var NodeCanvas::inspectPointerTargetsForAutomation() const {
    return automationInspector.inspectPointerTargets(automationPresentationState());
}

var NodeCanvas::inspectOpenGLDiagnosticsForAutomation() const {
    auto* root = new DynamicObject();
    root->setProperty("schema", "cycle-v2-opengl-diagnostics.v1");
    root->setProperty("canvasOpenGlAttached", canvasOpenGlAttached);
    root->setProperty("canvasBounds", rectangleToVar(getLocalBounds().toFloat()));
    root->setProperty("canvasScreenBounds", rectangleToVar(getScreenBounds().toFloat()));
    root->setProperty("expandedNodeId", expandedNodeId);
    root->setProperty("trimeshExpandedEditorCreated", editorHost.hasEditor());

    if (Component* component = editorHost.component()) {
        root->setProperty("trimeshExpandedEditor", componentDiagnosticsToVar(
                "hostedExpandedEditor",
                component));
    }

    const Node* expandedNode = findNode(expandedNodeId);
    if (expandedNode != nullptr) {
        root->setProperty("expandedNodeKind", labelForNodeKind(expandedNode->kind));
        root->setProperty("expandedEditorBounds", rectangleToVar(
                expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode)));
    }

    Array<var> panels;
    if (expandedNode != nullptr && expandedNode->kind == NodeKind::TrilinearMesh) {
        const TrimeshWidget* widget = previewResources.findTrimeshWidget(expandedNode->id);

        if (widget != nullptr) {
            panels.add(componentDiagnosticsToVar("trimeshPanel3D", widget->getExpandedPanel3DComponentIfCreated()));
            panels.add(componentDiagnosticsToVar("trimeshPanel2D", widget->getExpandedPanel2DComponentIfCreated()));
        } else {
            panels.add(componentDiagnosticsToVar("trimeshPanel3D", nullptr));
            panels.add(componentDiagnosticsToVar("trimeshPanel2D", nullptr));
        }
    }

    root->setProperty("panels", panels);
    root->setProperty("panelCount", panels.size());
    return root;
}

var NodeCanvas::captureAudioForAutomation(size_t frameCount) const {
    return automationInspector.captureAudio(frameCount);
}

File NodeCanvas::snapshotFile() const {
    return File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("CycleV2")
            .getChildFile("graph-snapshot.xml");
}

bool NodeCanvas::saveGraphToFile(const File& file) {
    if (!document.save(file)) {
        editStatusMessage = "Save failed";
        requestCanvasRepaint();
        return false;
    }

    editStatusMessage = "Saved " + file.getFileName();
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::loadGraphFromFile(const File& file) {
    if (!document.load(file)) {
        editStatusMessage = "Open failed";
        requestCanvasRepaint();
        return false;
    }
    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    spliceTargetEdgeIndex = -1;
    refreshCompiledState();
    editStatusMessage = "Opened " + file.getFileName();
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::saveSnapshot() {
    const File file = snapshotFile();
    if (!document.save(file)) {
        editStatusMessage = "Save failed";
        requestCanvasRepaint();
        return true;
    }

    editStatusMessage = "Saved snapshot";
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::loadSnapshot() {
    const File file = snapshotFile();

    if (!file.existsAsFile()) {
        editStatusMessage = "No snapshot";
        requestCanvasRepaint();
        return true;
    }

    if (!document.load(file)) {
        editStatusMessage = "Load failed";
        requestCanvasRepaint();
        return true;
    }
    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    spliceTargetEdgeIndex = -1;
    refreshCompiledState();
    editStatusMessage = "Loaded snapshot";
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::undo() {
    if (!document.undo()) {
        editStatusMessage = "Nothing to undo";
        requestCanvasRepaint();
        return true;
    }
    return restoreGraphXml({}, "Undo");
}

bool NodeCanvas::redo() {
    if (!document.redo()) {
        editStatusMessage = "Nothing to redo";
        requestCanvasRepaint();
        return true;
    }
    return restoreGraphXml({}, "Redo");
}

bool NodeCanvas::restoreGraphXml(const String& xml, const String& statusMessage) {
    if (xml.isNotEmpty() && !document.loadXml(xml, false)) {
        editStatusMessage = "Restore failed";
        requestCanvasRepaint();
        return true;
    }
    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    spliceTargetEdgeIndex = -1;
    refreshCompiledState();
    editStatusMessage = statusMessage;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::spliceSelectedNodeIntoEdgeAt(Point<float> screenPosition) {
    const Node* node = findNode(selectedNodeId);

    if (node == nullptr) {
        return false;
    }

    const int edgeIndex = findSpliceTargetEdgeAt(screenPosition, selectedNodeId);

    if (edgeIndex < 0 || edgeIndex >= (int) graph.getEdges().size()) {
        return false;
    }

    const Edge originalEdge = graph.getEdges()[(size_t) edgeIndex];
    const bool attachingSpy = node->kind == NodeKind::Spy;

    auto result = attachingSpy
            ? commands.attachSpyToEdge((size_t) edgeIndex, selectedNodeId)
            : commands.spliceNodeIntoEdge((size_t) edgeIndex, selectedNodeId);

    if (result.succeeded()) {
        selectedNodeId = result.nodeId;
        selectedEdgeIndex = -1;
        expandedNodeId = {};
        if (!attachingSpy) {
            spaceNodesAfterSplice(originalEdge.sourceNodeId, result.nodeId, originalEdge.destNodeId);
        }
        refreshCompiledState();
        editStatusMessage = attachingSpy ? "Attached spy to cable" : "Inserted node into cable";
        return true;
    }

    if (result.code == GraphEditCode::ValidationRejected) {
        editStatusMessage = "Cable insert incompatible";
    }

    return false;
}

void NodeCanvas::spaceNodesAfterSplice(
        const String& upstreamNodeId,
        const String& insertedNodeId,
        const String& downstreamNodeId) {
    const Node* upstreamNode = findNode(upstreamNodeId);
    const Node* insertedNode = findNode(insertedNodeId);

    if (upstreamNode == nullptr || insertedNode == nullptr) {
        return;
    }

    constexpr float neighbourSpacing = 56.f;
    const float desiredInsertedLeft = upstreamNode->bounds.getRight() + neighbourSpacing;
    if (insertedNode->bounds.getX() < desiredInsertedLeft) {
        commands.resizeNode(insertedNodeId, insertedNode->bounds.withX(desiredInsertedLeft));
    }

    insertedNode = findNode(insertedNodeId);
    const Node* downstreamNode = findNode(downstreamNodeId);
    if (insertedNode == nullptr || downstreamNode == nullptr) {
        return;
    }

    const float desiredDownstreamLeft = insertedNode->bounds.getRight() + neighbourSpacing;
    const float xOffset = desiredDownstreamLeft - downstreamNode->bounds.getX();

    if (xOffset <= 0.f) {
        return;
    }

    std::vector<String> downstreamNodeIds;
    downstreamNodeIds.push_back(downstreamNodeId);

    for (size_t scanIndex = 0; scanIndex < downstreamNodeIds.size(); ++scanIndex) {
        const String currentNodeId = downstreamNodeIds[scanIndex];

        for (const auto& edge : graph.getEdges()) {
            if (edge.sourceNodeId != currentNodeId
                    || edge.destNodeId == insertedNodeId
                    || containsString(downstreamNodeIds, edge.destNodeId)) {
                continue;
            }

            downstreamNodeIds.push_back(edge.destNodeId);
        }
    }

    commands.translateNodes(downstreamNodeIds, { xOffset, 0.f });
}

bool NodeCanvas::clearSelection() {
    if (selectedNodeId.isEmpty() && expandedNodeId.isEmpty()) {
        return false;
    }

    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::cycleOperationPortLayout(const String& nodeId) {
    const Node* node = findNode(nodeId);

    if (node == nullptr || !isOperationNode(node->kind) || node->inputs.size() < 2 || node->outputs.empty()) {
        return false;
    }

    const OperationPortLayout layout = operationPortLayoutFor(*node);
    if (!commands.editNodePresentation(nodeId, [layout](Node& edited) {
        switch (layout) {
            case OperationPortLayout::Side:
                edited.inputs[0].side = PortSide::Left;
                edited.inputs[1].side = PortSide::Top;
                edited.outputs[0].side = PortSide::Right;
                break;

            case OperationPortLayout::Uptack:
                edited.inputs[0].side = PortSide::Top;
                edited.inputs[1].side = PortSide::Bottom;
                edited.outputs[0].side = PortSide::Right;
                break;

            case OperationPortLayout::Vertical:
                edited.inputs[0].side = PortSide::Left;
                edited.inputs[1].side = PortSide::Bottom;
                edited.outputs[0].side = PortSide::Right;
                break;

            case OperationPortLayout::Tee:
                edited.inputs[0].side = PortSide::Left;
                edited.inputs[1].side = PortSide::Left;
                edited.outputs[0].side = PortSide::Right;
                break;
        }
    }).succeeded()) {
        return false;
    }

    selectedNodeId = nodeId;
    selectedEdgeIndex = -1;
    refreshCompiledState();
    return true;
}

bool NodeCanvas::cycleMeshOutputSide(const String& nodeId) {
    const Node* node = findNode(nodeId);

    if (node == nullptr || !hasOutputSideButton(node->kind) || node->outputs.empty()) {
        return false;
    }

    const PortSide nextSide = nextMeshOutputSide(*node);
    if (!commands.editNodePresentation(nodeId, [nextSide](Node& edited) {
        edited.outputs[0].side = nextSide;
    }).succeeded()) {
        return false;
    }
    selectedNodeId = nodeId;
    selectedEdgeIndex = -1;
    refreshCompiledState();
    return true;
}

bool NodeCanvas::cycleVoiceDomain(const String& nodeId) {
    const Node* node = findNode(nodeId);

    if (node == nullptr || node->kind != NodeKind::VoiceContext) {
        return false;
    }

    const String domain = VoiceContextCompactEditor::nextDomain(*node);
    commands.beginCompoundEdit();
    auto result = commands.setNodeParameter(nodeId, "domain", "Start Domain", domain);

    if (!result.succeeded()) {
        commands.cancelCompoundEdit();
        return false;
    }

    commands.editNodePresentation(nodeId, [&](Node& edited) {
        edited.subtitle = domain == "spectral" ? "spectral start" : "waveform start";
    });
    commands.commitCompoundEdit();
    selectedNodeId = nodeId;
    selectedEdgeIndex = -1;
    draggingNode = false;
    connectingCable = false;
    nodeDragUndoPushed = false;
    refreshCompiledState();
    editStatusMessage = "Voice start domain: " + domain;
    return true;
}

bool NodeCanvas::setVoiceContextParameter(
        const String& parameterId,
        const String& label,
        const String& value,
        const String& statusMessage) {
    const Node* node = findNode(expandedNodeId);

    if (node == nullptr || node->kind != NodeKind::VoiceContext) {
        return false;
    }

    if (parameterValueForNode(*node, parameterId) == value) {
        return true;
    }

    commands.beginCompoundEdit();
    auto result = commands.setNodeParameter(node->id, parameterId, label, value);

    if (!result.succeeded()) {
        commands.cancelCompoundEdit();
        return false;
    }

    if (parameterId == "domain") {
        commands.editNodePresentation(node->id, [&](Node& edited) {
            edited.subtitle = value == "spectral" ? "spectral start" : "waveform start";
        });
    }
    commands.commitCompoundEdit();
    selectedNodeId = node->id;
    selectedEdgeIndex = -1;
    draggingNode = false;
    connectingCable = false;
    nodeDragUndoPushed = false;
    refreshCompiledState();
    editStatusMessage = statusMessage;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::handleVoiceContextEditorClick(
        const Node& node,
        Rectangle<float> panel,
        Point<float> screenPosition) {
    const auto edit = VoiceContextCompactEditor::editAt(node, panel, screenPosition);
    if (!edit.has_value()) {
        return false;
    }

    switch (edit->control) {
        case VoiceContextEdit::Control::Domain:
            return setVoiceContextParameter(
                    "domain",
                    "Start Domain",
                    edit->value,
                    "Voice start domain: " + edit->value);

        case VoiceContextEdit::Control::Octave:
            return setVoiceContextParameter(
                    "octave",
                    "Octave",
                    edit->value,
                    "Octave: " + edit->value);

        case VoiceContextEdit::Control::Pitch:
            return setVoiceContextParameter(
                    "pitch",
                    "Pitch",
                    edit->value,
                    "Pitch: " + edit->value);

        case VoiceContextEdit::Control::Portamento:
            return setVoiceContextParameter(
                    "portamento",
                    "Portamento",
                    edit->value,
                    edit->value == "1" ? "Portamento on" : "Portamento off");

        case VoiceContextEdit::Control::Oversampling:
            return setVoiceContextParameter(
                    "oversampling",
                    "Oversampling",
                    edit->value,
                    "Oversampling: " + edit->value);
    }

    return false;
}

bool NodeCanvas::setTransformMode(const Node& node, TransformMode mode) {
    const String value = TransformCompactEditor::parameterValue(mode);
    if (parameterValueForNode(node, "mode") == value) {
        return true;
    }

    commands.beginCompoundEdit();
    auto result = commands.setNodeParameter(node.id, "mode", "Mode", value);
    if (!result.succeeded()) {
        commands.cancelCompoundEdit();
        return false;
    }

    commands.editNodePresentation(node.id, [&](Node& edited) {
        edited.subtitle = TransformCompactEditor::subtitle(edited.kind, mode);
    });
    commands.commitCompoundEdit();

    selectedNodeId = node.id;
    selectedEdgeIndex = -1;
    draggingNode = false;
    connectingCable = false;
    nodeDragUndoPushed = false;
    refreshCompiledState();
    editStatusMessage = TransformCompactEditor::status(node.kind, mode);
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::handleTransformEditorClick(
        const Node& node,
        Rectangle<float> panel,
        Point<float> screenPosition) {
    const auto mode = TransformCompactEditor::modeAt(node, panel, screenPosition);
    return mode.has_value() && setTransformMode(node, *mode);
}

std::array<String, 6> NodeCanvas::trimeshGuideAttachmentLabelsForNode(const Node& meshNode) {
    std::array<String, 6> labels;

    if (meshNode.kind != NodeKind::TrilinearMesh) {
        return labels;
    }

    TrimeshWidget& widget = trimeshWidgetFor(meshNode.id);
    const int vertexIndex = widget.resolvedSelectedVertexIndexForNode(meshNode);
    const auto& fields = TrimeshGuideAttachmentTarget::fields();

    for (int i = 0; i < (int) fields.size(); ++i) {
        const auto items = TrimeshGuideAttachmentMenu::itemsFor(
                graph,
                meshNode.id,
                vertexIndex,
                fields[(size_t) i]);

        for (const auto& item : items) {
            if (!item.attached) {
                continue;
            }

            labels[(size_t) i] = item.label;
            break;
        }
    }

    return labels;
}

void NodeCanvas::closeNodeEditor() {
    expandedNodeId = {};
    editorHost.close();
    repaintNodeEditor(true);
}

void NodeCanvas::repaintNodeEditor(bool openGl) {
    if (openGl) {
        openGLContext.triggerRepaint();
    }
    requestCanvasRepaint();
}

void NodeCanvas::selectEditedNode(const String& nodeId) {
    selectedNodeId = nodeId;
    selectedEdgeIndex = -1;
}

void NodeCanvas::setNodeEditorStatus(const String& message) {
    editStatusMessage = message;
}

void NodeCanvas::scheduleNodeEditorRefresh() {
    scheduleCompiledStateRefresh();
}

void NodeCanvas::flushNodeEditorRefresh() {
    flushScheduledCompiledStateRefresh();
}

void NodeCanvas::refreshNodeEditorPresentation() {
    refreshCompiledState();
}

Point<float> NodeCanvas::nodeEditorCreationPosition() const {
    return viewportCentreWorld();
}

void NodeCanvas::rebindNodeEditor() {
    updateExpandedEditorHost(findNode(expandedNodeId));
}

Effect2DWidget* NodeCanvas::effect2DWidget(const Node& node) {
    return &effect2DWidgetFor(node);
}

TrimeshWidget* NodeCanvas::trimeshWidget(const Node& node) {
    return &trimeshWidgetFor(node.id);
}

TrimeshRenderProfile NodeCanvas::trimeshRenderProfile(const Node& node) const {
    return renderProfileForNodeOutput(node, "out");
}

std::array<String, 6> NodeCanvas::trimeshGuideLabels(const Node& node) {
    return trimeshGuideAttachmentLabelsForNode(node);
}

bool NodeCanvas::canConnectPorts(const PortAddress& first, const PortAddress& second) const {
    if (first.input == second.input) {
        return false;
    }

    NodeGraph candidate = graph;
    return GraphEditor().connect(candidate, first, second).succeeded();
}

void NodeCanvas::requestCanvasRepaint() {
    renderInvalidation.request(NodeCanvasInvalidation::CanvasRepaint);
}

uint32_t NodeCanvas::availableRenderInvalidations() const {
    return isShowing() ? NodeCanvasInvalidation::CanvasRepaint : 0;
}

void NodeCanvas::flushRenderInvalidations(uint32_t categories) {
    if ((categories & NodeCanvasInvalidation::CanvasRepaint) != 0) {
        Component::repaint();
    }
}

}
