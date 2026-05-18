#include "NodeCanvas.h"

namespace CycleV2 {

namespace {

const Colour kCanvasBackground { 0xff101318 };
const Colour kCanvasGridMajor  { 0x2f5b6370 };
const Colour kCanvasGridMinor  { 0x182f363f };
const Colour kNodeBackground   { 0xff171d24 };
const Colour kNodeHeader       { 0xff202833 };
const Colour kNodeBorder       { 0xff3d4a58 };
const Colour kText             { 0xffe2e8ef };
const Colour kMutedText        { 0xff8793a1 };

float portY(const Node& node, const Port& port) {
    const auto& ports = port.input ? node.inputs : node.outputs;
    int index = 0;

    for (int i = 0; i < (int) ports.size(); ++i) {
        if (ports[(size_t) i].id == port.id) {
            index = i;
            break;
        }
    }

    return node.bounds.getY() + 56.f + (float) index * 28.f;
}

}

NodeCanvas::NodeCanvas() :
        graph(NodeGraph::createDemoGraph()) {
    compileResult = GraphCompiler().compile(graph);

    if (compileResult.succeeded()) {
        runtimeTrace = GraphRuntime().process(graph, compileResult.plan);
    }

    setOpaque(true);
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(false);
    openGLContext.attachTo(*this);
    startTimerHz(30);
}

NodeCanvas::~NodeCanvas() {
    stopTimer();
    openGLContext.detach();
}

void NodeCanvas::paint(Graphics& g) {
    drawGrid(g);
    drawEdges(g);
    drawNodes(g);
    drawMiniMap(g);
}

void NodeCanvas::resized() {
    repaint();
}

void NodeCanvas::newOpenGLContextCreated() {
}

void NodeCanvas::renderOpenGL() {
    OpenGLHelpers::clear(kCanvasBackground);
}

void NodeCanvas::openGLContextClosing() {
}

void NodeCanvas::timerCallback() {
    repaint();
}

void NodeCanvas::drawGrid(Graphics& g) {
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

void NodeCanvas::drawEdges(Graphics& g) {
    for (const auto& edge : graph.getEdges()) {
        const Node* sourceNode = findNode(edge.sourceNodeId);
        const Node* destNode = findNode(edge.destNodeId);

        if (sourceNode == nullptr || destNode == nullptr) {
            continue;
        }

        const Port* sourcePort = findPort(*sourceNode, edge.sourcePortId, false);
        const Port* destPort = findPort(*destNode, edge.destPortId, true);

        if (sourcePort == nullptr || destPort == nullptr) {
            continue;
        }

        auto source = getPortLocation(*sourceNode, *sourcePort).centre;
        auto dest = getPortLocation(*destNode, *destPort).centre;
        Path cable = createCablePath(source, dest, edge.attachment);
        Colour colour = colourForDomain(edge.domain);

        if (edge.attachment) {
            Path dashedCable;
            PathStrokeType stroke(2.0f, PathStrokeType::curved, PathStrokeType::rounded);
            Array<float> dashes { 8.f, 7.f };
            stroke.createDashedStroke(dashedCable, cable, dashes.getRawDataPointer(), dashes.size());
            g.setColour(colour.withAlpha(0.32f));
            g.strokePath(dashedCable, PathStrokeType(7.f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour(colour.withAlpha(0.92f));
            g.strokePath(dashedCable, PathStrokeType(2.f, PathStrokeType::curved, PathStrokeType::rounded));
        } else {
            g.setColour(colour.withAlpha(0.18f));
            g.strokePath(cable, PathStrokeType(9.f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour(colour.withAlpha(0.92f));
            g.strokePath(cable, PathStrokeType(3.f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
}

void NodeCanvas::drawNodes(Graphics& g) {
    for (const auto& node : graph.getNodes()) {
        drawNode(g, node);
    }
}

void NodeCanvas::drawNode(Graphics& g, const Node& node) {
    Rectangle<float> bounds = toScreen(node.bounds);
    const float corner = 8.f;

    g.setColour(Colours::black.withAlpha(0.32f));
    g.fillRoundedRectangle(bounds.translated(0.f, 9.f), corner);

    g.setColour(kNodeBackground);
    g.fillRoundedRectangle(bounds, corner);

    auto header = bounds.removeFromTop(42.f * zoom);
    g.setColour(kNodeHeader);
    g.fillRoundedRectangle(header, corner);
    g.fillRect(header.withTrimmedTop(header.getHeight() - corner));

    g.setColour(kNodeBorder);
    g.drawRoundedRectangle(toScreen(node.bounds), corner, 1.2f);

    g.setFont(FontOptions(15.f * zoom, Font::bold));
    g.setColour(kText);
    g.drawText(node.title, header.reduced(13.f * zoom, 4.f * zoom), Justification::centredLeft);

    g.setFont(FontOptions(10.5f * zoom));
    g.setColour(kMutedText);
    g.drawText(node.subtitle, header.reduced(13.f * zoom, 4.f * zoom), Justification::centredRight);

    const int executionIndex = executionIndexForNode(node.id);
    if (executionIndex >= 0) {
        Rectangle<float> badge = bounds.withSizeKeepingCentre(42.f * zoom, 20.f * zoom);
        badge.setX(bounds.getRight() - badge.getWidth() - 11.f * zoom);
        badge.setY(bounds.getY() + 49.f * zoom);

        g.setColour(Colour(0xff0e1318));
        g.fillRoundedRectangle(badge, 5.f * zoom);
        g.setColour(Colour(0xff354050));
        g.drawRoundedRectangle(badge, 5.f * zoom, 1.f);
        g.setColour(kText);
        g.setFont(FontOptions(10.f * zoom, Font::bold));
        g.drawText("#" + String(executionIndex + 1), badge, Justification::centred);
    }

    const RuntimeNodeTrace* traceNode = findRuntimeTrace(node.id);
    if (traceNode != nullptr && !traceNode->attachments.empty()) {
        Rectangle<float> attachmentBadge = bounds.withSizeKeepingCentre(72.f * zoom, 20.f * zoom);
        attachmentBadge.setX(bounds.getX() + 11.f * zoom);
        attachmentBadge.setY(bounds.getY() + 49.f * zoom);

        g.setColour(colourForDomain(PortDomain::EnvelopeSignal).withAlpha(0.16f));
        g.fillRoundedRectangle(attachmentBadge, 5.f * zoom);
        g.setColour(colourForDomain(PortDomain::EnvelopeSignal).withAlpha(0.64f));
        g.drawRoundedRectangle(attachmentBadge, 5.f * zoom, 1.f);
        g.setColour(kText);
        g.setFont(FontOptions(9.f * zoom));
        g.drawText("attach " + String((int) traceNode->attachments.size()),
                   attachmentBadge, Justification::centred);
    }

    auto nodeBounds = toScreen(node.bounds);
    auto preview = nodeBounds.reduced(13.f * zoom).withTrimmedTop(49.f * zoom).withTrimmedBottom(12.f * zoom);
    drawPreview(g, node, preview);

    auto drawPort = [&](const Node& portNode, const Port& port) {
        auto location = getPortLocation(portNode, port);
        Colour colour = colourForDomain(port.domain);

        g.setColour(colour.withAlpha(0.22f));
        g.fillEllipse(location.bounds.expanded(3.f));
        g.setColour(colour);
        g.fillEllipse(location.bounds);

        Rectangle<float> labelBounds = location.bounds.withSizeKeepingCentre(92.f * zoom, 18.f * zoom);
        labelBounds.setY(location.bounds.getY() - 3.f * zoom);

        if (port.input) {
            labelBounds.setX(location.bounds.getRight() + 7.f * zoom);
        } else {
            labelBounds.setRight(location.bounds.getX() - 7.f * zoom);
        }

        g.setFont(FontOptions(9.5f * zoom));
        g.setColour(kMutedText);
        g.drawText(port.label, labelBounds, port.input ? Justification::centredLeft
                                                       : Justification::centredRight);
    };

    for (const auto& port : node.inputs) {
        drawPort(node, port);
    }

    for (const auto& port : node.outputs) {
        drawPort(node, port);
    }
}

void NodeCanvas::drawPreview(Graphics& g, const Node& node, Rectangle<float> area) {
    if (area.getWidth() < 20.f || area.getHeight() < 20.f) {
        return;
    }

    g.setColour(Colour(0xff0e1318));
    g.fillRoundedRectangle(area, 5.f);
    g.setColour(Colour(0xff26313d));
    g.drawRoundedRectangle(area, 5.f, 1.f);

    if (node.kind == NodeKind::TrilinearWaveSurface) {
        Path surface;
        for (int i = 0; i < 8; ++i) {
            float x0 = area.getX() + (float) i * area.getWidth() / 8.f;
            float x1 = area.getX() + (float) (i + 1) * area.getWidth() / 8.f;
            float y0 = area.getY() + area.getHeight() * (0.28f + 0.08f * std::sin((float) i));
            float y1 = area.getBottom() - area.getHeight() * (0.20f + 0.07f * std::cos((float) i));
            Line<float> line({ x0, y1 }, { x1, y0 });
            g.setColour(Colour(0xff35d6d2).withAlpha(0.40f));
            g.drawLine(line, 1.2f);
        }
        g.setColour(Colour(0xff35d6d2).withAlpha(0.18f));
        g.fillRect(area.reduced(area.getWidth() * 0.14f, area.getHeight() * 0.24f));
        return;
    }

    Path curve;
    const int steps = 42;
    for (int i = 0; i < steps; ++i) {
        float t = (float) i / (float) (steps - 1);
        float y = 0.5f + 0.28f * std::sin(t * MathConstants<float>::twoPi * 1.35f + node.id.hashCode() * 0.001f);
        Point<float> p(area.getX() + t * area.getWidth(), area.getY() + y * area.getHeight());
        if (i == 0) {
            curve.startNewSubPath(p);
        } else {
            curve.lineTo(p);
        }
    }

    Colour colour = node.outputs.empty() ? Colour(0xff9aa5b2) : colourForDomain(node.outputs.front().domain);

    if (node.kind == NodeKind::Output) {
        const float left = area.getHeight() * 0.62f;
        const float right = area.getHeight() * 0.44f;
        auto leftMeter = area.withHeight(left).withY(area.getBottom() - left);
        auto rightMeter = area.withHeight(right).withY(area.getBottom() - right);
        g.setColour(Colour(0xff35d6d2).withAlpha(0.42f));
        g.fillRoundedRectangle(leftMeter.removeFromLeft(area.getWidth() * 0.42f), 3.f);
        g.fillRoundedRectangle(rightMeter.removeFromRight(area.getWidth() * 0.42f), 3.f);
        return;
    }

    g.setColour(colour.withAlpha(0.20f));
    g.fillPath(curve);
    g.setColour(colour.withAlpha(0.95f));
    g.strokePath(curve, PathStrokeType(2.f * zoom, PathStrokeType::curved, PathStrokeType::rounded));
}

void NodeCanvas::drawMiniMap(Graphics& g) {
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

    auto project = [&](Rectangle<float> r) {
        float x = jmap(r.getX(), graphBounds.getX(), graphBounds.getRight(), map.getX(), map.getRight());
        float y = jmap(r.getY(), graphBounds.getY(), graphBounds.getBottom(), map.getY(), map.getBottom());
        float w = r.getWidth() / graphBounds.getWidth() * map.getWidth();
        float h = r.getHeight() / graphBounds.getHeight() * map.getHeight();
        return Rectangle<float>(x, y, w, h);
    };

    for (const auto& node : graph.getNodes()) {
        g.setColour(Colour(0xff778596).withAlpha(0.62f));
        g.fillRoundedRectangle(project(node.bounds), 2.f);
    }

    Rectangle<float> viewWorld((-pan.x) / zoom, (-pan.y) / zoom,
                               (float) getWidth() / zoom, (float) getHeight() / zoom);
    g.setColour(Colour(0xff35d6d2).withAlpha(0.24f));
    g.fillRoundedRectangle(project(viewWorld), 3.f);
    g.setColour(Colour(0xff35d6d2).withAlpha(0.85f));
    g.drawRoundedRectangle(project(viewWorld), 3.f, 1.f);
}

Point<float> NodeCanvas::toScreen(Point<float> p) const {
    return { pan.x + p.x * zoom, pan.y + p.y * zoom };
}

Rectangle<float> NodeCanvas::toScreen(Rectangle<float> r) const {
    return { pan.x + r.getX() * zoom, pan.y + r.getY() * zoom,
             r.getWidth() * zoom, r.getHeight() * zoom };
}

NodeCanvas::PortLocation NodeCanvas::getPortLocation(const Node& node, const Port& port) const {
    constexpr float size = 11.f;
    float x = port.input ? node.bounds.getX() : node.bounds.getRight();
    Point<float> centre = toScreen(Point<float>(x, portY(node, port)));
    Rectangle<float> bounds(centre.x - size * 0.5f, centre.y - size * 0.5f, size, size);
    return { bounds, centre };
}

const Node* NodeCanvas::findNode(const String& id) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
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

Path NodeCanvas::createCablePath(Point<float> source, Point<float> dest, bool attachment) const {
    Path path;
    path.startNewSubPath(source);

    float dx = jmax(90.f * zoom, std::abs(dest.x - source.x) * 0.45f);
    float lift = attachment ? -30.f * zoom : 0.f;
    Point<float> c1(source.x + dx, source.y + lift);
    Point<float> c2(dest.x - dx, dest.y + lift);
    path.cubicTo(c1, c2, dest);

    return path;
}

}
