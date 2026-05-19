#include "NodeCanvas.h"

#include <iterator>

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
constexpr float kCableReferenceZoom = 0.58f;
constexpr float kCableStrokeScale = 0.70f;
constexpr float kPaletteWidth = 107.f;
constexpr float kPaletteRowHeight = 40.f;

float cableScaleForZoom(float zoom) {
    return zoom / kCableReferenceZoom * kCableStrokeScale;
}

float portScaleForZoom(float zoom) {
    return zoom / kCableReferenceZoom;
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
    drawEdgeLegend(g);
    drawNodePalette(g);
    drawHoverConsole(g);
}

void NodeCanvas::resized() {
    repaint();
}

void NodeCanvas::mouseMove(const MouseEvent& event) {
    lastMousePosition = event.position;
    repaint();
}

void NodeCanvas::mouseDown(const MouseEvent& event) {
    grabKeyboardFocus();
    editStatusMessage = {};
    dragStartPan = pan;
    lastMousePosition = event.position;

    NodeKind paletteKind;
    if (findPaletteKindAt(event.position, paletteKind)) {
        const String beforeEdit = GraphSerializer().toXmlString(graph);
        auto result = GraphEditor().addNode(graph, paletteKind, paletteCreationWorldPosition(event.position));

        if (result.succeeded()) {
            pushUndoSnapshot(beforeEdit);
            selectedNodeId = result.nodeId;
            expandedNodeId = {};
            selectedEdgeIndex = -1;
            if (const Node* node = findNode(result.nodeId)) {
                dragStartNodeBounds = node->bounds;
                draggingNode = true;
                nodeDragUndoPushed = false;
            }
            connectingCable = false;
            refreshCompiledState();
            editStatusMessage = "Node added";
            repaint();
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
        repaint();
        return;
    }

    if (expandedNodeId.isNotEmpty()) {
        const Rectangle<float> available = getLocalBounds().toFloat().reduced(28.f);
        const float width = jmin(available.getWidth(), jmax(420.f, available.getWidth() * 0.80f));
        const float height = jmin(available.getHeight(), jmax(300.f, available.getHeight() * 0.80f));
        const auto panel = Rectangle<float>(width, height).withCentre(available.getCentre());
        const auto closeButton = Rectangle<float>(24.f, 24.f).withCentre({ panel.getRight() - 24.f, panel.getY() + 22.f });

        if (closeButton.contains(event.position)) {
            expandedNodeId = {};
            repaint();
            return;
        }
    }

    selectedEdgeIndex = findEdgeAt(event.position);

    if (selectedEdgeIndex >= 0) {
        if (event.mods.isCtrlDown()) {
            const String beforeEdit = GraphSerializer().toXmlString(graph);
            auto result = GraphEditor().removeEdgeAt(graph, (size_t) selectedEdgeIndex);

            if (result.succeeded()) {
                pushUndoSnapshot(beforeEdit);
                selectedEdgeIndex = -1;
                refreshCompiledState();
                editStatusMessage = "Edge cut";
            }

            repaint();
            return;
        }

        selectedNodeId = {};
        draggingNode = false;
        repaint();
        return;
    }

    const Node* hitNode = findNodeAt(toWorld(event.position));
    selectedNodeId = hitNode != nullptr ? hitNode->id : String();
    selectedEdgeIndex = -1;
    draggingNode = hitNode != nullptr;

    if (hitNode != nullptr) {
        dragStartNodeBounds = hitNode->bounds;
        nodeDragUndoPushed = false;

        if (event.getNumberOfClicks() >= 2) {
            expandedNodeId = expandedNodeId == hitNode->id ? String() : hitNode->id;
        }
    } else {
        expandedNodeId = {};
    }

    repaint();
}

void NodeCanvas::mouseDrag(const MouseEvent& event) {
    lastMousePosition = event.position;

    if (connectingCable) {
        PortAddress hitPort;
        connectingPoint = findConnectablePortAt(event.position, connectingPort, hitPort)
                ? getPortLocation(hitPort).centre
                : event.position;
    } else if (draggingNode) {
        Node* node = findMutableNode(selectedNodeId);

        if (node != nullptr) {
            if (!nodeDragUndoPushed) {
                pushUndoSnapshot();
                nodeDragUndoPushed = true;
            }

            const auto offset = event.getOffsetFromDragStart().toFloat() / zoom;
            node->bounds = dragStartNodeBounds.translated(offset.x, offset.y);
        }
    } else {
        pan = dragStartPan + event.getOffsetFromDragStart().toFloat();
    }

    repaint();
}

void NodeCanvas::mouseUp(const MouseEvent& event) {
    lastMousePosition = event.position;

    if (!connectingCable) {
        return;
    }

    PortAddress destPort;
    connectingCable = false;

    if (findPortAt(event.position, destPort)) {
        const String beforeEdit = GraphSerializer().toXmlString(graph);
        auto result = GraphEditor().connect(graph, connectingPort, destPort);

        if (result.succeeded()) {
            pushUndoSnapshot(beforeEdit);
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
    ignoreUnused(event);
    constexpr float panScale = 720.f;
    pan += Point<float>(wheel.deltaX * panScale, wheel.deltaY * panScale);
    repaint();
}

void NodeCanvas::mouseMagnify(const MouseEvent& event, float scaleFactor) {
    const auto mouse = event.position;
    const auto beforeZoom = toWorld(mouse);
    zoom = jlimit(0.28f, 1.4f, zoom * scaleFactor);
    const auto afterZoom = toWorld(mouse);
    pan += (afterZoom - beforeZoom) * zoom;
    repaint();
}

bool NodeCanvas::keyPressed(const KeyPress& key) {
    const bool commandDown = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    const int keyCode = key.getKeyCode();
    const juce_wchar keyChar = CharacterFunctions::toLowerCase(key.getTextCharacter());

    if (commandDown && (keyChar == 's' || keyCode == 's' || keyCode == 'S')) {
        return saveSnapshot();
    }

    if (commandDown && (keyChar == 'z' || keyCode == 'z' || keyCode == 'Z')) {
        return key.getModifiers().isShiftDown() ? redo() : undo();
    }

    if (commandDown && (keyChar == 'y' || keyCode == 'y' || keyCode == 'Y')) {
        return redo();
    }

    if (commandDown && (keyChar == 'o' || keyCode == 'o' || keyCode == 'O')) {
        return loadSnapshot();
    }

    if (key == KeyPress::escapeKey) {
        return clearSelection();
    }

    if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey) {
        if (selectedEdgeIndex >= 0) {
            const String beforeEdit = GraphSerializer().toXmlString(graph);
            auto result = GraphEditor().removeEdgeAt(graph, (size_t) selectedEdgeIndex);

            if (!result.succeeded()) {
                return false;
            }

            pushUndoSnapshot(beforeEdit);
            selectedEdgeIndex = -1;
            refreshCompiledState();
            editStatusMessage = "Edge deleted";
            repaint();
            return true;
        }

        if (selectedNodeId.isEmpty()) {
            return false;
        }

        const String beforeEdit = GraphSerializer().toXmlString(graph);
        auto result = GraphEditor().removeNode(graph, selectedNodeId);

        if (!result.succeeded()) {
            return false;
        }

        pushUndoSnapshot(beforeEdit);
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
    const auto mouse = getMouseXYRelative().toFloat();

    if (getLocalBounds().toFloat().contains(mouse) && mouse != lastMousePosition) {
        lastMousePosition = mouse;
    }

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
    const auto& edges = graph.getEdges();

    for (int edgeIndex = 0; edgeIndex < (int) edges.size(); ++edgeIndex) {
        const auto& edge = edges[(size_t) edgeIndex];
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

        const bool selected = edgeIndex == selectedEdgeIndex;
        const float cableScale = cableScaleForZoom(zoom);

        if (edge.attachment) {
            Path dashedCable;
            PathStrokeType stroke(2.0f * cableScale, PathStrokeType::curved, PathStrokeType::rounded);
            Array<float> dashes { 8.f * cableScale, 7.f * cableScale };
            stroke.createDashedStroke(dashedCable, cable, dashes.getRawDataPointer(), dashes.size());
            g.setColour(colour.withAlpha(selected ? 0.46f : 0.32f));
            g.strokePath(dashedCable, PathStrokeType((selected ? 10.f : 7.f) * cableScale, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour(colour.withAlpha(0.92f));
            g.strokePath(dashedCable, PathStrokeType((selected ? 3.f : 2.f) * cableScale, PathStrokeType::curved, PathStrokeType::rounded));
        } else {
            g.setColour(colour.withAlpha(selected ? 0.28f : 0.18f));
            g.strokePath(cable, PathStrokeType((selected ? 12.f : 9.f) * cableScale, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour(colour.withAlpha(0.92f));
            g.strokePath(cable, PathStrokeType((selected ? 4.f : 3.f) * cableScale, PathStrokeType::curved, PathStrokeType::rounded));
        }

        const float endpointSize = (selected ? 14.f : 11.f) * cableScale;
        Rectangle<float> sourceMarker(source.x - endpointSize * 0.5f, source.y - endpointSize * 0.5f,
                                      endpointSize, endpointSize);
        Rectangle<float> destMarker(dest.x - endpointSize * 0.5f, dest.y - endpointSize * 0.5f,
                                    endpointSize, endpointSize);

        g.setColour(kCanvasBackground.withAlpha(0.92f));
        g.fillEllipse(sourceMarker);
        g.setColour(colour.withAlpha(0.96f));
        g.drawEllipse(sourceMarker, (selected ? 2.4f : 1.8f) * cableScale);
        g.fillEllipse(destMarker.reduced((selected ? 1.5f : 2.f) * cableScale));
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
    const float cableScale = cableScaleForZoom(zoom);

    g.setColour(colour.withAlpha(0.18f));
    g.strokePath(cable, PathStrokeType(9.f * cableScale, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour(colour.withAlpha(0.88f));
    g.strokePath(cable, PathStrokeType(3.f * cableScale, PathStrokeType::curved, PathStrokeType::rounded));

    const float markerSize = 11.f * cableScale;
    Rectangle<float> startMarker(source.x - markerSize * 0.5f, source.y - markerSize * 0.5f, markerSize, markerSize);
    Rectangle<float> destMarker(dest.x - markerSize * 0.5f, dest.y - markerSize * 0.5f, markerSize, markerSize);
    g.setColour(kCanvasBackground.withAlpha(0.92f));
    g.fillEllipse(startMarker);
    g.setColour(colour.withAlpha(0.96f));
    g.drawEllipse(startMarker, 1.8f * cableScale);
    g.fillEllipse(destMarker.reduced(2.f * cableScale));
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

    auto nodeBounds = toScreen(node.bounds);
    auto preview = nodeBounds.withTrimmedTop(42.f * zoom).reduced(1.f * zoom);
    drawPreview(g, node, preview);

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
    if (area.getWidth() < 20.f || area.getHeight() < 20.f) {
        return;
    }

    g.setColour(Colour(0xff0e1318));
    g.fillRoundedRectangle(area, 5.f);
    g.setColour(Colour(0xff26313d));
    g.drawRoundedRectangle(area, 5.f, 1.f);

    if (node.kind == NodeKind::Fft || node.kind == NodeKind::Ifft) {
        const bool inverse = node.kind == NodeKind::Ifft;
        const Rectangle<float> icon = area.reduced(jmax(8.f, area.getWidth() * 0.22f), area.getHeight() * 0.22f);
        const Colour time = colourForDomain(PortDomain::TimeSignal);
        const Colour mag = colourForDomain(PortDomain::SpectralMagnitudeSignal);
        const Colour phase = colourForDomain(PortDomain::SpectralPhaseSignal);

        if (inverse) {
            Path wave;
            for (int i = 0; i < 32; ++i) {
                const float t = (float) i / 31.f;
                const Point<float> p(
                        icon.getX() + t * icon.getWidth(),
                        icon.getCentreY() + std::sin(t * MathConstants<float>::twoPi) * icon.getHeight() * 0.30f);

                if (i == 0) {
                    wave.startNewSubPath(p);
                } else {
                    wave.lineTo(p);
                }
            }

            g.setColour(time.withAlpha(0.90f));
            g.strokePath(wave, PathStrokeType(2.f, PathStrokeType::curved, PathStrokeType::rounded));
        } else {
            const float barWidth = icon.getWidth() / 7.f;

            for (int i = 0; i < 6; ++i) {
                const float height = icon.getHeight() * (0.25f + (float) ((i * 17) % 5) * 0.13f);
                Rectangle<float> bar(icon.getX() + (float) i * barWidth, icon.getBottom() - height,
                                     barWidth * 0.52f, height);
                g.setColour((i % 2 == 0 ? mag : phase).withAlpha(0.78f));
                g.fillRoundedRectangle(bar, 2.f);
            }
        }

        g.setColour(kMutedText.withAlpha(0.72f));
        g.setFont(FontOptions(jmin(18.f, area.getHeight() * 0.30f), Font::bold));
        g.drawText(inverse ? "IFFT" : "FFT", area, Justification::centredBottom);
        return;
    }

    if (node.kind == NodeKind::StereoSplit || node.kind == NodeKind::StereoJoin) {
        const bool split = node.kind == NodeKind::StereoSplit;
        const Colour colour = colourForDomain(PortDomain::TimeSignal);
        const float y = area.getCentreY();
        const float left = area.getX() + area.getWidth() * 0.28f;
        const float right = area.getRight() - area.getWidth() * 0.28f;

        g.setColour(colour.withAlpha(0.85f));
        g.drawLine(Line<float>({ left, y }, { right, y - area.getHeight() * 0.18f }), 2.f);
        g.drawLine(Line<float>({ left, y }, { right, y + area.getHeight() * 0.18f }), 2.f);
        g.setFont(FontOptions(jmin(18.f, area.getHeight() * 0.30f), Font::bold));
        g.setColour(kMutedText.withAlpha(0.72f));
        g.drawText(split ? "SPLIT" : "JOIN", area, Justification::centredBottom);
        return;
    }

    if (node.kind == NodeKind::Output) {
        const Colour colour = colourForDomain(PortDomain::TimeSignal);
        Rectangle<float> meterArea = area.reduced(area.getWidth() * 0.28f, area.getHeight() * 0.18f);
        const float width = meterArea.getWidth() * 0.32f;

        g.setColour(colour.withAlpha(0.18f));
        g.fillRoundedRectangle(meterArea.removeFromLeft(width), 2.f);
        g.fillRoundedRectangle(meterArea.removeFromRight(width), 2.f);
        g.setColour(kMutedText.withAlpha(0.72f));
        g.setFont(FontOptions(jmin(18.f, area.getHeight() * 0.30f), Font::bold));
        g.drawText("OUT", area, Justification::centredBottom);
        return;
    }

    if (node.kind == NodeKind::TrilinearWaveSurface || node.kind == NodeKind::TrilinearMesh) {
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

    if (node.kind == NodeKind::SpectralMagnitudeProcessor) {
        drawSpectrumBars(g, area.reduced(8.f), colourForDomain(PortDomain::SpectralMagnitudeSignal), node.id.hashCode());
        return;
    }

    if (node.kind == NodeKind::SpectralPhaseProcessor) {
        drawPhaseTrace(g, area.reduced(8.f), colourForDomain(PortDomain::SpectralPhaseSignal), node.id.hashCode());
        return;
    }

    if (node.kind == NodeKind::Envelope) {
        drawEnvelopeCurve(g, area.reduced(8.f));
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

    if (node.kind == NodeKind::Add) {
        g.setColour(kMutedText.withAlpha(0.76f));
        g.setFont(FontOptions(jmin(36.f, area.getHeight() * 0.58f), Font::bold));
        g.drawText("+", area, Justification::centred);
        return;
    }

    if (node.kind == NodeKind::Multiply) {
        g.setColour(kMutedText.withAlpha(0.76f));
        g.setFont(FontOptions(jmin(36.f, area.getHeight() * 0.58f), Font::bold));
        g.drawText("x", area, Justification::centred);
        return;
    }

    g.setColour(colour.withAlpha(0.20f));
    g.fillPath(curve);
    g.setColour(colour.withAlpha(0.95f));
    g.strokePath(curve, PathStrokeType(2.f * zoom, PathStrokeType::curved, PathStrokeType::rounded));
}

void NodeCanvas::drawSpectrumBars(Graphics& g, Rectangle<float> area, Colour colour, int seed) {
    Random random(seed);
    const int bars = 34;
    const float barWidth = area.getWidth() / (float) bars;

    g.setColour(colour.withAlpha(0.08f));
    g.fillRect(area);

    for (int i = 0; i < bars; ++i) {
        const float t = (float) i / (float) (bars - 1);
        const float falloff = 1.f - t * 0.72f;
        const float jitter = 0.35f + random.nextFloat() * 0.55f;
        const float height = area.getHeight() * jlimit(0.08f, 0.98f, falloff * jitter);
        Rectangle<float> bar(
                area.getX() + (float) i * barWidth,
                area.getBottom() - height,
                jmax(1.f, barWidth - 1.f),
                height);

        g.setColour(colour.withAlpha(0.28f));
        g.fillRect(bar);
        g.setColour(colour.withAlpha(0.82f));
        g.drawVerticalLine(roundToInt(bar.getCentreX()), bar.getY(), bar.getBottom());
    }
}

void NodeCanvas::drawPhaseTrace(Graphics& g, Rectangle<float> area, Colour colour, int seed) {
    Random random(seed ^ 0x4f3759df);
    Path trace;
    const int steps = 48;

    for (int i = 0; i < steps; ++i) {
        const float t = (float) i / (float) (steps - 1);
        const float y = 0.5f + (random.nextFloat() - 0.5f) * 0.72f;
        Point<float> p(area.getX() + t * area.getWidth(), area.getY() + y * area.getHeight());

        if (i == 0) {
            trace.startNewSubPath(p);
        } else {
            trace.lineTo(p);
        }
    }

    g.setColour(colour.withAlpha(0.10f));
    g.fillRect(area);
    g.setColour(colour.withAlpha(0.38f));
    g.drawHorizontalLine(roundToInt(area.getCentreY()), area.getX(), area.getRight());
    g.setColour(colour.withAlpha(0.90f));
    g.strokePath(trace, PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));
}

void NodeCanvas::drawEnvelopeCurve(Graphics& g, Rectangle<float> area) {
    Path envelope;
    envelope.startNewSubPath(area.getX(), area.getBottom());
    envelope.lineTo(area.getX() + area.getWidth() * 0.16f, area.getY() + area.getHeight() * 0.12f);
    envelope.lineTo(area.getX() + area.getWidth() * 0.46f, area.getY() + area.getHeight() * 0.12f);
    envelope.cubicTo(
            area.getX() + area.getWidth() * 0.58f,
            area.getY() + area.getHeight() * 0.36f,
            area.getX() + area.getWidth() * 0.70f,
            area.getY() + area.getHeight() * 0.82f,
            area.getRight(),
            area.getBottom());

    g.setColour(colourForDomain(PortDomain::EnvelopeSignal).withAlpha(0.10f));
    g.fillRect(area);
    g.setColour(colourForDomain(PortDomain::EnvelopeSignal).withAlpha(0.92f));
    g.strokePath(envelope, PathStrokeType(2.f, PathStrokeType::curved, PathStrokeType::rounded));
}

void NodeCanvas::drawExpandedEditor(Graphics& g, const Node& node) {
    const Rectangle<float> available = getLocalBounds().toFloat().reduced(28.f);
    const float width = jmin(available.getWidth(), jmax(420.f, available.getWidth() * 0.80f));
    const float height = jmin(available.getHeight(), jmax(300.f, available.getHeight() * 0.80f));
    Rectangle<float> panel = Rectangle<float>(width, height).withCentre(available.getCentre());

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

    Rectangle<float> closeButton = Rectangle<float>(24.f, 24.f).withCentre({ header.getRight() - 24.f, header.getCentreY() });
    g.setColour(Colour(0xff0e1318));
    g.fillEllipse(closeButton);
    g.setColour(Colour(0xff354050));
    g.drawEllipse(closeButton, 1.f);
    g.setColour(kText);
    g.drawLine(closeButton.getX() + 8.f, closeButton.getY() + 8.f,
               closeButton.getRight() - 8.f, closeButton.getBottom() - 8.f, 1.4f);
    g.drawLine(closeButton.getRight() - 8.f, closeButton.getY() + 8.f,
               closeButton.getX() + 8.f, closeButton.getBottom() - 8.f, 1.4f);

    auto content = panel.reduced(18.f, 16.f);
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

    Rectangle<float> legend((float) getWidth() - 748.f, (float) getHeight() - 42.f, 544.f, 24.f);

    if (legend.getX() < 110.f) {
        return;
    }

    g.setColour(Colour(0xaa0b0e13));
    g.fillRoundedRectangle(legend, 5.f);
    g.setColour(Colour(0xff354050));
    g.drawRoundedRectangle(legend, 5.f, 1.f);

    float x = legend.getX() + 16.f;
    const float y = legend.getCentreY();
    const Font legendFont(FontOptions(9.f));
    constexpr float lineWidth = 25.5f;
    constexpr float labelGap = 9.f;
    constexpr float itemGap = 26.f;

    g.setFont(legendFont);

    for (const auto& entry : entries) {
        const Colour colour = colourForDomain(entry.domain);
        const float labelWidth = legendFont.getStringWidthFloat(entry.label);
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
        g.drawText(entry.label, Rectangle<float>(labelX, legend.getY(), labelWidth + 1.f, legend.getHeight()),
                   Justification::centredLeft);
        x = labelX + labelWidth + itemGap;
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
    struct PaletteEntry {
        NodeKind kind;
        const char* label;
    };

    const PaletteEntry entries[] = {
            { NodeKind::TrilinearMesh, "Mesh" },
            { NodeKind::Fft, "FFT" },
            { NodeKind::Ifft, "IFFT" },
            { NodeKind::Envelope, "Env" },
            { NodeKind::Add, "Add" },
            { NodeKind::Multiply, "Mul" },
            { NodeKind::StereoSplit, "Split" },
            { NodeKind::StereoJoin, "Join" },
            { NodeKind::Output, "Out" }
    };

    Rectangle<float> panel(18.f, 74.f, kPaletteWidth, 10.f + (float) std::size(entries) * kPaletteRowHeight);
    g.setColour(Colour(0xaa0b0e13));
    g.fillRoundedRectangle(panel, 6.f);
    g.setColour(Colour(0xff354050));
    g.drawRoundedRectangle(panel, 6.f, 1.f);

    for (int i = 0; i < (int) std::size(entries); ++i) {
        Rectangle<float> row(panel.getX() + 9.f, panel.getY() + 8.f + (float) i * kPaletteRowHeight,
                             panel.getWidth() - 18.f, 31.f);
        Node previewNode = GraphNodeFactory().createNode(entries[i].kind, {}, {});
        Colour colour = colourForDomain(PortDomain::TimeSignal);

        if (!previewNode.outputs.empty()) {
            colour = portDisplayColour(previewNode, previewNode.outputs.front());
        } else if (!previewNode.inputs.empty()) {
            colour = portDisplayColour(previewNode, previewNode.inputs.front());
        }

        g.setColour(colour.withAlpha(0.12f));
        g.fillRoundedRectangle(row, 4.f);
        g.setColour(colour.withAlpha(0.48f));
        g.drawRoundedRectangle(row, 4.f, 1.f);
        g.setColour(kText);
        g.setFont(FontOptions(12.f, Font::bold));
        g.drawText(entries[i].label, row, Justification::centred);
    }
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
    const float size = 8.8f * portScaleForZoom(zoom);
    Point<float> worldCentre;

    switch (port.side) {
        case PortSide::Top:
        case PortSide::Bottom: {
            const int index = portIndexOnSide(node, port);
            const int count = jmax(1, portCountOnSide(node, port.side));
            const float x = node.bounds.getX() + node.bounds.getWidth() * ((float) index + 1.f) / ((float) count + 1.f);
            const float y = port.side == PortSide::Top ? node.bounds.getY() : node.bounds.getBottom();
            worldCentre = { x, y };
            break;
        }

        case PortSide::Right:
            worldCentre = { node.bounds.getRight(), sidePortY(node, port) };
            break;

        case PortSide::Left:
        default:
            worldCentre = { node.bounds.getX(), sidePortY(node, port) };
            break;
    }

    Point<float> centre = toScreen(worldCentre);
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
            if (getPortLocation(node, port).bounds.expanded(10.f).contains(screenPosition)) {
                result = { node.id, port.id, true };
                return true;
            }
        }

        for (const auto& port : node.outputs) {
            if (getPortLocation(node, port).bounds.expanded(10.f).contains(screenPosition)) {
                result = { node.id, port.id, false };
                return true;
            }
        }
    }

    return false;
}

bool NodeCanvas::findConnectablePortAt(
        Point<float> screenPosition,
        const PortAddress& source,
        PortAddress& result) const {
    PortAddress candidate;

    if (!findPortAt(screenPosition, candidate)) {
        return false;
    }

    if (!canConnectPorts(source, candidate)) {
        return false;
    }

    result = candidate;
    return true;
}

bool NodeCanvas::findPaletteKindAt(Point<float> screenPosition, NodeKind& kind) const {
    const NodeKind entries[] = {
            NodeKind::TrilinearMesh,
            NodeKind::Fft,
            NodeKind::Ifft,
            NodeKind::Envelope,
            NodeKind::Add,
            NodeKind::Multiply,
            NodeKind::StereoSplit,
            NodeKind::StereoJoin,
            NodeKind::Output
    };

    Rectangle<float> panel(18.f, 74.f, kPaletteWidth, 10.f + (float) std::size(entries) * kPaletteRowHeight);

    if (!panel.contains(screenPosition)) {
        return false;
    }

    for (int i = 0; i < (int) std::size(entries); ++i) {
        Rectangle<float> row(panel.getX() + 9.f, panel.getY() + 8.f + (float) i * kPaletteRowHeight,
                             panel.getWidth() - 18.f, 31.f);

        if (row.contains(screenPosition)) {
            kind = entries[i];
            return true;
        }
    }

    return false;
}

int NodeCanvas::findEdgeAt(Point<float> screenPosition) const {
    const auto& edges = graph.getEdges();

    for (int edgeIndex = (int) edges.size() - 1; edgeIndex >= 0; --edgeIndex) {
        const auto& edge = edges[(size_t) edgeIndex];
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

        Path hitPath;
        PathStrokeType(16.f, PathStrokeType::curved, PathStrokeType::rounded)
                .createStrokedPath(hitPath, createCablePath(
                        getPortLocation(*sourceNode, *sourcePort).centre,
                        getPortLocation(*destNode, *destPort).centre,
                        edge.attachment));

        if (hitPath.contains(screenPosition)) {
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

String NodeCanvas::hoverTextFor(Point<float> screenPosition) const {
    NodeKind paletteKind;

    if (findPaletteKindAt(screenPosition, paletteKind)) {
        Node node = GraphNodeFactory().createNode(paletteKind, {}, {});
        return "Create " + node.title + "  /  " + node.subtitle;
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
        return String(edge.attachment ? "Attachment" : "Signal")
                + " edge  /  " + labelForDomain(edge.domain)
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
    return node.title + "  /  " + node.subtitle
            + "  /  inputs " + String((int) node.inputs.size())
            + "  /  outputs " + String((int) node.outputs.size());
}

Point<float> NodeCanvas::viewportCentreWorld() const {
    return toWorld(getLocalBounds().toFloat().getCentre());
}

Point<float> NodeCanvas::paletteCreationWorldPosition(Point<float> paletteClickPosition) const {
    const float paletteRight = 18.f + kPaletteWidth;
    const float x = jmin((float) getWidth() - 280.f, paletteRight + 32.f);
    return toWorld({ x, paletteClickPosition.y });
}

void NodeCanvas::refreshCompiledState() {
    compileResult = GraphCompiler().compile(graph);
    runtimeTrace = {};

    if (compileResult.succeeded()) {
        runtimeTrace = GraphRuntime().process(graph, compileResult.plan);
    }
}

File NodeCanvas::snapshotFile() const {
    return File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("CycleV2")
            .getChildFile("graph-snapshot.xml");
}

bool NodeCanvas::saveSnapshot() {
    const File file = snapshotFile();
    file.getParentDirectory().createDirectory();

    if (!file.replaceWithText(GraphSerializer().toXmlString(graph))) {
        editStatusMessage = "Save failed";
        repaint();
        return true;
    }

    editStatusMessage = "Saved snapshot";
    repaint();
    return true;
}

bool NodeCanvas::loadSnapshot() {
    const File file = snapshotFile();

    if (!file.existsAsFile()) {
        editStatusMessage = "No snapshot";
        repaint();
        return true;
    }

    NodeGraph loaded = GraphSerializer().fromXmlString(file.loadFileAsString());

    if (loaded.getNodes().empty()) {
        editStatusMessage = "Load failed";
        repaint();
        return true;
    }

    pushUndoSnapshot();
    graph = std::move(loaded);
    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    redoStack.clear();
    refreshCompiledState();
    editStatusMessage = "Loaded snapshot";
    repaint();
    return true;
}

bool NodeCanvas::undo() {
    if (undoStack.empty()) {
        editStatusMessage = "Nothing to undo";
        repaint();
        return true;
    }

    redoStack.push_back(GraphSerializer().toXmlString(graph));
    const String xml = undoStack.back();
    undoStack.pop_back();
    return restoreGraphXml(xml, "Undo");
}

bool NodeCanvas::redo() {
    if (redoStack.empty()) {
        editStatusMessage = "Nothing to redo";
        repaint();
        return true;
    }

    undoStack.push_back(GraphSerializer().toXmlString(graph));
    const String xml = redoStack.back();
    redoStack.pop_back();
    return restoreGraphXml(xml, "Redo");
}

void NodeCanvas::pushUndoSnapshot() {
    pushUndoSnapshot(GraphSerializer().toXmlString(graph));
}

void NodeCanvas::pushUndoSnapshot(String xml) {
    if (xml.isEmpty()) {
        return;
    }

    undoStack.push_back(std::move(xml));
    redoStack.clear();

    constexpr size_t maxUndoDepth = 64;
    if (undoStack.size() > maxUndoDepth) {
        undoStack.erase(undoStack.begin());
    }
}

bool NodeCanvas::restoreGraphXml(const String& xml, const String& statusMessage) {
    NodeGraph restored = GraphSerializer().fromXmlString(xml);

    if (restored.getNodes().empty()) {
        editStatusMessage = "Restore failed";
        repaint();
        return true;
    }

    graph = std::move(restored);
    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    refreshCompiledState();
    editStatusMessage = statusMessage;
    repaint();
    return true;
}

bool NodeCanvas::clearSelection() {
    if (selectedNodeId.isEmpty() && expandedNodeId.isEmpty()) {
        return false;
    }

    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    repaint();
    return true;
}

bool NodeCanvas::canConnectPorts(const PortAddress& first, const PortAddress& second) const {
    if (first.input == second.input) {
        return false;
    }

    NodeGraph candidate = graph;
    return GraphEditor().connect(candidate, first, second).succeeded();
}

Path NodeCanvas::createCablePath(Point<float> source, Point<float> dest, bool) const {
    Path path;
    path.startNewSubPath(source);

    const float deltaX = dest.x - source.x;
    const float dx = jmax(36.f * zoom, std::abs(deltaX) * 0.45f);
    const float lift = -30.f * zoom;
    const Point<float> c1(source.x + dx, source.y + lift);
    const Point<float> c2(dest.x - dx, dest.y + lift);
    path.cubicTo(c1, c2, dest);

    return path;
}

}
