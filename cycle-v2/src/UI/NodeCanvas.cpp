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
    refreshCompiledState();

    setOpaque(true);
    setWantsKeyboardFocus(true);
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
    drawConnectionPreview(g);
    drawNodes(g);
    drawMiniMap(g);
    drawGraphStatus(g);
}

void NodeCanvas::resized() {
    repaint();
}

void NodeCanvas::mouseDown(const MouseEvent& event) {
    grabKeyboardFocus();
    editStatusMessage = {};
    dragStartPan = pan;

    PortAddress hitPort;
    if (findPortAt(event.position, hitPort)) {
        connectingPort = hitPort;
        connectingPoint = event.position;
        connectingCable = true;
        draggingNode = false;
        selectedNodeId = hitPort.nodeId;
        repaint();
        return;
    }

    const Node* hitNode = findNodeAt(toWorld(event.position));
    selectedNodeId = hitNode != nullptr ? hitNode->id : String();
    draggingNode = hitNode != nullptr;

    if (hitNode != nullptr) {
        dragStartNodeBounds = hitNode->bounds;

        if (event.getNumberOfClicks() >= 2) {
            expandedNodeId = expandedNodeId == hitNode->id ? String() : hitNode->id;
        }
    } else {
        expandedNodeId = {};
    }

    repaint();
}

void NodeCanvas::mouseDrag(const MouseEvent& event) {
    if (connectingCable) {
        connectingPoint = event.position;
    } else if (draggingNode) {
        Node* node = findMutableNode(selectedNodeId);

        if (node != nullptr) {
            const auto offset = event.getOffsetFromDragStart().toFloat() / zoom;
            node->bounds = dragStartNodeBounds.translated(offset.x, offset.y);
        }
    } else {
        pan = dragStartPan + event.getOffsetFromDragStart().toFloat();
    }

    repaint();
}

void NodeCanvas::mouseUp(const MouseEvent& event) {
    if (!connectingCable) {
        return;
    }

    PortAddress destPort;
    connectingCable = false;

    if (findPortAt(event.position, destPort)) {
        auto result = GraphEditor().connect(graph, connectingPort, destPort);

        if (result.succeeded()) {
            refreshCompiledState();
            editStatusMessage = "Connected";
        } else if (result.code == GraphEditCode::ValidationRejected) {
            editStatusMessage = "Incompatible connection";
        } else {
            editStatusMessage = "Connection cancelled";
        }
    }

    repaint();
}

void NodeCanvas::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) {
    const auto mouse = event.position;
    const auto beforeZoom = toWorld(mouse);
    const float zoomFactor = std::pow(1.12f, wheel.deltaY * 6.f);
    zoom = jlimit(0.28f, 1.4f, zoom * zoomFactor);
    const auto afterZoom = toWorld(mouse);
    pan += (afterZoom - beforeZoom) * zoom;
    repaint();
}

bool NodeCanvas::keyPressed(const KeyPress& key) {
    if (key == KeyPress::escapeKey) {
        return clearSelection();
    }

    if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey) {
        if (selectedNodeId.isEmpty()) {
            return false;
        }

        auto result = GraphEditor().removeNode(graph, selectedNodeId);

        if (!result.succeeded()) {
            return false;
        }

        expandedNodeId = {};
        selectedNodeId = {};
        refreshCompiledState();
        editStatusMessage = "Node deleted";
        repaint();
        return true;
    }

    return false;
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

void NodeCanvas::drawConnectionPreview(Graphics& g) {
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
    const Path cable = createCablePath(source, dest, false);
    const Colour colour = colourForDomain(port->domain);

    g.setColour(colour.withAlpha(0.18f));
    g.strokePath(cable, PathStrokeType(9.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour(colour.withAlpha(0.88f));
    g.strokePath(cable, PathStrokeType(3.f, PathStrokeType::curved, PathStrokeType::rounded));
}

void NodeCanvas::drawNodes(Graphics& g) {
    for (const auto& node : graph.getNodes()) {
        drawNode(g, node);
    }

    if (const Node* node = findNode(expandedNodeId)) {
        drawExpandedEditor(g, *node);
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

    if (node.id == selectedNodeId) {
        g.setColour(colourForDomain(PortDomain::TimeSignal).withAlpha(0.82f));
        g.drawRoundedRectangle(toScreen(node.bounds).expanded(2.f), corner + 2.f, 2.f);
    }

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
        g.drawText(port.label + " " + labelForChannelLayout(port.channelLayout),
                   labelBounds,
                   port.input ? Justification::centredLeft
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

void NodeCanvas::drawExpandedEditor(Graphics& g, const Node& node) {
    Rectangle<float> anchor = toScreen(node.bounds);
    Rectangle<float> panel(anchor.getRight() + 18.f, anchor.getY(), 360.f, 260.f);

    if (panel.getRight() > (float) getWidth() - 18.f) {
        panel.setRight(anchor.getX() - 18.f);
    }

    if (panel.getBottom() > (float) getHeight() - 18.f) {
        panel.setBottom((float) getHeight() - 18.f);
    }

    g.setColour(Colours::black.withAlpha(0.38f));
    g.fillRoundedRectangle(panel.translated(0.f, 10.f), 8.f);
    g.setColour(Colour(0xff141a21));
    g.fillRoundedRectangle(panel, 8.f);
    g.setColour(colourForDomain(PortDomain::TimeSignal).withAlpha(0.72f));
    g.drawRoundedRectangle(panel, 8.f, 1.5f);

    auto header = panel.removeFromTop(44.f);
    g.setColour(Colour(0xff202833));
    g.fillRoundedRectangle(header, 8.f);
    g.fillRect(header.withTrimmedTop(header.getHeight() - 8.f));
    g.setColour(kText);
    g.setFont(FontOptions(15.f, Font::bold));
    g.drawText(node.title, header.reduced(13.f, 4.f), Justification::centredLeft);
    g.setColour(kMutedText);
    g.setFont(FontOptions(10.5f));
    g.drawText(labelForNodeKind(node.kind), header.reduced(13.f, 4.f), Justification::centredRight);

    auto content = panel.reduced(13.f, 12.f);
    auto preview = content.removeFromTop(126.f);
    drawPreview(g, node, preview);
    content.removeFromTop(10.f);

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
            g.drawText(port.label + " " + labelForChannelLayout(port.channelLayout),
                       row.withTrimmedLeft(12.f), Justification::centredLeft);
        }
    };

    drawPorts(left, node.inputs, "Inputs");
    drawPorts(right, node.outputs, "Outputs");
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

Point<float> NodeCanvas::toScreen(Point<float> p) const {
    return { pan.x + p.x * zoom, pan.y + p.y * zoom };
}

Point<float> NodeCanvas::toWorld(Point<float> p) const {
    return { (p.x - pan.x) / zoom, (p.y - pan.y) / zoom };
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
    for (const auto& node : graph.getNodes()) {
        for (const auto& port : node.inputs) {
            if (getPortLocation(node, port).bounds.expanded(5.f).contains(screenPosition)) {
                result = { node.id, port.id, true };
                return true;
            }
        }

        for (const auto& port : node.outputs) {
            if (getPortLocation(node, port).bounds.expanded(5.f).contains(screenPosition)) {
                result = { node.id, port.id, false };
                return true;
            }
        }
    }

    return false;
}

const Node* NodeCanvas::findNode(const String& id) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

Node* NodeCanvas::findMutableNode(const String& id) {
    for (auto& node : graph.getNodesForEditing()) {
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

void NodeCanvas::refreshCompiledState() {
    compileResult = GraphCompiler().compile(graph);
    runtimeTrace = {};

    if (compileResult.succeeded()) {
        runtimeTrace = GraphRuntime().process(graph, compileResult.plan);
    }
}

bool NodeCanvas::clearSelection() {
    if (selectedNodeId.isEmpty() && expandedNodeId.isEmpty()) {
        return false;
    }

    selectedNodeId = {};
    expandedNodeId = {};
    repaint();
    return true;
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
