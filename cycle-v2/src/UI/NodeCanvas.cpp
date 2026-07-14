#include "NodeCanvas.h"
#include "NodeViewModule.h"

#include "../Runtime/GraphAudioExecutor.h"

#include <Util/Arithmetic.h>

#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>

namespace CycleV2 {

Rectangle<float> nodePreviewBounds(Rectangle<float> nodeBounds, NodeKind kind, float zoom);
Rectangle<float> previewContentArea(Rectangle<float> area);

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
constexpr float kPaletteWidth = 72.f;
constexpr float kPaletteX = 18.f;
constexpr float kPaletteY = 74.f;
constexpr float kPaletteRowHeight = 78.f;
constexpr float kPalettePulloutWidth = 144.f;
constexpr float kPalettePulloutRowHeight = 52.f;
constexpr float kExpandedEditorScale = 0.90f;
constexpr float kExpandedEditorMinMargin = 18.f;
constexpr float kExpandedEditorHeaderHeight = 34.f;
constexpr float kVoiceEditorLabelWidth = 78.f;
constexpr float kVoiceEditorRowHeight = 23.f;
constexpr float kVoiceEditorRowGap = 3.f;
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

struct PaletteEntry {
    NodeKind kind;
    const char* label;
};

struct PaletteSection {
    const char* title;
    const char* shortLabel;
    PortDomain domain {};
    const PaletteEntry* entries {};
    int entryCount {};
};

const PaletteEntry kContextEntries[] = {
        { NodeKind::VoiceContext, "Voice Context" }
};

const PaletteEntry kTransformEntries[] = {
        { NodeKind::Fft, "Time → Freq" },
        { NodeKind::Ifft, "Freq → Time" }
};

const PaletteEntry kMathEntries[] = {
        { NodeKind::Add, "Add" },
        { NodeKind::Multiply, "Multiply" }
};

const PaletteEntry kSourceEntries[] = {
        { NodeKind::TrilinearMesh, "Mesh" },
        { NodeKind::ImageSource, "Image" },
        { NodeKind::WaveSource, "Wave" }
};

const PaletteEntry kControlEntries[] = {
        { NodeKind::Envelope, "Envelope" },
        { NodeKind::GuideCurve, "Guide" }
};

const PaletteEntry kFxEntries[] = {
        { NodeKind::ImpulseResponse, "IR" },
        { NodeKind::Waveshaper, "Waveshaper" },
        { NodeKind::Reverb, "Reverb" },
        { NodeKind::Delay, "Delay" }
};

const PaletteEntry kChannelEntries[] = {
        { NodeKind::Spy, "Spy" },
        { NodeKind::StereoSplit, "Split" },
        { NodeKind::StereoJoin, "Join" },
        { NodeKind::Output, "Output" }
};

const PaletteSection kPaletteSections[] = {
        { "Context",   "Context",   PortDomain::VoiceControlSignal, kContextEntries,   (int) std::size(kContextEntries) },
        { "Transform", "Transform", PortDomain::SpectralMagnitudeSignal, kTransformEntries, (int) std::size(kTransformEntries) },
        { "Math",      "Math",      PortDomain::ControlSignal, kMathEntries,      (int) std::size(kMathEntries) },
        { "Source",    "Source",    PortDomain::TimeSignal, kSourceEntries,    (int) std::size(kSourceEntries) },
        { "Control",   "Control",   PortDomain::EnvelopeSignal, kControlEntries,   (int) std::size(kControlEntries) },
        { "FX",        "FX",        PortDomain::SpectralPhaseSignal, kFxEntries,        (int) std::size(kFxEntries) },
        { "Channel",   "Channel",   PortDomain::TimeSignal, kChannelEntries,   (int) std::size(kChannelEntries) }
};

float cableScaleForZoom(float zoom) {
    return zoom / kCableReferenceZoom * kCableStrokeScale;
}

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

var pointToVar(Point<float> point) {
    auto* object = new DynamicObject();
    object->setProperty("x", point.x);
    object->setProperty("y", point.y);
    return object;
}

var pointerTargetToVar(
        const String& id,
        const String& kind,
        Rectangle<float> bounds,
        const String& nodeId = {},
        const String& portId = {},
        bool input = false,
        const String& parameterId = {},
        const String& axis = {}) {
    auto* object = new DynamicObject();
    object->setProperty("id", id);
    object->setProperty("kind", kind);
    object->setProperty("bounds", rectangleToVar(bounds));
    object->setProperty("centre", pointToVar(bounds.getCentre()));

    if (nodeId.isNotEmpty()) {
        object->setProperty("nodeId", nodeId);
    }
    if (portId.isNotEmpty()) {
        object->setProperty("portId", portId);
        object->setProperty("input", input);
    }
    if (parameterId.isNotEmpty()) {
        object->setProperty("parameterId", parameterId);
    }
    if (axis.isNotEmpty()) {
        object->setProperty("axis", axis);
    }

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

var audioMetricsToVar(const SignalPayload& payload, double sampleRate) {
    auto* object = new DynamicObject();
    const auto& samples = payload.block.samples;
    double sumSquares = 0.0;
    float peak = 0.f;

    for (float sample : samples) {
        const float magnitude = sample >= 0.f ? sample : -sample;
        peak = jmax(peak, magnitude);
        sumSquares += (double) sample * (double) sample;
    }

    const double sampleCount = (double) jmax(1, (int) samples.size());
    object->setProperty("sampleRate", sampleRate);
    object->setProperty("channels", 1);
    object->setProperty("samples", (int) samples.size());
    object->setProperty("durationMs", sampleRate > 0.0 ? 1000.0 * (double) samples.size() / sampleRate : 0.0);
    object->setProperty("peak", peak);
    object->setProperty("rms", std::sqrt(sumSquares / sampleCount));
    object->setProperty("domain", labelForDomain(payload.domain));
    object->setProperty("channelLayout", labelForChannelLayout(payload.channelLayout));
    return object;
}

String trimeshHitRegionKind(TrimeshExpandedHitRegionKind kind) {
    switch (kind) {
        case TrimeshExpandedHitRegionKind::MorphControl:
            return "trimeshMorphRail";
        case TrimeshExpandedHitRegionKind::PrimaryAxis:
            return "trimeshPrimaryAxis";
        case TrimeshExpandedHitRegionKind::LinkToggle:
            return "trimeshLinkToggle";
        case TrimeshExpandedHitRegionKind::VertexParameter:
            return "trimeshVertexParameter";
        case TrimeshExpandedHitRegionKind::VertexGuideAttachment:
            return "trimeshVertexGuideAttachment";
    }

    return "trimeshControl";
}

var portToVar(const Port& port) {
    auto* object = new DynamicObject();
    object->setProperty("id", port.id);
    object->setProperty("label", port.label);
    object->setProperty("domain", labelForDomain(port.domain));
    object->setProperty("channelLayout", labelForChannelLayout(port.channelLayout));
    object->setProperty("purpose", port.purpose == PortPurpose::ScratchAttachment ? "scratchAttachment" : "signal");
    object->setProperty("input", port.input);
    return object;
}

var parameterToVar(const NodeParameter& parameter) {
    auto* object = new DynamicObject();
    object->setProperty("id", parameter.id);
    object->setProperty("label", parameter.label);
    object->setProperty("value", parameter.value);
    return object;
}

var edgeToVar(const Edge& edge) {
    auto* object = new DynamicObject();
    object->setProperty("sourceNodeId", edge.sourceNodeId);
    object->setProperty("sourcePortId", edge.sourcePortId);
    object->setProperty("destNodeId", edge.destNodeId);
    object->setProperty("destPortId", edge.destPortId);
    object->setProperty("domain", labelForDomain(edge.domain));
    object->setProperty("attachment", edge.attachment);
    return object;
}

String labelForPreviewRole(PreviewModuleRole role) {
    switch (role) {
        case PreviewModuleRole::Waveform:           return "Waveform";
        case PreviewModuleRole::Image:              return "Image";
        case PreviewModuleRole::MeshSurface:        return "Mesh Surface";
        case PreviewModuleRole::SpectrumMagnitude:  return "Spectrum Magnitude";
        case PreviewModuleRole::SpectrumPhase:      return "Spectrum Phase";
        case PreviewModuleRole::Envelope:           return "Envelope";
        case PreviewModuleRole::ImpulseResponse:    return "Impulse Response";
        case PreviewModuleRole::Waveshaper:         return "Waveshaper";
        case PreviewModuleRole::OutputMeters:       return "Output Meters";
        case PreviewModuleRole::VoiceContext:       return "Voice Context";
        case PreviewModuleRole::SignalSpy:          return "Signal Spy";
        case PreviewModuleRole::Generic:            return "Generic";
        case PreviewModuleRole::None:
        default:
            return "None";
    }
}

var previewStatsToVar(const NodePreviewResult& preview) {
    auto* object = new DynamicObject();
    const auto& values = preview.primary;

    float minimum = 0.f;
    float maximum = 0.f;
    double total = 0.0;
    double absoluteTotal = 0.0;

    if (!values.empty()) {
        minimum = values.front();
        maximum = values.front();
    }

    for (float value : values) {
        minimum = jmin(minimum, value);
        maximum = jmax(maximum, value);
        total += (double) value;
        absoluteTotal += (double) (value >= 0.f ? value : -value);
    }

    const double count = (double) jmax(1, (int) values.size());
    object->setProperty("nodeId", preview.nodeId);
    object->setProperty("role", labelForPreviewRole(preview.role));
    object->setProperty("domain", labelForDomain(preview.domain));
    object->setProperty("gridColumns", (int) preview.gridColumns);
    object->setProperty("gridRows", (int) preview.gridRows);
    object->setProperty("sampleCount", (int) values.size());
    object->setProperty("minimum", minimum);
    object->setProperty("maximum", maximum);
    object->setProperty("mean", total / count);
    object->setProperty("absoluteSum", absoluteTotal);
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

Rectangle<float> paletteRailBounds() {
    return {
            kPaletteX,
            kPaletteY,
            kPaletteWidth,
            12.f + (float) std::size(kPaletteSections) * kPaletteRowHeight
    };
}

Rectangle<float> paletteGroupBounds(int sectionIndex) {
    return {
            kPaletteX + 7.f,
            kPaletteY + 7.f + (float) sectionIndex * kPaletteRowHeight,
            kPaletteWidth - 14.f,
            kPaletteRowHeight - 9.f
    };
}

Rectangle<float> palettePulloutBounds(int sectionIndex) {
    const auto group = paletteGroupBounds(sectionIndex);
    const auto& section = kPaletteSections[(size_t) sectionIndex];
    const float height = (float) section.entryCount * kPalettePulloutRowHeight;
    return {
            paletteRailBounds().getRight() + 10.f,
            group.getY(),
            kPalettePulloutWidth,
            height
    };
}

Rectangle<float> palettePulloutEntryBounds(int sectionIndex, int entryIndex) {
    const auto panel = palettePulloutBounds(sectionIndex);
    return {
            panel.getX(),
            panel.getY() + (float) entryIndex * kPalettePulloutRowHeight,
            panel.getWidth(),
            kPalettePulloutRowHeight - 7.f
    };
}

Rectangle<float> paletteHoverBounds(int sectionIndex) {
    const auto group = paletteGroupBounds(sectionIndex);
    const auto pullout = palettePulloutBounds(sectionIndex);
    const float top = jmin(group.getY(), pullout.getY()) - 22.f;
    const float bottom = jmax(group.getBottom(), pullout.getBottom()) + 22.f;

    return {
            group.getX() - 6.f,
            top,
            pullout.getRight() - group.getX() + 12.f,
            bottom - top
    };
}

int paletteGroupIndexAt(Point<float> screenPosition) {
    for (int i = 0; i < (int) std::size(kPaletteSections); ++i) {
        if (paletteGroupBounds(i).expanded(3.f).contains(screenPosition)) {
            return i;
        }
    }

    return -1;
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

Rectangle<float> visibleCableBounds(const Path& cable, float zoom) {
    return cable.getBounds().expanded(jmax(8.f, 14.f * cableScaleForZoom(zoom)));
}

Point<float> outwardNormalForSide(PortSide side) {
    switch (side) {
        case PortSide::Top:       return { 0.f, -1.f };
        case PortSide::Bottom:    return { 0.f, 1.f };
        case PortSide::Left:      return { -1.f, 0.f };
        case PortSide::Right:     return { 1.f, 0.f };
        default:                  return { 1.f, 0.f };
    }
}

Point<float> normalizedOrFallback(Point<float> vector, Point<float> fallback) {
    const float length = vector.getDistanceFromOrigin();

    if (length <= 0.001f) {
        return fallback;
    }

    return { vector.x / length, vector.y / length };
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

Rectangle<float> voiceDomainButtonBounds(const Rectangle<float>& nodeBounds, float zoom) {
    const Rectangle<float> body = nodeBounds.withTrimmedTop(42.f * zoom);
    const float width = jmin(nodeBounds.getWidth() - 96.f * zoom, 64.f * zoom);
    return Rectangle<float>(width, 28.f * zoom)
            .withCentre({ nodeBounds.getCentreX(), body.getY() + 28.f * zoom });
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

Rectangle<float> voiceContextEditorColumnBounds(Rectangle<float> panel) {
    panel.removeFromTop(30.f);
    return panel.reduced(26.f, 18.f);
}

Rectangle<float> transformEditorColumnBounds(Rectangle<float> panel) {
    panel.removeFromTop(30.f);
    return panel.reduced(24.f, 18.f);
}

String voiceDomainForNode(const Node& node) {
    return parameterValueForNode(node, "domain", "waveform");
}

String voiceDomainButtonLabel(const Node& node) {
    const String domain = voiceDomainForNode(node);
    return domain == "spectral" ? "Spectral" : "Waveform";
}

String nextVoiceDomain(const Node& node) {
    return voiceDomainForNode(node) == "spectral" ? "waveform" : "spectral";
}

bool expandedEditorBlocksCanvas(const Node* node) {
    return node != nullptr
            && NodeViewModuleRegistry::instance().moduleFor(node->kind)
                    .capabilities().expandedEditorBlocksCanvas;
}

bool hasHostedExpandedEditor(const Node& node) {
    return NodeViewModuleRegistry::instance().moduleFor(node.kind).capabilities().hostedEditor;
}

void drawVoiceContextOctaveSlider(Graphics& g, Rectangle<float> area, const Node& node, float zoom) {
    const Colour colour = colourForDomain(PortDomain::PitchSignal);
    const float centreY = area.getCentreY();
    const float left = area.getX() + 2.f * zoom;
    const float right = area.getRight() - 2.f * zoom;
    const float tickHeight = jmax(5.f * zoom, area.getHeight() * 0.20f);
    const float tickStroke = jmax(1.f, 1.15f * zoom);
    const float thumbSize = jmax(11.f * zoom, area.getHeight() * 0.44f);
    const int octave = jlimit(-2, 2, parameterValueForNode(node, "octave", "0").getIntValue());
    const float thumbX = jmap((float) (octave + 2), 0.f, 4.f, left, right);

    g.setColour(kMutedText.withAlpha(0.28f));
    g.drawLine(Line<float>({ left, centreY }, { right, centreY }), jmax(1.f, 1.4f * zoom));
    g.setColour(colour.withAlpha(0.70f));
    g.drawLine(Line<float>({ left, centreY }, { thumbX, centreY }), jmax(1.f, 2.f * zoom));

    for (int i = -2; i <= 2; ++i) {
        const float x = jmap((float) (i + 2), 0.f, 4.f, left, right);
        const bool active = i == octave;

        g.setColour(active ? colour.withAlpha(0.92f) : kMutedText.withAlpha(0.55f));
        g.drawLine(Line<float>({ x, centreY - tickHeight * 0.5f }, { x, centreY + tickHeight * 0.5f }), tickStroke);
    }

    g.setColour(Colour(0xff071015).withAlpha(0.72f));
    g.fillEllipse(Rectangle<float>(thumbSize + 4.f * zoom, thumbSize + 4.f * zoom).withCentre({ thumbX, centreY }));
    g.setColour(colour.withAlpha(0.98f));
    g.fillEllipse(Rectangle<float>(thumbSize, thumbSize).withCentre({ thumbX, centreY }));
    g.setColour(Colours::white.withAlpha(0.22f));
    g.drawEllipse(Rectangle<float>(thumbSize, thumbSize).withCentre({ thumbX, centreY }), jmax(1.f, 1.f * zoom));
}

void drawVoiceContextSourceSelector(Graphics& g, Rectangle<float> area, const Node& node) {
    Rectangle<float> labelArea = area.removeFromLeft(kVoiceEditorLabelWidth);
    Rectangle<float> control = area.reduced(0.f, 2.f);
    const bool spectral = voiceDomainForNode(node) == "spectral";
    const Colour waveformColour = colourForDomain(PortDomain::TimeSignal);
    const Colour spectralColour = colourForDomain(PortDomain::SpectralMagnitudeSignal);
    const Colour activeColour = spectral ? spectralColour : waveformColour;

    g.setFont(FontOptions(10.8f, Font::bold));
    g.setColour(kMutedText.withAlpha(0.76f));
    g.drawText("Source", labelArea, Justification::centredLeft);

    Rectangle<float> waveformLabel = control.removeFromLeft(62.f);
    control.removeFromLeft(8.f);
    Rectangle<float> switchArea = control.removeFromLeft(42.f).reduced(1.f, 2.f);
    control.removeFromLeft(8.f);
    Rectangle<float> spectralLabel = control.removeFromLeft(56.f);
    const float knobSize = switchArea.getHeight() - 4.f;
    const Point<float> knobCentre(
            spectral ? switchArea.getRight() - switchArea.getHeight() * 0.5f : switchArea.getX() + switchArea.getHeight() * 0.5f,
            switchArea.getCentreY());

    g.setColour(spectral ? kMutedText.withAlpha(0.62f) : kText.withAlpha(0.92f));
    g.drawText("Waveform", waveformLabel, Justification::centredRight);
    g.setColour(spectral ? kText.withAlpha(0.92f) : kMutedText.withAlpha(0.62f));
    g.drawText("Spectral", spectralLabel, Justification::centredLeft);
    g.setColour(Colour(0xff0e1318));
    g.fillRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f);
    g.setColour(activeColour.withAlpha(0.62f));
    g.drawRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f, 1.1f);
    g.fillEllipse(Rectangle<float>(knobSize, knobSize).withCentre(knobCentre));
}

void drawVoiceContextSlider(
        Graphics& g,
        Rectangle<float> area,
        const String& label,
        float normalized,
        Colour colour) {
    const float trackY = area.getCentreY();
    const float labelWidth = kVoiceEditorLabelWidth;
    Rectangle<float> labelArea = area.removeFromLeft(labelWidth);
    Rectangle<float> valueArea = area.reduced(2.f, 0.f);
    const float left = valueArea.getX();
    const float right = valueArea.getRight();
    const float knobX = jmap(jlimit(0.f, 1.f, normalized), 0.f, 1.f, left, right);
    const float knobSize = jmax(8.f, area.getHeight() * 0.35f);

    g.setFont(FontOptions(11.f, Font::bold));
    g.setColour(kMutedText.withAlpha(0.76f));
    g.drawText(label, labelArea, Justification::centredLeft);
    g.setColour(kMutedText.withAlpha(0.30f));
    g.drawLine(Line<float>({ left, trackY }, { right, trackY }), 1.4f);
    g.setColour(colour.withAlpha(0.76f));
    g.drawLine(Line<float>({ left, trackY }, { knobX, trackY }), 2.2f);
    g.fillEllipse(Rectangle<float>(knobSize, knobSize).withCentre({ knobX, trackY }));
}

void drawVoiceContextCheckbox(Graphics& g, Rectangle<float> area, const String& label, bool checked) {
    Rectangle<float> labelArea = area.removeFromLeft(kVoiceEditorLabelWidth);
    const float box = 15.f;
    const Rectangle<float> checkBox(box, box);
    const Rectangle<float> placed = checkBox.withCentre({ area.getX() + box * 0.5f, area.getCentreY() });
    const Colour colour = colourForDomain(PortDomain::PitchSignal);

    g.setColour(checked ? colour.withAlpha(0.18f) : Colour(0xff0e1318));
    g.fillRoundedRectangle(placed, 3.f);
    g.setColour(checked ? colour.withAlpha(0.86f) : kMutedText.withAlpha(0.70f));
    g.drawRoundedRectangle(placed, 3.f, 1.3f);

    if (checked) {
        Path tick;
        tick.startNewSubPath(placed.getX() + 4.f, placed.getCentreY());
        tick.lineTo(placed.getCentreX() - 1.f, placed.getBottom() - 4.f);
        tick.lineTo(placed.getRight() - 3.5f, placed.getY() + 4.f);
        g.strokePath(tick, PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    g.setFont(FontOptions(11.f, Font::bold));
    g.setColour(kMutedText.withAlpha(0.76f));
    g.drawText(label, labelArea, Justification::centredLeft);
}

void drawVoiceContextStopSlider(
        Graphics& g,
        Rectangle<float> area,
        const String& label,
        const std::vector<String>& values,
        const String& value,
        Colour colour) {
    Rectangle<float> labelArea = area.removeFromLeft(kVoiceEditorLabelWidth);
    Rectangle<float> control = area.reduced(2.f, 0.f);
    const float trackY = control.getCentreY() - 2.f;
    const float left = control.getX();
    const float right = control.getRight();
    int activeIndex {};

    for (int i = 0; i < (int) values.size(); ++i) {
        if (values[(size_t) i] == value) {
            activeIndex = i;
            break;
        }
    }

    const float activeX = values.size() <= 1
            ? left
            : jmap((float) activeIndex, 0.f, (float) values.size() - 1.f, left, right);

    g.setFont(FontOptions(11.f, Font::bold));
    g.setColour(kMutedText.withAlpha(0.76f));
    g.drawText(label, labelArea, Justification::centredLeft);
    g.setColour(kMutedText.withAlpha(0.28f));
    g.drawLine(Line<float>({ left, trackY }, { right, trackY }), 1.4f);
    g.setColour(colour.withAlpha(0.70f));
    g.drawLine(Line<float>({ left, trackY }, { activeX, trackY }), 2.f);

    for (int i = 0; i < (int) values.size(); ++i) {
        const float x = values.size() <= 1
                ? left
                : jmap((float) i, 0.f, (float) values.size() - 1.f, left, right);
        const bool active = i == activeIndex;

        g.setColour(active ? colour.withAlpha(0.92f) : kMutedText.withAlpha(0.52f));
        g.fillEllipse(Rectangle<float>(active ? 9.f : 5.f, active ? 9.f : 5.f).withCentre({ x, trackY }));
        g.setFont(FontOptions(8.5f, Font::bold));
        g.setColour(active ? kText.withAlpha(0.90f) : kMutedText.withAlpha(0.66f));
        g.drawText(values[(size_t) i], Rectangle<float>(x - 14.f, trackY + 5.f, 28.f, 12.f), Justification::centred);
    }
}

void drawVoiceContextEditor(Graphics& g, Rectangle<float> content, const Node& node) {
    const Colour colour = colourForDomain(PortDomain::PitchSignal);
    Rectangle<float> column = content;
    const float pitch = parameterValueForNode(node, "pitch", "0").getFloatValue();
    const bool portamento = parameterValueForNode(node, "portamento", "0") == "1"
            || parameterValueForNode(node, "portamento", "false") == "true";

    drawVoiceContextSourceSelector(g, column.removeFromTop(kVoiceEditorRowHeight), node);
    column.removeFromTop(kVoiceEditorRowGap);

    Rectangle<float> octaveRow = column.removeFromTop(kVoiceEditorRowHeight);
    g.setFont(FontOptions(11.f, Font::bold));
    g.setColour(kMutedText.withAlpha(0.76f));
    g.drawText("Octave", octaveRow.removeFromLeft(kVoiceEditorLabelWidth), Justification::centredLeft);
    drawVoiceContextOctaveSlider(g, octaveRow.reduced(2.f, 0.f), node, 1.f);
    column.removeFromTop(kVoiceEditorRowGap);

    drawVoiceContextSlider(g, column.removeFromTop(kVoiceEditorRowHeight), "Pitch", (pitch + 12.f) / 24.f, colour);
    column.removeFromTop(kVoiceEditorRowGap);
    drawVoiceContextCheckbox(g, column.removeFromTop(kVoiceEditorRowHeight), "Portamento", portamento);
    column.removeFromTop(kVoiceEditorRowGap);
    drawVoiceContextStopSlider(
            g,
            column.removeFromTop(kVoiceEditorRowHeight),
            "Oversampling",
            { "1x", "2x", "4x" },
            parameterValueForNode(node, "oversampling", "1x"),
            colour);
}

String transformModeForNode(const Node& node) {
    if (node.kind == NodeKind::Fft) {
        return parameterValueForNode(node, "mode", "cycle");
    }

    if (node.kind == NodeKind::Ifft) {
        return parameterValueForNode(node, "mode", "cyclic");
    }

    return {};
}

String transformSubtitleForMode(const Node& node, const String& mode) {
    if (node.kind == NodeKind::Fft) {
        return mode == "fixedWindow" ? "fixed window" : "cycle chunks";
    }

    if (node.kind == NodeKind::Ifft) {
        return mode == "acyclicCarry" ? "carry overlap" : "cyclic overlap";
    }

    return {};
}

String transformModeStatus(const Node& node, const String& mode) {
    if (node.kind == NodeKind::Fft) {
        return mode == "fixedWindow" ? "Time to freq: fixed time-window" : "Time to freq: chunked by cycle";
    }

    if (node.kind == NodeKind::Ifft) {
        return mode == "acyclicCarry" ? "Freq to time: acyclic carry-buffer overlap" : "Freq to time: cyclic overlap";
    }

    return {};
}

void drawTransformModeChoice(
        Graphics& g,
        Rectangle<float> area,
        const String& label,
        const String& leftText,
        const String& rightText,
        bool rightActive,
        Colour colour) {
    Rectangle<float> labelArea = area.removeFromLeft(kVoiceEditorLabelWidth);
    Rectangle<float> control = area.reduced(2.f, 0.f);
    const float corner = 6.f;
    const Rectangle<float> left = control.removeFromLeft((control.getWidth() - 5.f) * 0.5f);
    control.removeFromLeft(5.f);
    const Rectangle<float> right = control;
    const Rectangle<float> active = rightActive ? right : left;

    g.setFont(FontOptions(11.f, Font::bold));
    g.setColour(kMutedText.withAlpha(0.76f));
    g.drawText(label, labelArea, Justification::centredLeft);
    g.setColour(Colour(0xff0e1318));
    g.fillRoundedRectangle(left, corner);
    g.fillRoundedRectangle(right, corner);
    g.setColour(kMutedText.withAlpha(0.32f));
    g.drawRoundedRectangle(left, corner, 1.f);
    g.drawRoundedRectangle(right, corner, 1.f);
    g.setColour(colour.withAlpha(0.18f));
    g.fillRoundedRectangle(active, corner);
    g.setColour(colour.withAlpha(0.82f));
    g.drawRoundedRectangle(active, corner, 1.2f);

    g.setFont(FontOptions(10.6f, Font::bold));
    g.setColour(rightActive ? kMutedText.withAlpha(0.62f) : kText.withAlpha(0.92f));
    g.drawText(leftText, left.reduced(5.f, 0.f), Justification::centred);
    g.setColour(rightActive ? kText.withAlpha(0.92f) : kMutedText.withAlpha(0.62f));
    g.drawText(rightText, right.reduced(5.f, 0.f), Justification::centred);
}

void drawTransformEditor(Graphics& g, Rectangle<float> content, const Node& node) {
    Rectangle<float> column = content;
    const String mode = transformModeForNode(node);
    const Colour colour = colourForDomain(node.kind == NodeKind::Fft
            ? PortDomain::SpectralMagnitudeSignal
            : PortDomain::TimeSignal);

    if (node.kind == NodeKind::Fft) {
        drawTransformModeChoice(
                g,
                column.removeFromTop(26.f),
                "Window",
                "Cycle chunks",
                "Fixed time",
                mode == "fixedWindow",
                colour);
        return;
    }

    if (node.kind == NodeKind::Ifft) {
        drawTransformModeChoice(
                g,
                column.removeFromTop(26.f),
                "Overlap",
                "Cyclic",
                "Acyclic carry",
                mode == "acyclicCarry",
                colour);
    }
}

Colour previewColourForRole(PreviewModuleRole role, const Node& node) {
    switch (role) {
        case PreviewModuleRole::SpectrumMagnitude: return colourForDomain(PortDomain::SpectralMagnitudeSignal);
        case PreviewModuleRole::SpectrumPhase:     return colourForDomain(PortDomain::SpectralPhaseSignal);
        case PreviewModuleRole::Envelope:          return colourForDomain(PortDomain::EnvelopeSignal);
        case PreviewModuleRole::OutputMeters:
        case PreviewModuleRole::Waveform:
        case PreviewModuleRole::Waveshaper:        return colourForDomain(PortDomain::TimeSignal);
        case PreviewModuleRole::SignalSpy:         return Colour(0xffd2d9e2);
        case PreviewModuleRole::MeshSurface:       return colourForDomain(PortDomain::MeshField);
        default:                                   break;
    }

    return node.outputs.empty() ? Colour(0xff9aa5b2) : colourForDomain(node.outputs.front().domain);
}

String previewCacheSignature(const Node& node, PortDomain domain) {
    String signature = String((int) node.kind) + ":" + String((int) domain);

    for (const auto& parameter : node.parameters) {
        signature += "|" + parameter.id + "=" + parameter.value;
    }

    for (const auto& output : node.outputs) {
        signature += "|out:" + output.id + ":" + String((int) output.domain);
    }

    return signature;
}

void drawPreviewTrace(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<float>& values,
        Colour colour,
        float zoom,
        bool fillBackground = true) {
    if (values.empty()) {
        return;
    }

    Path trace;

    for (size_t i = 0; i < values.size(); ++i) {
        const float t = values.size() > 1 ? (float) i / (float) (values.size() - 1) : 0.f;
        const float y = jlimit(0.f, 1.f, values[i]);
        const Point<float> point(
                area.getX() + t * area.getWidth(),
                area.getBottom() - y * area.getHeight());

        if (i == 0) {
            trace.startNewSubPath(point);
        } else {
            trace.lineTo(point);
        }
    }

    if (fillBackground) {
        g.setColour(colour.withAlpha(0.12f));
        g.fillRect(area);
    }

    g.setColour(colour.withAlpha(0.92f));
    g.strokePath(trace, PathStrokeType(2.f * zoom, PathStrokeType::curved, PathStrokeType::rounded));
}

void drawPreviewMeters(
        Graphics& g,
        Rectangle<float> area,
        const NodePreviewResult& preview,
        Colour colour) {
    const float left = preview.primary.empty() ? 0.f : jlimit(0.f, 1.f, preview.primary.front());
    const float right = preview.secondary.empty() ? left : jlimit(0.f, 1.f, preview.secondary.front());
    Rectangle<float> meterArea = area.reduced(area.getWidth() * 0.20f, area.getHeight() * 0.08f);
    const float width = meterArea.getWidth() * 0.28f;
    auto leftMeter = meterArea.removeFromLeft(width);
    auto rightMeter = meterArea.removeFromRight(width);

    auto drawMeter = [&](Rectangle<float> bounds, float level) {
        constexpr int segments = 12;
        const float gap = jmax(1.f, bounds.getHeight() * 0.015f);
        const float segmentHeight = (bounds.getHeight() - gap * (float) (segments - 1)) / (float) segments;
        const int litSegments = jlimit(0, segments, roundToInt(level * (float) segments));

        for (int i = 0; i < segments; ++i) {
            const int levelIndex = segments - 1 - i;
            const Rectangle<float> segment(
                    bounds.getX(),
                    bounds.getY() + (float) i * (segmentHeight + gap),
                    bounds.getWidth(),
                    segmentHeight);
            const bool lit = levelIndex < litSegments;
            const float normalized = (float) levelIndex / (float) (segments - 1);
            Colour segmentColour = colour;

            if (normalized > 0.78f) {
                segmentColour = Colour(0xffff705f);
            } else if (normalized > 0.58f) {
                segmentColour = Colour(0xfff4d35e);
            }

            g.setColour(segmentColour.withAlpha(lit ? 0.82f : 0.14f));
            g.fillRoundedRectangle(segment, 1.4f);
        }
    };

    drawMeter(leftMeter, left);
    drawMeter(rightMeter, right);
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

void remapSpectralRows(std::vector<float>& surface, size_t columns, size_t rows) {
    if (columns == 0 || rows < 2 || surface.size() < columns * rows) {
        return;
    }

    std::vector<float> source = surface;
    std::vector<float> rowMap(rows);
    Buffer<float> mappedRows(rowMap.data(), (int) rowMap.size());
    mappedRows.ramp(0.f, 1.f / (float) (rowMap.size() - 1));
    Arithmetic::applyLogMapping(mappedRows, 500.f);

    for (size_t column = 0; column < columns; ++column) {
        const size_t columnOffset = column * rows;

        for (size_t row = 0; row < rows; ++row) {
            const float sourcePosition = jlimit(0.f, (float) (rows - 1), rowMap[row] * (float) (rows - 1));
            const size_t rowA = (size_t) sourcePosition;
            const size_t rowB = std::min(rowA + 1, rows - 1);
            const float amount = sourcePosition - (float) rowA;
            const float valueA = source[columnOffset + rowA];
            const float valueB = source[columnOffset + rowB];
            surface[columnOffset + row] = valueA + amount * (valueB - valueA);
        }
    }
}

void unwrapPhaseRowsAcrossColumns(std::vector<float>& surface, size_t columns, size_t rows) {
    if (columns < 2 || rows == 0 || surface.size() < columns * rows) {
        return;
    }

    const float wrap = MathConstants<float>::twoPi;

    for (size_t row = 0; row < rows; ++row) {
        float offset = 0.f;
        float previous = surface[row];

        for (size_t column = 1; column < columns; ++column) {
            const size_t index = column * rows + row;
            const float current = surface[index];
            const float delta = current + offset - previous;

            if (delta > MathConstants<float>::pi) {
                offset -= wrap;
            } else if (delta < -MathConstants<float>::pi) {
                offset += wrap;
            }

            surface[index] = current + offset;
            previous = surface[index];
        }
    }
}

void mapMagnitudeForPreview(std::vector<float>& surface) {
    if (surface.empty()) {
        return;
    }

    Buffer<float> buffer(surface.data(), (int) surface.size());
    constexpr float kMagnitudeDisplayGain = 16.f;
    constexpr float kInverseLogDisplayGain = 1.f / 2.833213344f; // 1 / ln(1 + 16)

    buffer.abs()
            .mul(kMagnitudeDisplayGain)
            .add(1.f)
            .ln()
            .mul(kInverseLogDisplayGain)
            .clip(0.f, 1.f);
}

void mapUnwrappedPhaseForPreview(std::vector<float>& surface) {
    if (surface.empty()) {
        return;
    }

    float minimum = std::numeric_limits<float>::max();
    float maximum = std::numeric_limits<float>::lowest();

    for (float value : surface) {
        minimum = jmin(minimum, value);
        maximum = jmax(maximum, value);
    }

    const float realMaximum = jmax(std::abs(minimum), std::abs(maximum));
    if (realMaximum <= 0.f) {
        Buffer<float>(surface.data(), (int) surface.size()).set(0.5f);
        return;
    }

    const float scaleExponent = std::ceil(std::log2(realMaximum) + 0.5f);
    const float phaseScale = std::pow(2.f, -scaleExponent);

    Buffer<float>(surface.data(), (int) surface.size()).mul(phaseScale).add(0.5f).clip(0.f, 1.f);
}

std::vector<float> rawMappedPreviewSurface(const NodePreviewResult& preview) {
    std::vector<float> surface = preview.primary;

    if (surface.empty()) {
        return surface;
    }

    if (preview.domain == PortDomain::SpectralMagnitudeSignal) {
        remapSpectralRows(surface, preview.gridColumns, preview.gridRows);
        mapMagnitudeForPreview(surface);

        return surface;
    }

    if (preview.domain == PortDomain::SpectralPhaseSignal) {
        unwrapPhaseRowsAcrossColumns(surface, preview.gridColumns, preview.gridRows);
        remapSpectralRows(surface, preview.gridColumns, preview.gridRows);
        mapUnwrappedPhaseForPreview(surface);

        return surface;
    }

    if (preview.domain == PortDomain::TimeSignal) {
        Buffer<float> buffer(surface.data(), (int) surface.size());
        buffer.mul(0.5f).add(0.5f).clip(0.f, 1.f);

        return surface;
    }

    Buffer<float>(surface.data(), (int) surface.size()).clip(0.f, 1.f);

    return surface;
}

bool drawPreviewHeatmapSurface(
        Graphics& g,
        Rectangle<float> area,
        const NodePreviewResult& preview) {
    TrimeshRenderData renderData;
    renderData.surface = rawMappedPreviewSurface(preview);
    renderData.domain = preview.domain;
    renderData.columns = (int) preview.gridColumns;
    renderData.rows = (int) preview.gridRows;
    renderData.cyclic = preview.domain == PortDomain::TimeSignal;

    if (!renderData.canDrawSurface()) {
        return false;
    }

    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(preview.domain);
    const Image image = TrimeshSurfaceRenderer::createHeatmapImage(renderData, profile);
    if (!image.isValid()) {
        return false;
    }

    g.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    const Rectangle<float> content = area.reduced(jmin(area.getWidth(), area.getHeight()) * 0.024f);
    g.setColour(Colour(0xff07090d));
    g.fillRect(content);
    g.drawImage(image, content);
    return true;
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

void drawFftSquareCycle(Graphics& g, Rectangle<float> area, float strokeScale) {
    Path path;
    const float left = area.getX() + area.getWidth() * 0.10f;
    const float mid = area.getCentreX();
    const float right = area.getRight() - area.getWidth() * 0.10f;
    const float top = area.getY() + area.getHeight() * 0.20f;
    const float bottom = area.getY() + area.getHeight() * 0.80f;

    path.startNewSubPath(left, bottom);
    path.lineTo(left, top);
    path.lineTo(mid, top);
    path.lineTo(mid, bottom);
    path.lineTo(right, bottom);
    path.lineTo(right, top);

    g.setColour(Colour(0xff58d4e8));
    g.strokePath(path, PathStrokeType(2.55f * strokeScale, PathStrokeType::mitered, PathStrokeType::rounded));
}

void drawFftHarmonicStack(Graphics& g, Rectangle<float> area, float strokeScale) {
    const struct {
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

        for (int i = 0; i < halfCycles; ++i) {
            const float x0 = waveArea.getX() + (float) i * halfWidth;
            const float x1 = x0 + halfWidth;
            const float controlY = waveArea.getCentreY() + (i % 2 == 0 ? -amplitude : amplitude);

            path.cubicTo(
                    x0 + halfWidth * sineControl, controlY,
                    x1 - halfWidth * sineControl, controlY,
                    x1, waveArea.getCentreY());
        }

        g.setColour(partial.colour.withAlpha(0.90f));
        g.strokePath(path, PathStrokeType(1.45f * strokeScale, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

void drawFftChevron(Graphics& g, Rectangle<float> icon, float strokeScale) {
    Path path;
    const Point<float> top(icon.getX() + icon.getWidth() * 0.476f, icon.getY() + icon.getHeight() * 0.39f);
    const Point<float> middle(icon.getX() + icon.getWidth() * 0.512f, icon.getY() + icon.getHeight() * 0.50f);
    const Point<float> bottom(icon.getX() + icon.getWidth() * 0.476f, icon.getY() + icon.getHeight() * 0.61f);

    path.startNewSubPath(top);
    path.lineTo(middle);
    path.lineTo(bottom);

    g.setColour(Colour(0xff596a78));
    g.strokePath(path, PathStrokeType(2.f * strokeScale, PathStrokeType::mitered, PathStrokeType::rounded));
}

void drawFftTransformPreview(Graphics& g, Rectangle<float> area, bool inverse) {
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
        drawFftHarmonicStack(g, left, strokeScale);
        drawFftChevron(g, icon, strokeScale);
        drawFftSquareCycle(g, right, strokeScale);
    } else {
        drawFftSquareCycle(g, left, strokeScale);
        drawFftChevron(g, icon, strokeScale);
        drawFftHarmonicStack(g, right, strokeScale);
    }
}

void drawMathOperationPreview(Graphics& g, Rectangle<float> area, bool multiply, float zoom) {
    const Colour colour = colourForDomain(PortDomain::ControlSignal);
    const Rectangle<float> icon = fitAspect(area.reduced(area.getWidth() * 0.20f, area.getHeight() * 0.12f), 1.f);
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

    g.setColour(Colour(0xff071015).withAlpha(0.46f));
    g.strokePath(mark, PathStrokeType(stroke + 2.f * zoom, PathStrokeType::mitered, PathStrokeType::rounded));
    g.setColour(colour.withAlpha(0.94f));
    g.strokePath(mark, PathStrokeType(stroke, PathStrokeType::mitered, PathStrokeType::rounded));
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
    ,   previewResult(presentation.previewResult()) {
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
    repaint();
}

void NodeCanvas::mouseMove(const MouseEvent& event) {
    lastMousePosition = event.position;
    updatePaletteHover(event.position);
    setMouseCursor(MouseCursor::NormalCursor);
    repaint();
}

void NodeCanvas::mouseDown(const MouseEvent& event) {
    grabKeyboardFocus();
    updatePaletteHover(event.position);
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

        if (expandedNode != nullptr && expandedNode->kind == NodeKind::TrilinearMesh) {
            repaint();
            return;
        }

        if (closeButton.contains(event.position)) {
            expandedNodeId = {};
            repaint();
            return;
        }

        if (expandedNode != nullptr
                && expandedNode->kind == NodeKind::VoiceContext
                && panel.contains(event.position)
                && handleVoiceContextEditorClick(event.position)) {
            return;
        }

        if (expandedNode != nullptr
                && (expandedNode->kind == NodeKind::Fft || expandedNode->kind == NodeKind::Ifft)
                && panel.contains(event.position)
                && handleTransformEditorClick(event.position)) {
            return;
        }

        expandedEditorDragCaptured = panel.contains(event.position);

        repaint();
        return;
    }

    NodeKind paletteKind;
    if (findPaletteKindAt(event.position, paletteKind)) {
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
            activePaletteSectionIndex = -1;
            repaint();
        }

        return;
    }

    String layoutNodeId;
    if (findOperationLayoutButtonAt(event.position, layoutNodeId)) {
        if (cycleOperationPortLayout(layoutNodeId)) {
            editStatusMessage = "Port layout cycled";
            repaint();
        }

        return;
    }

    String outputSideNodeId;
    if (findMeshOutputSideButtonAt(event.position, outputSideNodeId)) {
        if (cycleMeshOutputSide(outputSideNodeId)) {
            editStatusMessage = "Output side cycled";
            repaint();
        }

        return;
    }

    String voiceNodeId;
    if (findVoiceDomainButtonAt(event.position, voiceNodeId)) {
        if (cycleVoiceDomain(voiceNodeId)) {
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

        repaint();
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

            repaint();
            return;
        }

        selectedNodeId = {};
        draggingNode = false;
        repaint();
        return;
    }

    selectedNodeId = {};
    selectedEdgeIndex = -1;
    draggingNode = false;
    expandedNodeId = {};

    repaint();
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

    repaint();
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
            endTrimeshMorphEdit();
            endTrimeshVertexParameterEdit();
            expandedEditorDragCaptured = false;
            repaint();
            return;
        }

        draggingNode = false;
        commands.commitCompoundEdit();
        nodeDragUndoPushed = false;
        nodeDragMoved = false;
        spliceTargetEdgeIndex = -1;
        activeSnapHasX = false;
        activeSnapHasY = false;
        endTrimeshMorphEdit();
        endTrimeshVertexParameterEdit();
        expandedEditorDragCaptured = false;
        repaint();
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
    endTrimeshMorphEdit();
    endTrimeshVertexParameterEdit();
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

    repaint();
}

void NodeCanvas::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) {
    const Node* expandedNode = findNode(expandedNodeId);
    if (expandedNode != nullptr && expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode).contains(event.position)) {
        repaint();
        return;
    }

    constexpr float panScale = 720.f;
    viewport.panBy(Point<float>(wheel.deltaX * panScale, wheel.deltaY * panScale));
    repaint();
}

void NodeCanvas::mouseMagnify(const MouseEvent& event, float scaleFactor) {
    viewport.zoomAround(event.position, scaleFactor);
    repaint();
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
            repaint();
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
        repaint();
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
        drawGlTrimeshExpandedPanel();
        drawGlEffect2DExpandedPanel();
    } else {
        OpenGLHelpers::clear(kCanvasBackground);
    }
}

void NodeCanvas::openGLContextClosing() {
    for (auto& entry : trimeshWidgets) {
        entry.second->releaseSharedGlResources();
    }

    for (auto& entry : effect2DWidgets) {
        entry.second->releaseSharedGlResources();
    }

    renderer.shutdown();
}

void NodeCanvas::timerCallback() {
    if (compiledStateRefreshPending
            && (int32) (Time::getMillisecondCounter() - compiledStateRefreshDueMs) >= 0) {
        flushScheduledCompiledStateRefresh();
    }

    updateExpandedEditorHost(findNode(expandedNodeId));

    const auto mouse = getMouseXYRelative().toFloat();
    const int previousPaletteSectionIndex = activePaletteSectionIndex;

    if (getLocalBounds().toFloat().contains(mouse)) {
        updatePaletteHover(mouse);
    }

    if (getLocalBounds().toFloat().contains(mouse)
            && (mouse != lastMousePosition || previousPaletteSectionIndex != activePaletteSectionIndex)) {
        lastMousePosition = mouse;
        repaint();
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

    for (int edgeIndex = 0; edgeIndex < (int) edges.size(); ++edgeIndex) {
        const auto& edge = edges[(size_t) edgeIndex];
        CableEndpoint sourceEndpoint;
        CableEndpoint destEndpoint;

        if (!resolveCableEndpoints(edge, sourceEndpoint, destEndpoint)) {
            continue;
        }

        const Path cable = createCablePath(
                sourceEndpoint.centre,
                destEndpoint.centre,
                sourceEndpoint.side,
                destEndpoint.side,
                edge.attachment);
        const Rectangle<float> cableBounds = visibleCableBounds(cable, zoom);

        if (!cableBounds.intersects(visibleArea)) {
            continue;
        }

        Colour colour = colourForDomain(displayDomainForEdge(edge));
        const bool invalid = edgeHasValidationIssue(edge);

        if (invalid) {
            colour = Colour(0xffff5a5f);
        }

        renderer.renderCable(
                cable,
                sourceEndpoint.centre,
                destEndpoint.centre,
                colour,
                cableScaleForZoom(zoom),
                edgeIndex == selectedEdgeIndex || edgeIndex == spliceTargetEdgeIndex,
                edge.attachment,
                invalid,
                destEndpoint.portLike);
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

bool NodeCanvas::drawGlEffect2DExpandedPanel() {
    const Node* expandedNode = findNode(expandedNodeId);

    if (expandedNode == nullptr) {
        return false;
    }

    if ((expandedNode->kind == NodeKind::Envelope
            || expandedNode->kind == NodeKind::GuideCurve
            || expandedNode->kind == NodeKind::ImpulseResponse
            || expandedNode->kind == NodeKind::Waveshaper)
            && effect2DExpandedEditor != nullptr
            && effect2DExpandedEditor->isVisible()) {
        effect2DExpandedEditor->renderOpenGL((float) openGLContext.getRenderingScale());
        return true;
    }

    return false;
}

void NodeCanvas::drawGlTrimeshExpandedPanel() {
    const Node* expandedNode = findNode(expandedNodeId);

    if (expandedNode == nullptr) {
        return;
    }

    if (expandedNode->kind == NodeKind::TrilinearMesh
            && trimeshExpandedEditor != nullptr
            && trimeshExpandedEditor->isVisible()) {
        trimeshExpandedEditor->renderOpenGL((float) openGLContext.getRenderingScale());
    }
}

void NodeCanvas::drawGlEffect2DPreviews() {
    const float zoom = viewport.getZoom();
    const auto visibleArea = getLocalBounds().toFloat().expanded(120.f);
    const Node* expandedNode = findNode(expandedNodeId);
    const bool hasExpandedEditor = expandedEditorBlocksCanvas(expandedNode);
    const Rectangle<float> expandedPanel = expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode);
    const float scaleFactor = (float) openGLContext.getRenderingScale();

    for (const auto& node : graph.getNodes()) {
        if (!isEffect2DNode(node.kind)) {
            continue;
        }

        const Rectangle<float> nodeBounds = toScreen(node.bounds);

        if (!nodeBounds.intersects(visibleArea)) {
            continue;
        }

        if (hasExpandedEditor && expandedPanel.intersects(nodeBounds)) {
            continue;
        }

        const Rectangle<float> preview = nodePreviewBounds(nodeBounds, node.kind, zoom);
        effect2DWidgetFor(node).renderPreviewSnapshotOpenGL(node, preview, scaleFactor);
    }
}

void NodeCanvas::drawEdges(Graphics& g) {
    const float zoom = viewport.getZoom();
    if (kUseGlCanvasEdges) {
        return;
    }

    const auto& edges = graph.getEdges();
    const auto visibleArea = getLocalBounds().toFloat().expanded(160.f);
    const Node* expandedNode = findNode(expandedNodeId);
    const bool hasExpandedEditor = expandedEditorBlocksCanvas(expandedNode);
    const Rectangle<float> expandedPanel = expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode);

    for (int edgeIndex = 0; edgeIndex < (int) edges.size(); ++edgeIndex) {
        const auto& edge = edges[(size_t) edgeIndex];
        CableEndpoint sourceEndpoint;
        CableEndpoint destEndpoint;

        if (!resolveCableEndpoints(edge, sourceEndpoint, destEndpoint)) {
            continue;
        }

        auto source = sourceEndpoint.centre;
        auto dest = destEndpoint.centre;
        Path cable = createCablePath(source, dest, sourceEndpoint.side, destEndpoint.side, edge.attachment);
        const Rectangle<float> cableBounds = visibleCableBounds(cable, zoom);

        if (!cableBounds.intersects(visibleArea)) {
            continue;
        }

        if (hasExpandedEditor && expandedPanel.intersects(cableBounds)) {
            continue;
        }

        Colour colour = colourForDomain(displayDomainForEdge(edge));
        const bool invalid = edgeHasValidationIssue(edge);

        const bool selected = edgeIndex == selectedEdgeIndex;
        const bool spliceTarget = edgeIndex == spliceTargetEdgeIndex;
        const float cableScale = cableScaleForZoom(zoom);

        if (invalid) {
            colour = Colour(0xffff5a5f);
        }

        if (edge.attachment) {
            Path dashedCable;
            PathStrokeType stroke(2.0f * cableScale, PathStrokeType::curved, PathStrokeType::rounded);
            Array<float> dashes { 8.f * cableScale, 7.f * cableScale };
            stroke.createDashedStroke(dashedCable, cable, dashes.getRawDataPointer(), dashes.size());
            g.setColour(colour.withAlpha(spliceTarget ? 0.62f : (selected ? 0.46f : 0.32f)));
            g.strokePath(
                    dashedCable,
                    PathStrokeType((spliceTarget ? 13.f : (selected ? 10.f : 7.f)) * cableScale,
                                   PathStrokeType::curved,
                                   PathStrokeType::rounded));
            g.setColour(colour.withAlpha(0.92f));
            g.strokePath(
                    dashedCable,
                    PathStrokeType((spliceTarget ? 4.5f : (selected ? 3.f : 2.f)) * cableScale,
                                   PathStrokeType::curved,
                                   PathStrokeType::rounded));
        } else {
            g.setColour(colour.withAlpha(spliceTarget ? 0.42f : (selected ? 0.28f : 0.18f)));
            g.strokePath(
                    cable,
                    PathStrokeType((spliceTarget ? 15.f : (selected ? 12.f : 9.f)) * cableScale,
                                   PathStrokeType::curved,
                                   PathStrokeType::rounded));
            g.setColour(colour.withAlpha(0.92f));
            g.strokePath(
                    cable,
                    PathStrokeType((spliceTarget ? 5.2f : (selected ? 4.f : 3.f)) * cableScale,
                                   PathStrokeType::curved,
                                   PathStrokeType::rounded));

            if (invalid) {
                Path dashedCable;
                PathStrokeType stroke(1.8f * cableScale, PathStrokeType::curved, PathStrokeType::rounded);
                Array<float> dashes { 5.f * cableScale, 6.f * cableScale };
                stroke.createDashedStroke(dashedCable, cable, dashes.getRawDataPointer(), dashes.size());
                g.setColour(Colours::white.withAlpha(0.58f));
                g.strokePath(dashedCable, PathStrokeType(1.4f * cableScale, PathStrokeType::curved, PathStrokeType::rounded));
            }
        }

        if (spliceTarget) {
            const Point<float> midpoint = cable.getPointAlongPath(cable.getLength() * 0.5f);
            const float markerSize = 17.f * cableScale;
            Rectangle<float> insertMarker(
                    midpoint.x - markerSize * 0.5f,
                    midpoint.y - markerSize * 0.5f,
                    markerSize,
                    markerSize);

            g.setColour(kCanvasBackground.withAlpha(0.92f));
            g.fillEllipse(insertMarker);
            g.setColour(colour.withAlpha(0.98f));
            g.drawEllipse(insertMarker, 2.2f * cableScale);
            g.drawLine(
                    insertMarker.getX() + markerSize * 0.30f,
                    midpoint.y,
                    insertMarker.getRight() - markerSize * 0.30f,
                    midpoint.y,
                    1.9f * cableScale);
            g.drawLine(
                    midpoint.x,
                    insertMarker.getY() + markerSize * 0.30f,
                    midpoint.x,
                    insertMarker.getBottom() - markerSize * 0.30f,
                    1.9f * cableScale);
        }

        const float endpointSize = (spliceTarget ? 15.f : (selected ? 14.f : 11.f)) * cableScale;
        Rectangle<float> sourceMarker(source.x - endpointSize * 0.5f, source.y - endpointSize * 0.5f,
                                      endpointSize, endpointSize);
        Rectangle<float> destMarker(dest.x - endpointSize * 0.5f, dest.y - endpointSize * 0.5f,
                                    endpointSize, endpointSize);

        g.setColour(kCanvasBackground.withAlpha(0.92f));
        g.fillEllipse(sourceMarker);
        g.setColour(colour.withAlpha(0.96f));
        g.drawEllipse(sourceMarker, (spliceTarget ? 2.8f : (selected ? 2.4f : 1.8f)) * cableScale);

        if (destEndpoint.portLike) {
            g.fillEllipse(destMarker.reduced((spliceTarget ? 1.2f : (selected ? 1.5f : 2.f)) * cableScale));
        } else {
            Path badge;
            const float radius = (selected ? 7.f : 5.8f) * cableScale;
            badge.startNewSubPath(dest.x, dest.y - radius);
            badge.lineTo(dest.x + radius, dest.y);
            badge.lineTo(dest.x, dest.y + radius);
            badge.lineTo(dest.x - radius, dest.y);
            badge.closeSubPath();

            g.setColour(kCanvasBackground.withAlpha(0.92f));
            g.fillPath(badge);
            g.setColour(colour.withAlpha(selected ? 0.98f : 0.78f));
            g.strokePath(badge, PathStrokeType((selected ? 2.2f : 1.6f) * cableScale));
        }
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
    const Path cable = createCablePath(source, dest, sourceSide, destSide, false);

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
    auto preview = nodePreviewBounds(nodeBounds, node.kind, zoom);

    if (node.kind != NodeKind::VoiceContext) {
        drawPreview(g, node, preview);
    }

    if (node.kind == NodeKind::VoiceContext) {
        auto pill = voiceDomainButtonBounds(nodeBounds, zoom);
        const bool spectral = voiceDomainForNode(node) == "spectral";
        const float labelWidth = 82.f * zoom;
        const Rectangle<float> waveformLabel(pill.getX() - labelWidth - 9.f * zoom, pill.getY(), labelWidth, pill.getHeight());
        const Rectangle<float> spectralLabel(pill.getRight() + 9.f * zoom, pill.getY(), labelWidth, pill.getHeight());
        const Colour waveformColour = colourForDomain(PortDomain::TimeSignal);
        const Colour spectralColour = colourForDomain(PortDomain::SpectralMagnitudeSignal);
        const Colour activeColour = spectral ? spectralColour : waveformColour;
        const float knobSize = pill.getHeight() - 6.f * zoom;
        const Rectangle<float> knob(knobSize, knobSize);
        const Point<float> knobCentre(
                spectral ? pill.getRight() - pill.getHeight() * 0.5f : pill.getX() + pill.getHeight() * 0.5f,
                pill.getCentreY());

        g.setFont(FontOptions(15.1f * zoom, Font::bold));
        g.setColour(spectral ? kMutedText.withAlpha(0.70f) : kText);
        g.drawText("Waveform", waveformLabel, Justification::centredRight);
        g.setColour(spectral ? kText : kMutedText.withAlpha(0.70f));
        g.drawText("Spectral", spectralLabel, Justification::centredLeft);
        g.setColour(Colour(0xff091015).withAlpha(0.96f));
        g.fillRoundedRectangle(pill, pill.getHeight() * 0.5f);
        g.setColour(activeColour.withAlpha(0.22f));
        g.fillRoundedRectangle(pill.reduced(2.f * zoom), pill.getHeight() * 0.5f);
        g.setColour(activeColour.withAlpha(0.82f));
        g.drawRoundedRectangle(pill, pill.getHeight() * 0.5f, 1.2f * uiScale);
        g.setColour(activeColour);
        g.fillEllipse(knob.withCentre(knobCentre));
        g.setColour(Colours::white.withAlpha(0.30f));
        g.drawEllipse(knob.withCentre(knobCentre), 1.f * uiScale);
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
    const float zoom = viewport.getZoom();
    const int width = roundToInt(area.getWidth());
    const int height = roundToInt(area.getHeight());
    const PortDomain previewDomain = displayDomainForNodeOutput(node, "out");
    const TrimeshRenderProfile previewProfile = renderProfileForNodeOutput(node, "out");
    const String signature = previewCacheSignature(node, previewDomain);

    if (width <= 0 || height <= 0) {
        return;
    }

    if (node.kind == NodeKind::TrilinearMesh) {
        trimeshWidgetFor(node.id).paintCompact(g, node, area, zoom, previewProfile);
        return;
    }

    if (node.kind == NodeKind::Envelope
            || node.kind == NodeKind::GuideCurve
            || node.kind == NodeKind::ImpulseResponse
            || node.kind == NodeKind::Waveshaper) {
        if (effect2DWidgetFor(node).paintPreviewSnapshot(g, area)) {
            return;
        }

        drawPreviewUncached(g, node, area, previewDomain);
        return;
    }

    if (const NodePreviewResult* preview = findPreviewResult(node.id)) {
        if (preview->role == PreviewModuleRole::SignalSpy) {
            drawCachedPreviewHeatmapSurface(g, node.id, area, *preview);
            return;
        }
    }

    CachedPreviewSprite& cached = cachedPreviewSpriteFor(node.id);

    if (!cached.image.isValid()
            || cached.width != width
            || cached.height != height
            || cached.domain != previewDomain
            || cached.signature != signature) {
        cached.image = Image(Image::ARGB, width, height, true);
        cached.width = width;
        cached.height = height;
        cached.domain = previewDomain;
        cached.signature = signature;

        Graphics spriteGraphics(cached.image);
        drawPreviewUncached(spriteGraphics, node, { 0.f, 0.f, (float) width, (float) height }, previewDomain);
    }

    g.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    g.drawImage(cached.image, area);
}

bool NodeCanvas::drawCachedPreviewHeatmapSurface(
        Graphics& g,
        const String& cacheKey,
        Rectangle<float> area,
        const NodePreviewResult& preview) {
    const int width = roundToInt(area.getWidth());
    const int height = roundToInt(area.getHeight());

    if (width <= 0 || height <= 0) {
        return false;
    }

    CachedPreviewSprite& cached = cachedPreviewSpriteFor(cacheKey);
    const String signature = String((int) preview.role)
            + ":" + String((int) preview.domain)
            + ":" + String((int) preview.gridColumns)
            + "x" + String((int) preview.gridRows)
            + ":" + String((int) preview.primary.size())
            + ":" + String::toHexString((int64) previewContentHash(preview));

    if (!cached.image.isValid()
            || cached.width != width
            || cached.height != height
            || cached.domain != preview.domain
            || cached.signature != signature) {
        cached.image = Image(Image::ARGB, width, height, true);
        cached.width = width;
        cached.height = height;
        cached.domain = preview.domain;
        cached.signature = signature;

        Graphics spriteGraphics(cached.image);
        if (!drawPreviewHeatmapSurface(spriteGraphics, { 0.f, 0.f, (float) width, (float) height }, preview)) {
            cached.image = {};
            return false;
        }
    }

    g.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    g.drawImage(cached.image, area);
    return true;
}

Rectangle<float> previewContentArea(Rectangle<float> area) {
    return area.reduced(jmin(area.getWidth(), area.getHeight()) * 0.12f);
}

Rectangle<float> nodePreviewBounds(Rectangle<float> nodeBounds, NodeKind kind, float zoom) {
    Rectangle<float> preview = nodeBounds.withTrimmedTop(42.f * zoom).reduced(8.f * zoom);

    if (kind == NodeKind::Fft || kind == NodeKind::Ifft) {
        preview = nodeBounds.withTrimmedTop(40.f * zoom).reduced(3.f * zoom, 5.f * zoom);
    } else if (kind == NodeKind::Waveshaper) {
        const float size = jmin(preview.getWidth(), preview.getHeight());
        preview = Rectangle<float>(size, size).withCentre(preview.getCentre());
    }

    return preview;
}

void drawEffect2DPreview(
        Graphics& g,
        Rectangle<float> area,
        NodeKind kind,
        const std::vector<Effect2DPanelBridge::PreviewVertex>& vertices,
        float zoom) {
    const Colour line = Colour(0xffe2e8ef);
    const Colour dim = Colour(0xff8b95a3);
    Rectangle<float> graph = area.reduced(8.f, 7.f);

    if (kind == NodeKind::Waveshaper) {
        const float size = jmin(graph.getWidth(), graph.getHeight());
        graph = Rectangle<float>(size, size).withCentre(graph.getCentre());
    }

    g.setColour(Colour(0xff0d1117).withAlpha(0.34f));
    g.fillRect(graph);

    float xMaximum = 1.f;

    if (kind == NodeKind::Envelope) {
        for (const auto& vertex : vertices) {
            xMaximum = jmax(xMaximum, vertex.x);
        }
    }

    auto mapPoint = [xMaximum](Rectangle<float> bounds, float x, float y) {
        const float normalizedX = xMaximum > 0.f ? x / xMaximum : x;
        return Point<float>(
                bounds.getX() + bounds.getWidth() * normalizedX,
                bounds.getBottom() - bounds.getHeight() * y);
    };

    auto drawVertexPath = [&](Rectangle<float> inner) {
        if (vertices.empty()) {
            return;
        }

        g.saveState();
        g.reduceClipRegion(inner.toNearestInt());

        Path curve;
        bool started {};

        for (const auto& vertex : vertices) {
            const Point<float> point = mapPoint(inner, vertex.x, vertex.y);

            if (!started) {
                curve.startNewSubPath(point);
                started = true;
            } else {
                curve.lineTo(point);
            }
        }

        g.setColour(line.withAlpha(0.88f));
        g.strokePath(curve, PathStrokeType(jmax(1.4f, 2.f * zoom), PathStrokeType::curved, PathStrokeType::rounded));
        g.restoreState();
    };

    if (kind == NodeKind::Waveshaper) {
        const Rectangle<float> inner = graph.reduced(graph.getWidth() * 0.125f, graph.getHeight() * 0.125f);
        g.setColour(dim.withAlpha(0.44f));
        g.drawRect(inner, jmax(1.f, zoom));
        drawVertexPath(graph);
        return;
    }

    if (kind == NodeKind::Envelope) {
        g.setColour(dim.withAlpha(0.44f));
        g.drawRect(graph, jmax(1.f, zoom));
        drawVertexPath(graph);
        return;
    }

    if (kind == NodeKind::GuideCurve) {
        const Rectangle<float> inner = graph.withTrimmedLeft(graph.getWidth() * 0.05f)
                .withTrimmedRight(graph.getWidth() * 0.05f);
        g.setColour(dim.withAlpha(0.44f));
        g.drawRect(inner, jmax(1.f, zoom));
        drawVertexPath(graph);
        return;
    }

    if (kind == NodeKind::ImpulseResponse) {
        const Rectangle<float> inner = graph.withTrimmedLeft(graph.getWidth() * 0.0625f);
        g.setColour(dim.withAlpha(0.44f));
        g.drawRect(inner, jmax(1.f, zoom));
        drawVertexPath(graph);
    }
}

void NodeCanvas::drawPreviewUncached(
        Graphics& g,
        const Node& node,
        Rectangle<float> area,
        PortDomain previewDomain) {
    const float zoom = viewport.getZoom();
    if (area.getWidth() < 20.f || area.getHeight() < 20.f) {
        return;
    }

    if (node.kind == NodeKind::TrilinearMesh) {
        trimeshWidgetFor(node.id).paintCompact(g, node, area, zoom, previewDomain);
        return;
    }

    if (node.kind == NodeKind::Envelope
            || node.kind == NodeKind::GuideCurve
            || node.kind == NodeKind::ImpulseResponse
            || node.kind == NodeKind::Waveshaper) {
        drawEffect2DPreview(g, area, node.kind, effect2DWidgetFor(node).previewVertices(), zoom);
        return;
    }

    if (const NodePreviewResult* preview = findPreviewResult(node.id)) {
        const Colour colour = previewColourForRole(preview->role, node);

        if (preview->role == PreviewModuleRole::OutputMeters) {
            drawPreviewMeters(g, area, *preview, colour);
            return;
        }

        if (preview->role == PreviewModuleRole::SignalSpy) {
            drawPreviewHeatmapSurface(g, area, *preview);
            return;
        }

        if (!preview->primary.empty()) {
            const Rectangle<float> content = previewContentArea(area);
            drawPreviewTrace(g, content, preview->primary, colour, zoom);

            if (!preview->secondary.empty()) {
                drawPreviewTrace(g, content, preview->secondary, colour.withAlpha(0.58f), zoom);
            }

            return;
        }
    }

    if (node.kind == NodeKind::Fft || node.kind == NodeKind::Ifft) {
        drawFftTransformPreview(g, area, node.kind == NodeKind::Ifft);
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
        const NodePreviewResult preview {
                node.id,
                PreviewModuleRole::OutputMeters,
                { 0.64f },
                { 0.58f }
        };
        drawPreviewMeters(g, area, preview, colour);
        return;
    }

    if (node.kind == NodeKind::Spy) {
        return;
    }

    if (node.kind == NodeKind::Add || node.kind == NodeKind::Multiply) {
        drawMathOperationPreview(g, area, node.kind == NodeKind::Multiply, zoom);
        return;
    }

    if (node.kind == NodeKind::ImageSource) {
        auto imageArea = area.reduced(area.getWidth() * 0.12f, area.getHeight() * 0.16f);
        g.setColour(colourForDomain(PortDomain::ControlSignal).withAlpha(0.12f));
        g.fillRect(imageArea);
        g.setColour(colourForDomain(PortDomain::TimeSignal).withAlpha(0.42f));
        g.fillRect(imageArea.removeFromLeft(imageArea.getWidth() * 0.46f).reduced(1.f));
        g.setColour(colourForDomain(PortDomain::SpectralMagnitudeSignal).withAlpha(0.36f));
        g.fillRect(imageArea.removeFromTop(imageArea.getHeight() * 0.48f).reduced(1.f));
        g.setColour(colourForDomain(PortDomain::SpectralPhaseSignal).withAlpha(0.34f));
        g.fillRect(imageArea.reduced(1.f));
        return;
    }

    if (node.kind == NodeKind::WaveSource) {
        Path wave;
        for (int i = 0; i < 42; ++i) {
            const float t = (float) i / 41.f;
            const Point<float> p(
                    area.getX() + t * area.getWidth(),
                    area.getCentreY() + fastSin(t * MathConstants<float>::twoPi * 1.25f) * area.getHeight() * 0.34f);

            if (i == 0) {
                wave.startNewSubPath(p);
            } else {
                wave.lineTo(p);
            }
        }

        g.setColour(colourForDomain(PortDomain::TimeSignal).withAlpha(0.12f));
        g.fillRect(area);
        g.setColour(colourForDomain(PortDomain::TimeSignal).withAlpha(0.92f));
        g.strokePath(wave, PathStrokeType(2.f * zoom, PathStrokeType::curved, PathStrokeType::rounded));
        return;
    }

    Path curve;
    const int steps = 42;
    for (int i = 0; i < steps; ++i) {
        float t = (float) i / (float) (steps - 1);
        float y = 0.5f + 0.28f * fastSin(t * MathConstants<float>::twoPi * 1.35f + node.id.hashCode() * 0.001f);
        Point<float> p(area.getX() + t * area.getWidth(), area.getY() + y * area.getHeight());
        if (i == 0) {
            curve.startNewSubPath(p);
        } else {
            curve.lineTo(p);
        }
    }

    Colour colour = node.outputs.empty() ? Colour(0xff9aa5b2) : colourForDomain(node.outputs.front().domain);

    g.setColour(colour.withAlpha(0.20f));
    g.fillPath(curve);
    g.setColour(colour.withAlpha(0.95f));
    g.strokePath(curve, PathStrokeType(2.f * zoom, PathStrokeType::curved, PathStrokeType::rounded));
}

TrimeshWidget& NodeCanvas::trimeshWidgetFor(const String& nodeId) {
    for (auto& entry : trimeshWidgets) {
        if (entry.first == nodeId) {
            return *entry.second;
        }
    }

    trimeshWidgets.emplace_back(nodeId, std::make_unique<TrimeshWidget>());
    auto safeThis = Component::SafePointer<NodeCanvas>(this);
    trimeshWidgets.back().second->setMeshEditedCallback([safeThis, nodeId] {
        if (safeThis != nullptr) {
            safeThis->persistTrimeshMeshEdits(nodeId);
        }
    });
    return *trimeshWidgets.back().second;
}

Effect2DWidget& NodeCanvas::effect2DWidgetFor(const Node& node) {
    for (auto& entry : effect2DWidgets) {
        if (entry.first == node.id) {
            return *entry.second;
        }
    }

    effect2DWidgets.emplace_back(node.id, std::make_unique<Effect2DWidget>(node.kind));
    return *effect2DWidgets.back().second;
}

NodeCanvas::CachedPreviewSprite& NodeCanvas::cachedPreviewSpriteFor(const String& nodeId) {
    for (auto& entry : previewSpriteCache) {
        if (entry.first == nodeId) {
            return entry.second;
        }
    }

    previewSpriteCache.emplace_back(nodeId, CachedPreviewSprite {});
    return previewSpriteCache.back().second;
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
    const float zoom = viewport.getZoom();
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
        drawVoiceContextEditor(g, voiceContextEditorColumnBounds(outerPanel), node);
        return;
    }

    if (isTransformEditor) {
        drawTransformEditor(g, transformEditorColumnBounds(outerPanel), node);
        return;
    }

    if (node.kind == NodeKind::Spy) {
        const Rectangle<float> preview = content.reduced(2.f);

        if (const NodePreviewResult* result = findPreviewResult(node.id)) {
            if (drawPreviewHeatmapSurface(g, preview, *result)) {
                return;
            }
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
    const bool shouldShowTrimesh = node != nullptr && node->kind == NodeKind::TrilinearMesh;
    const bool shouldShowEffect2D = node != nullptr
            && (node->kind == NodeKind::Envelope
                || node->kind == NodeKind::GuideCurve
                || node->kind == NodeKind::ImpulseResponse
                || node->kind == NodeKind::Waveshaper);
    const String hostedNodeId = shouldShowTrimesh || shouldShowEffect2D ? node->id : String();
    hideExpandedEditorHostsExcept(hostedNodeId);

    if (!shouldShowTrimesh && trimeshExpandedEditor != nullptr) {
        trimeshExpandedEditor->setVisible(false);
        trimeshExpandedEditorNodeId = {};
    }

    if (!shouldShowEffect2D && effect2DExpandedEditor != nullptr) {
        effect2DExpandedEditor->setVisible(false);
        effect2DExpandedEditorNodeId = {};
    }

    if (shouldShowEffect2D) {
        if (trimeshExpandedEditor != nullptr) {
            trimeshExpandedEditor->setVisible(false);
        }
        trimeshExpandedEditorNodeId = {};

        Effect2DWidget& widget = effect2DWidgetFor(*node);

        if (effect2DExpandedEditor != nullptr && effect2DExpandedEditorNodeId != node->id) {
            effect2DExpandedEditor->setVisible(false);
            removeChildComponent(effect2DExpandedEditor.get());
            effect2DExpandedEditor.reset();
        }

        if (effect2DExpandedEditor == nullptr) {
            effect2DExpandedEditor = std::make_unique<Effect2DExpandedEditorComponent>(widget);
            effect2DExpandedEditorNodeId = node->id;
            auto safeThis = Component::SafePointer<NodeCanvas>(this);
            Effect2DExpandedEditorComponent::Callbacks callbacks;
            callbacks.close = [safeThis] {
                if (safeThis != nullptr) {
                    safeThis->expandedNodeId = {};
                    safeThis->updateExpandedEditorHost(nullptr);
                    safeThis->repaint();
                }
            };
            callbacks.repaintOpenGL = [safeThis] {
                if (safeThis != nullptr) {
                    safeThis->openGLContext.triggerRepaint();
                }
            };
            callbacks.setNodeParameter = [safeThis](const String& parameterId, const String& label, const String& value) {
                if (safeThis == nullptr || safeThis->expandedNodeId.isEmpty()) {
                    return;
                }

                const Node* node = safeThis->findNode(safeThis->expandedNodeId);
                if (node == nullptr || parameterValueForNode(*node, parameterId) == value) {
                    return;
                }

                auto result = safeThis->commands.setNodeParameter(
                        node->id,
                        parameterId,
                        label,
                        value);

                if (!result.succeeded()) {
                    return;
                }

                safeThis->scheduleCompiledStateRefresh();
                safeThis->openGLContext.triggerRepaint();
                safeThis->repaint();
            };
            effect2DExpandedEditor->setCallbacks(std::move(callbacks));
            addAndMakeVisible(effect2DExpandedEditor.get());
        }

        const Rectangle<int> editorBounds = expandedEditorBoundsForNode(getLocalBounds().toFloat(), node).toNearestInt();

        if (effect2DExpandedEditor->getBounds() != editorBounds) {
            effect2DExpandedEditor->setBounds(editorBounds);
        }

        effect2DExpandedEditor->setNode(*node);
        effect2DExpandedEditor->setVisible(true);
        effect2DExpandedEditor->toFront(false);
        openGLContext.triggerRepaint();
        return;
    }

    if (!shouldShowTrimesh) {
        return;
    }

    TrimeshWidget& widget = trimeshWidgetFor(node->id);
    auto safeThis = Component::SafePointer<NodeCanvas>(this);

    if (trimeshExpandedEditor != nullptr && trimeshExpandedEditorNodeId != node->id) {
        trimeshExpandedEditor->setVisible(false);
        removeChildComponent(trimeshExpandedEditor.get());
        trimeshExpandedEditor.reset();
    }

    if (trimeshExpandedEditor == nullptr) {
        trimeshExpandedEditor = std::make_unique<TrimeshExpandedEditorComponent>(widget);
        trimeshExpandedEditorNodeId = node->id;
        TrimeshExpandedEditorComponent::Callbacks callbacks;
        callbacks.close = [safeThis] {
            if (safeThis != nullptr) {
                safeThis->expandedNodeId = {};
                safeThis->updateExpandedEditorHost(nullptr);
                safeThis->repaint();
            }
        };
        callbacks.repaintOpenGL = [safeThis] {
            if (safeThis != nullptr) {
                safeThis->openGLContext.triggerRepaint();
            }
        };
        callbacks.setPrimaryAxis = [safeThis](const String& axisValue) {
            if (safeThis != nullptr) {
                safeThis->setTrimeshPrimaryAxisValue(axisValue);
            }
        };
        callbacks.toggleLinkAxis = [safeThis](const String& axisValue) {
            if (safeThis != nullptr) {
                safeThis->toggleTrimeshLinkAxisValue(axisValue);
            }
        };
        callbacks.beginMorphEdit = [safeThis](const String& parameterId, float value) {
            if (safeThis != nullptr) {
                safeThis->beginTrimeshMorphEdit(parameterId, value);
            }
        };
        callbacks.updateMorphEdit = [safeThis](float value) {
            if (safeThis != nullptr) {
                safeThis->updateTrimeshMorphEditValue(value);
            }
        };
        callbacks.endMorphEdit = [safeThis] {
            if (safeThis != nullptr) {
                safeThis->endTrimeshMorphEdit();
            }
        };
        callbacks.beginVertexParameterEdit = [safeThis](const String& parameterId, float value) {
            if (safeThis != nullptr) {
                safeThis->beginTrimeshVertexParameterEdit(parameterId, value);
            }
        };
        callbacks.updateVertexParameterEdit = [safeThis](float value) {
            if (safeThis != nullptr) {
                safeThis->updateTrimeshVertexParameterEditValue(value);
            }
        };
        callbacks.endVertexParameterEdit = [safeThis] {
            if (safeThis != nullptr) {
                safeThis->endTrimeshVertexParameterEdit();
            }
        };
        callbacks.showVertexGuideAttachmentMenu = [safeThis](
                const String& parameterField,
                Rectangle<int> targetScreenArea) {
            if (safeThis != nullptr) {
                safeThis->showTrimeshGuideAttachmentMenu(parameterField, targetScreenArea);
            }
        };
        callbacks.selectVertex = [safeThis](int vertexIndex) {
            if (safeThis != nullptr) {
                safeThis->selectTrimeshVertexIndex(vertexIndex);
            }
        };
        trimeshExpandedEditor->setCallbacks(std::move(callbacks));
        addAndMakeVisible(trimeshExpandedEditor.get());
    }

    widget.setMeshEditedCallback([safeThis, nodeId = node->id] {
        if (safeThis != nullptr) {
            safeThis->persistTrimeshMeshEdits(nodeId);
        }
    });

    const Rectangle<int> editorBounds = expandedEditorBounds(getLocalBounds().toFloat()).toNearestInt();

    if (trimeshExpandedEditor->getBounds() != editorBounds) {
        trimeshExpandedEditor->setBounds(editorBounds);
    }

    trimeshExpandedEditor->setRenderProfile(renderProfileForNodeOutput(*node, "out"));
    trimeshExpandedEditor->setGuideAttachmentLabels(trimeshGuideAttachmentLabelsForNode(*node));
    trimeshExpandedEditor->setNode(*node);
    trimeshExpandedEditor->setVisible(true);
    trimeshExpandedEditor->toFront(false);
}

void NodeCanvas::hideExpandedEditorHosts() {
    hideExpandedEditorHostsExcept({});
}

void NodeCanvas::hideExpandedEditorHostsExcept(const String& nodeId) {
    for (auto& entry : trimeshWidgets) {
        if (entry.first == nodeId) {
            continue;
        }

        Component* component = entry.second->getExpandedPanel3DComponentIfCreated();
        Component* waveComponent = entry.second->getExpandedPanel2DComponentIfCreated();

        if (component != nullptr && component->getParentComponent() == this && component->isVisible()) {
            component->setVisible(false);
        }

        if (waveComponent != nullptr && waveComponent->getParentComponent() == this && waveComponent->isVisible()) {
            waveComponent->setVisible(false);
        }
    }

    for (auto& entry : effect2DWidgets) {
        if (entry.first == nodeId) {
            continue;
        }

        Component* component = entry.second->getExpandedPanelComponentIfCreated();

        if (component != nullptr && component->isVisible()) {
            component->setVisible(false);
        }
    }
}

void NodeCanvas::detachExpandedEditorHosts() {
    for (auto& entry : trimeshWidgets) {
        Component* component = entry.second->getExpandedPanel3DComponentIfCreated();
        Component* waveComponent = entry.second->getExpandedPanel2DComponentIfCreated();

        if (component != nullptr && component->getParentComponent() == this) {
            removeChildComponent(component);
        }

        if (waveComponent != nullptr && waveComponent->getParentComponent() == this) {
            removeChildComponent(waveComponent);
        }
    }

    if (effect2DExpandedEditor != nullptr && effect2DExpandedEditor->getParentComponent() == this) {
        removeChildComponent(effect2DExpandedEditor.get());
    }
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
    const int activeSectionIndex = activePaletteSectionIndex;

    auto drawEntryIcon = [&](NodeKind kind, Rectangle<float> area) {
        Node previewNode = GraphNodeFactory().createNode(kind, {}, {});
        const PortDomain domain = previewNode.outputs.empty()
                ? (previewNode.inputs.empty() ? PortDomain::ControlSignal : previewNode.inputs.front().domain)
                : previewNode.outputs.front().domain;
        drawPreviewUncached(g, previewNode, area, domain);
    };

    for (int sectionIndex = 0; sectionIndex < (int) std::size(kPaletteSections); ++sectionIndex) {
        const auto& section = kPaletteSections[(size_t) sectionIndex];
        const bool active = sectionIndex == activeSectionIndex;
        const Rectangle<float> button = paletteGroupBounds(sectionIndex);

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

    const auto& activeSection = kPaletteSections[(size_t) activeSectionIndex];
    const Colour sectionColour = Colour(0xff8290a2);

    for (int entryIndex = 0; entryIndex < activeSection.entryCount; ++entryIndex) {
        const auto& entry = activeSection.entries[entryIndex];
        const Rectangle<float> row = palettePulloutEntryBounds(activeSectionIndex, entryIndex);
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

bool NodeCanvas::resolveCableEndpoints(
        const Edge& edge,
        CableEndpoint& sourceEndpoint,
        CableEndpoint& destEndpoint) const {
    const Node* sourceNode = findNode(edge.sourceNodeId);
    const Node* destNode = findNode(edge.destNodeId);

    if (sourceNode == nullptr || destNode == nullptr) {
        return false;
    }

    const Port* sourcePort = findPort(*sourceNode, edge.sourcePortId, false);
    const Port* destPort = findPort(*destNode, edge.destPortId, true);

    if (sourcePort == nullptr) {
        return false;
    }

    const PortLocation sourceLocation = getPortLocation(*sourceNode, *sourcePort);
    sourceEndpoint = { sourceLocation.centre, sourcePort->side, true };

    if (destPort != nullptr) {
        const PortLocation destLocation = getPortLocation(*destNode, *destPort);
        destEndpoint = { destLocation.centre, destPort->side, true };
        return true;
    }

    if (edge.attachment && isDynamicTrimeshGuideTarget(*destNode, edge.destPortId)) {
        destEndpoint = dynamicTrimeshGuideEndpoint(*destNode, edge.destPortId);
        return true;
    }

    return false;
}

bool NodeCanvas::isDynamicTrimeshGuideTarget(const Node& node, const String& portId) const {
    if (node.kind != NodeKind::TrilinearMesh) {
        return false;
    }

    return TrimeshGuideAttachmentTarget::parse(portId).isValid();
}

NodeCanvas::CableEndpoint NodeCanvas::dynamicTrimeshGuideEndpoint(
        const Node& node,
        const String& portId) const {
    const Rectangle<float> bounds = toScreen(node.bounds);
    const TrimeshGuideAttachmentTarget target = TrimeshGuideAttachmentTarget::parse(portId);
    const int fieldIndex = jmax(0, target.fieldIndex());
    const int fieldCount = TrimeshGuideAttachmentTarget::fieldCount;

    return {
            {
                    bounds.getX() + bounds.getWidth() * ((float) fieldIndex + 1.f) / ((float) fieldCount + 1.f),
                    bounds.getY()
            },
            PortSide::Top,
            false
    };
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

bool NodeCanvas::findPaletteKindAt(Point<float> screenPosition, NodeKind& kind) const {
    int sectionIndex = paletteGroupIndexAt(screenPosition);

    if (sectionIndex < 0
            && activePaletteSectionIndex >= 0
            && paletteHoverBounds(activePaletteSectionIndex).contains(screenPosition)) {
        sectionIndex = activePaletteSectionIndex;
    }

    if (sectionIndex < 0) {
        return false;
    }

    const auto& section = kPaletteSections[(size_t) sectionIndex];

    for (int entryIndex = 0; entryIndex < section.entryCount; ++entryIndex) {
        if (palettePulloutEntryBounds(sectionIndex, entryIndex).contains(screenPosition)) {
            kind = section.entries[entryIndex].kind;
            return true;
        }
    }

    return false;
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

        if (voiceDomainButtonBounds(toScreen(node.bounds), zoom).expanded(4.f * zoom).contains(screenPosition)) {
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

    for (int edgeIndex = (int) edges.size() - 1; edgeIndex >= 0; --edgeIndex) {
        const auto& edge = edges[(size_t) edgeIndex];

        if (edge.sourceNodeId == nodeId || edge.destNodeId == nodeId) {
            continue;
        }

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
        PathStrokeType(22.f, PathStrokeType::curved, PathStrokeType::rounded)
                .createStrokedPath(hitPath, createCablePath(
                        getPortLocation(*sourceNode, *sourcePort).centre,
                        getPortLocation(*destNode, *destPort).centre,
                        sourcePort->side,
                        destPort->side,
                        edge.attachment));

        if (hitPath.contains(screenPosition)) {
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

    if (findPaletteKindAt(screenPosition, paletteKind)) {
        Node node = GraphNodeFactory().createNode(paletteKind, {}, {});
        return "Create " + node.title + "  /  " + node.subtitle;
    }

    int paletteSectionIndex = paletteGroupIndexAt(screenPosition);
    if (paletteSectionIndex < 0
            && activePaletteSectionIndex >= 0
            && paletteHoverBounds(activePaletteSectionIndex).contains(screenPosition)) {
        paletteSectionIndex = activePaletteSectionIndex;
    }

    if (paletteSectionIndex >= 0) {
        return "Node group  /  " + String(kPaletteSections[(size_t) paletteSectionIndex].title);
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
            return "Voice start domain  /  " + voiceDomainButtonLabel(*node)
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
    const float paletteRight = 18.f + kPaletteWidth;
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
    previewSpriteCache.clear();
    presentation.refresh(graph, document.revision(), document.lastChange());
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
    repaint();
}

var NodeCanvas::exportAutomationState() const {
    auto* root = new DynamicObject();
    root->setProperty("schema", "cycle-v2-automation-state.v1");
    root->setProperty("width", getWidth());
    root->setProperty("height", getHeight());
    root->setProperty("zoom", viewport.getZoom());
    root->setProperty("panX", viewport.getPan().x);
    root->setProperty("panY", viewport.getPan().y);
    root->setProperty("selectedNodeId", selectedNodeId);
    root->setProperty("expandedNodeId", expandedNodeId);
    root->setProperty("selectedEdgeIndex", selectedEdgeIndex);
    root->setProperty("editStatusMessage", editStatusMessage);
    root->setProperty("nodeCount", (int) graph.getNodes().size());
    root->setProperty("edgeCount", (int) graph.getEdges().size());
    root->setProperty("compileSucceeded", compileResult.succeeded());
    root->setProperty("validationIssueCount", (int) compileResult.validationIssues.size());
    root->setProperty("compileIssueCount", (int) compileResult.compileIssues.size());
    root->setProperty("runtimeTraceNodeCount", (int) runtimeTrace.nodes.size());
    root->setProperty("previewNodeCount", (int) previewResult.nodes.size());

    Array<var> nodes;
    for (const auto& node : graph.getNodes()) {
        auto* nodeObject = new DynamicObject();
        nodeObject->setProperty("id", node.id);
        nodeObject->setProperty("kind", labelForNodeKind(node.kind));
        nodeObject->setProperty("title", node.title);
        nodeObject->setProperty("subtitle", node.subtitle);
        nodeObject->setProperty("bounds", rectangleToVar(node.bounds));

        Array<var> inputs;
        for (const auto& port : node.inputs) {
            inputs.add(portToVar(port));
        }

        Array<var> outputs;
        for (const auto& port : node.outputs) {
            outputs.add(portToVar(port));
        }

        Array<var> parameters;
        for (const auto& parameter : node.parameters) {
            parameters.add(parameterToVar(parameter));
        }

        nodeObject->setProperty("inputs", inputs);
        nodeObject->setProperty("outputs", outputs);
        nodeObject->setProperty("parameters", parameters);
        nodes.add(nodeObject);
    }

    Array<var> edges;
    for (const auto& edge : graph.getEdges()) {
        edges.add(edgeToVar(edge));
    }

    Array<var> validationIssues;
    for (const auto& issue : compileResult.validationIssues) {
        auto* issueObject = new DynamicObject();
        issueObject->setProperty("message", issue.message);
        issueObject->setProperty("sourceNodeId", issue.sourceNodeId);
        issueObject->setProperty("sourcePortId", issue.sourcePortId);
        issueObject->setProperty("destNodeId", issue.destNodeId);
        issueObject->setProperty("destPortId", issue.destPortId);
        validationIssues.add(issueObject);
    }

    Array<var> compileIssues;
    for (const auto& issue : compileResult.compileIssues) {
        auto* issueObject = new DynamicObject();
        issueObject->setProperty("message", issue.message);
        compileIssues.add(issueObject);
    }

    Array<var> nodeOrder;
    for (const auto& nodeId : compileResult.plan.nodeOrder) {
        nodeOrder.add(nodeId);
    }

    Array<var> previewStats;
    for (const auto& preview : previewResult.nodes) {
        previewStats.add(previewStatsToVar(preview));
    }

    root->setProperty("nodes", nodes);
    root->setProperty("edges", edges);
    root->setProperty("validationIssues", validationIssues);
    root->setProperty("compileIssues", compileIssues);
    root->setProperty("nodeOrder", nodeOrder);
    root->setProperty("previewStats", previewStats);
    return root;
}

String NodeCanvas::exportGraphXml() const {
    return document.toXml();
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
    repaint();
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
    repaint();
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
    repaint();
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
    repaint();
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
    repaint();
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
    repaint();
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
    repaint();
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

    if (!beginTrimeshMorphEdit(axis, jlimit(0.f, 1.f, value))) {
        return false;
    }

    endTrimeshMorphEdit();
    return true;
}

bool NodeCanvas::setPrimaryAxisForAutomation(const String& nodeId, const String& axis) {
    if (expandedNodeId != nodeId && !openNodeEditorForAutomation(nodeId)) {
        return false;
    }

    return setTrimeshPrimaryAxisValue(axis);
}

bool NodeCanvas::toggleLinkForAutomation(const String& nodeId, const String& axis) {
    if (expandedNodeId != nodeId && !openNodeEditorForAutomation(nodeId)) {
        return false;
    }

    return toggleTrimeshLinkAxisValue(axis);
}

bool NodeCanvas::selectVertexForAutomation(const String& nodeId, int vertexIndex) {
    if (expandedNodeId != nodeId && !openNodeEditorForAutomation(nodeId)) {
        return false;
    }

    return selectTrimeshVertexIndex(vertexIndex);
}

bool NodeCanvas::setVertexParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        float value) {
    if (expandedNodeId != nodeId && !openNodeEditorForAutomation(nodeId)) {
        return false;
    }

    if (!beginTrimeshVertexParameterEdit(parameterId, jlimit(0.f, 1.f, value))) {
        return false;
    }

    endTrimeshVertexParameterEdit();
    repaint();
    return true;
}

bool NodeCanvas::getNodeParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        String& value) const {
    return automation.getParameter(nodeId, parameterId, value);
}

var NodeCanvas::inspectNodeControlsForAutomation(const String& nodeId) const {
    auto* root = new DynamicObject();
    const Node* node = findNode(nodeId);
    root->setProperty("nodeId", nodeId);
    root->setProperty("resolved", node != nullptr);

    if (node == nullptr) {
        return root;
    }

    root->setProperty("kind", labelForNodeKind(node->kind));
    root->setProperty("expanded", expandedNodeId == nodeId);

    Array<var> parameters;
    for (const auto& parameter : node->parameters) {
        parameters.add(parameterToVar(parameter));
    }
    root->setProperty("parameters", parameters);

    if (node->kind == NodeKind::TrilinearMesh) {
        Array<var> morphSliders;

        for (const auto& axis : { String("yellow"), String("red"), String("blue") }) {
            auto* slider = new DynamicObject();
            slider->setProperty("id", axis);
            slider->setProperty("value", parameterValueForNode(*node, axis, "0.5").getDoubleValue());
            slider->setProperty("minimum", 0.0);
            slider->setProperty("maximum", 1.0);
            morphSliders.add(slider);
        }

        Array<var> primaryAxisButtons;
        Array<var> linkToggles;

        for (const auto& axis : { String("yellow"), String("red"), String("blue") }) {
            auto* primary = new DynamicObject();
            primary->setProperty("id", axis);
            primary->setProperty("selected", parameterValueForNode(*node, "primaryAxis", "yellow") == axis);
            primaryAxisButtons.add(primary);

            auto* link = new DynamicObject();
            const String defaultValue = axis == "yellow" ? "1" : "0";
            link->setProperty("id", axis);
            link->setProperty("selected", parameterValueForNode(*node, "link." + axis, defaultValue).getIntValue() != 0);
            linkToggles.add(link);
        }

        root->setProperty("morphSliders", morphSliders);
        root->setProperty("primaryAxisButtons", primaryAxisButtons);
        root->setProperty("linkToggles", linkToggles);
    }

    if ((node->kind == NodeKind::Envelope
            || node->kind == NodeKind::GuideCurve
            || node->kind == NodeKind::ImpulseResponse
            || node->kind == NodeKind::Waveshaper)
            && expandedNodeId == nodeId
            && effect2DExpandedEditor != nullptr) {
        root->setProperty("effect2D", effect2DExpandedEditor->automationState());
    }

    return root;
}

var NodeCanvas::inspectPointerTargetsForAutomation() const {
    auto* root = new DynamicObject();
    Array<var> targets;

    targets.add(pointerTargetToVar(
            "canvas",
            "canvas",
            getLocalBounds().toFloat()));

    viewport.setBounds(getLocalBounds().toFloat());
    const auto& scene = sceneBuilder.build(
            graph, viewport, presentation.revision(), document.revision());
    for (const auto& sceneTarget : scene.targets) {
        if (sceneTarget.kind != NodeSceneTargetKind::Node && !sceneTarget.isPort()) {
            continue;
        }
        targets.add(pointerTargetToVar(
                sceneTarget.semanticId,
                sceneTarget.kind == NodeSceneTargetKind::Node
                        ? "node"
                        : (sceneTarget.kind == NodeSceneTargetKind::InputPort ? "inputPort" : "outputPort"),
                sceneTarget.bounds,
                sceneTarget.nodeId,
                sceneTarget.portId,
                sceneTarget.kind == NodeSceneTargetKind::InputPort));
    }

    for (const auto& sceneEdge : scene.edges) {
        const auto& edge = graph.getEdges()[(size_t) sceneEdge.edgeIndex];
        auto* target = new DynamicObject();
        target->setProperty("id", "edge:" + String(sceneEdge.edgeIndex));
        target->setProperty("kind", edge.attachment ? "attachmentEdge" : "edge");
        target->setProperty("edgeIndex", sceneEdge.edgeIndex);
        target->setProperty("bounds", rectangleToVar(sceneEdge.hitPath.getBounds()));
        target->setProperty("sourceNodeId", edge.sourceNodeId);
        target->setProperty("sourcePortId", edge.sourcePortId);
        target->setProperty("destNodeId", edge.destNodeId);
        target->setProperty("destPortId", edge.destPortId);
        targets.add(target);
    }

    const Node* expandedNode = findNode(expandedNodeId);
    if (expandedNode != nullptr) {
        Rectangle<float> panel = expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode);
        targets.add(pointerTargetToVar(
                "expanded:" + expandedNode->id,
                "expandedEditor",
                panel,
                expandedNode->id));
        targets.add(pointerTargetToVar(
                "expanded:" + expandedNode->id + ".close",
                "expandedCloseButton",
                expandedEditorCloseButton(panel),
                expandedNode->id));

        if (expandedNode->kind == NodeKind::TrilinearMesh) {
            Rectangle<float> content = panel;
            content.removeFromTop(kExpandedEditorHeaderHeight);
            content = content.reduced(10.f, 8.f);

            targets.add(pointerTargetToVar(
                    "expanded:" + expandedNode->id + ".panel3D",
                    "trimeshPanel3D",
                    TrimeshWidget::expandedGridPanelContentBounds(content),
                    expandedNode->id));
            targets.add(pointerTargetToVar(
                    "expanded:" + expandedNode->id + ".panel2D",
                    "trimeshPanel2D",
                    TrimeshWidget::expandedWavePanelContentBounds(content),
                    expandedNode->id));

            const auto& widget = const_cast<NodeCanvas*>(this)->trimeshWidgetFor(expandedNode->id);
            for (const auto& region : widget.expandedControlHitRegions(content)) {
                const String kind = trimeshHitRegionKind(region.kind);
                const String suffix = region.parameterId.isNotEmpty() ? region.parameterId : region.axisValue;
                targets.add(pointerTargetToVar(
                        "expanded:" + expandedNode->id + "." + kind + "." + suffix,
                        kind,
                        region.bounds,
                        expandedNode->id,
                        {},
                        false,
                        region.parameterId,
                        region.axisValue));
            }
        } else if ((expandedNode->kind == NodeKind::Envelope
                || expandedNode->kind == NodeKind::GuideCurve
                || expandedNode->kind == NodeKind::ImpulseResponse
                || expandedNode->kind == NodeKind::Waveshaper)
                && effect2DExpandedEditor != nullptr) {
            const Rectangle<float> panelHost = effect2DExpandedEditor
                    ->panelBoundsForAutomation()
                    .translated(panel.getX(), panel.getY());
            targets.add(pointerTargetToVar(
                    "expanded:" + expandedNode->id + ".panel2D",
                    "effect2DPanel",
                    panelHost,
                    expandedNode->id));
        }
    }

    root->setProperty("schema", "cycle-v2-pointer-targets.v1");
    root->setProperty("coordinateSpace", "canvasLocal");
    root->setProperty("targetCount", targets.size());
    root->setProperty("targets", targets);
    return root;
}

var NodeCanvas::inspectOpenGLDiagnosticsForAutomation() const {
    auto* root = new DynamicObject();
    root->setProperty("schema", "cycle-v2-opengl-diagnostics.v1");
    root->setProperty("canvasOpenGlAttached", canvasOpenGlAttached);
    root->setProperty("canvasBounds", rectangleToVar(getLocalBounds().toFloat()));
    root->setProperty("canvasScreenBounds", rectangleToVar(getScreenBounds().toFloat()));
    root->setProperty("expandedNodeId", expandedNodeId);
    root->setProperty("trimeshExpandedEditorCreated", trimeshExpandedEditor != nullptr);

    if (trimeshExpandedEditor != nullptr) {
        root->setProperty("trimeshExpandedEditor", componentDiagnosticsToVar(
                "trimeshExpandedEditor",
                trimeshExpandedEditor.get()));
    }

    const Node* expandedNode = findNode(expandedNodeId);
    if (expandedNode != nullptr) {
        root->setProperty("expandedNodeKind", labelForNodeKind(expandedNode->kind));
        root->setProperty("expandedEditorBounds", rectangleToVar(
                expandedEditorBoundsForNode(getLocalBounds().toFloat(), expandedNode)));
    }

    Array<var> panels;
    if (expandedNode != nullptr && expandedNode->kind == NodeKind::TrilinearMesh) {
        TrimeshWidget* widget = nullptr;

        for (const auto& entry : trimeshWidgets) {
            if (entry.first == expandedNode->id) {
                widget = entry.second.get();
                break;
            }
        }

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
    auto* root = new DynamicObject();
    root->setProperty("schema", "cycle-v2-audio-capture.v1");
    root->setProperty("compileSucceeded", compileResult.succeeded());
    root->setProperty("requestedFrames", (int) frameCount);

    if (!compileResult.succeeded()) {
        Array<var> issues;

        for (const auto& issue : compileResult.validationIssues) {
            auto* object = new DynamicObject();
            object->setProperty("message", issue.message);
            object->setProperty("sourceNodeId", issue.sourceNodeId);
            object->setProperty("destNodeId", issue.destNodeId);
            issues.add(object);
        }

        root->setProperty("validationIssues", issues);
        return root;
    }

    const GraphAudioResult result = presentation.captureAudio(graph, frameCount);
    root->setProperty("metrics", audioMetricsToVar(result.output, 44100.0));

    Array<var> samples;
    for (float sample : result.output.block.samples) {
        samples.add(sample);
    }
    root->setProperty("samples", samples);

    Array<var> nodes;
    for (const auto& node : result.nodes) {
        auto* object = new DynamicObject();
        object->setProperty("nodeId", node.nodeId);
        object->setProperty("metrics", audioMetricsToVar(node.output, 44100.0));
        object->setProperty("outputCount", (int) node.outputs.size());
        nodes.add(object);
    }

    root->setProperty("nodeCount", nodes.size());
    root->setProperty("nodes", nodes);
    return root;
}

File NodeCanvas::snapshotFile() const {
    return File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("CycleV2")
            .getChildFile("graph-snapshot.xml");
}

bool NodeCanvas::saveGraphToFile(const File& file) {
    if (!document.save(file)) {
        editStatusMessage = "Save failed";
        repaint();
        return false;
    }

    editStatusMessage = "Saved " + file.getFileName();
    repaint();
    return true;
}

bool NodeCanvas::loadGraphFromFile(const File& file) {
    if (!document.load(file)) {
        editStatusMessage = "Open failed";
        repaint();
        return false;
    }
    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    spliceTargetEdgeIndex = -1;
    refreshCompiledState();
    editStatusMessage = "Opened " + file.getFileName();
    repaint();
    return true;
}

bool NodeCanvas::saveSnapshot() {
    const File file = snapshotFile();
    if (!document.save(file)) {
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

    if (!document.load(file)) {
        editStatusMessage = "Load failed";
        repaint();
        return true;
    }
    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    spliceTargetEdgeIndex = -1;
    refreshCompiledState();
    editStatusMessage = "Loaded snapshot";
    repaint();
    return true;
}

bool NodeCanvas::undo() {
    if (!document.undo()) {
        editStatusMessage = "Nothing to undo";
        repaint();
        return true;
    }
    return restoreGraphXml({}, "Undo");
}

bool NodeCanvas::redo() {
    if (!document.redo()) {
        editStatusMessage = "Nothing to redo";
        repaint();
        return true;
    }
    return restoreGraphXml({}, "Redo");
}

bool NodeCanvas::restoreGraphXml(const String& xml, const String& statusMessage) {
    if (xml.isNotEmpty() && !document.loadXml(xml, false)) {
        editStatusMessage = "Restore failed";
        repaint();
        return true;
    }
    selectedNodeId = {};
    expandedNodeId = {};
    selectedEdgeIndex = -1;
    spliceTargetEdgeIndex = -1;
    refreshCompiledState();
    editStatusMessage = statusMessage;
    repaint();
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

    auto result = node->kind == NodeKind::Spy
            ? commands.attachSpyToEdge((size_t) edgeIndex, selectedNodeId)
            : commands.spliceNodeIntoEdge((size_t) edgeIndex, selectedNodeId);

    if (result.succeeded()) {
        selectedNodeId = result.nodeId;
        selectedEdgeIndex = -1;
        expandedNodeId = {};
        if (node->kind != NodeKind::Spy) {
            shoveNodesForwardAfterSplice(result.nodeId, originalEdge.destNodeId);
        }
        refreshCompiledState();
        editStatusMessage = node->kind == NodeKind::Spy ? "Attached spy to cable" : "Inserted node into cable";
        return true;
    }

    if (result.code == GraphEditCode::ValidationRejected) {
        editStatusMessage = "Cable insert incompatible";
    }

    return false;
}

void NodeCanvas::shoveNodesForwardAfterSplice(const String& insertedNodeId, const String& downstreamNodeId) {
    const Node* insertedNode = findNode(insertedNodeId);
    const Node* downstreamNode = findNode(downstreamNodeId);

    if (insertedNode == nullptr || downstreamNode == nullptr) {
        return;
    }

    const float desiredDownstreamLeft = insertedNode->bounds.getRight() + 56.f;
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
    repaint();
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

    const String domain = nextVoiceDomain(*node);
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
    repaint();
    return true;
}

bool NodeCanvas::handleVoiceContextEditorClick(Point<float> screenPosition) {
    const Node* node = findNode(expandedNodeId);

    if (node == nullptr || node->kind != NodeKind::VoiceContext) {
        return false;
    }

    Rectangle<float> column = voiceContextEditorColumnBounds(expandedEditorBoundsForNode(getLocalBounds().toFloat(), node));

    auto nextRow = [&]() {
        Rectangle<float> row = column.removeFromTop(kVoiceEditorRowHeight);
        column.removeFromTop(kVoiceEditorRowGap);
        return row;
    };

    Rectangle<float> sourceRow = nextRow();
    Rectangle<float> sourceControl = sourceRow.withTrimmedLeft(kVoiceEditorLabelWidth).reduced(0.f, 2.f);
    const Rectangle<float> waveformLabel = sourceControl.removeFromLeft(62.f);
    sourceControl.removeFromLeft(8.f);
    const Rectangle<float> switchArea = sourceControl.removeFromLeft(42.f);
    sourceControl.removeFromLeft(8.f);
    const Rectangle<float> spectralLabel = sourceControl.removeFromLeft(56.f);

    if (waveformLabel.expanded(4.f, 2.f).contains(screenPosition)
            || switchArea.expanded(4.f, 2.f).contains(screenPosition)) {
        if (switchArea.expanded(4.f, 2.f).contains(screenPosition)) {
            return setVoiceContextParameter("domain", "Start Domain", nextVoiceDomain(*node),
                                            "Voice start domain: " + nextVoiceDomain(*node));
        }

        return setVoiceContextParameter("domain", "Start Domain", "waveform", "Voice start domain: waveform");
    }

    if (spectralLabel.expanded(4.f, 2.f).contains(screenPosition)) {
        return setVoiceContextParameter("domain", "Start Domain", "spectral", "Voice start domain: spectral");
    }

    Rectangle<float> octaveRow = nextRow();
    Rectangle<float> octaveControl = octaveRow.withTrimmedLeft(kVoiceEditorLabelWidth).reduced(2.f, 0.f);
    if (octaveControl.expanded(8.f, 4.f).contains(screenPosition)) {
        const float left = octaveControl.getX() + 2.f;
        const float right = octaveControl.getRight() - 2.f;
        const float normalized = jlimit(0.f, 1.f, (screenPosition.x - left) / jmax(1.f, right - left));
        const int octave = jlimit(-2, 2, roundToInt(normalized * 4.f) - 2);
        return setVoiceContextParameter("octave", "Octave", String(octave), "Octave: " + String(octave));
    }

    Rectangle<float> pitchRow = nextRow();
    Rectangle<float> pitchControl = pitchRow.withTrimmedLeft(kVoiceEditorLabelWidth).reduced(2.f, 0.f);
    if (pitchControl.expanded(8.f, 4.f).contains(screenPosition)) {
        const float normalized = jlimit(0.f, 1.f,
                                        (screenPosition.x - pitchControl.getX()) / jmax(1.f, pitchControl.getWidth()));
        const int pitch = roundToInt(jmap(normalized, -12.f, 12.f));
        return setVoiceContextParameter("pitch", "Pitch", String(pitch), "Pitch: " + String(pitch));
    }

    Rectangle<float> portamentoRow = nextRow();
    Rectangle<float> portamentoControl = portamentoRow.withTrimmedLeft(kVoiceEditorLabelWidth);
    if (portamentoControl.expanded(6.f, 4.f).contains(screenPosition)) {
        const bool enabled = parameterValueForNode(*node, "portamento", "0") == "1"
                || parameterValueForNode(*node, "portamento", "false") == "true";
        return setVoiceContextParameter("portamento", "Portamento", enabled ? "0" : "1",
                                        enabled ? "Portamento off" : "Portamento on");
    }

    Rectangle<float> oversamplingRow = nextRow();
    Rectangle<float> oversamplingControl = oversamplingRow.withTrimmedLeft(kVoiceEditorLabelWidth).reduced(2.f, 0.f);
    if (oversamplingControl.expanded(8.f, 6.f).contains(screenPosition)) {
        const float normalized = jlimit(0.f, 1.f,
                                        (screenPosition.x - oversamplingControl.getX())
                                                / jmax(1.f, oversamplingControl.getWidth()));
        const int stop = jlimit(0, 2, roundToInt(normalized * 2.f));
        const String next = stop == 0 ? "1x" : (stop == 1 ? "2x" : "4x");
        return setVoiceContextParameter("oversampling", "Oversampling", next, "Oversampling: " + next);
    }

    return false;
}

bool NodeCanvas::setTransformParameter(
        const String& parameterId,
        const String& label,
        const String& value,
        const String& statusMessage) {
    const Node* node = findNode(expandedNodeId);

    if (node == nullptr || (node->kind != NodeKind::Fft && node->kind != NodeKind::Ifft)) {
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

    if (parameterId == "mode") {
        const String nodeId = node->id;
        commands.editNodePresentation(nodeId, [&](Node& edited) {
            edited.subtitle = transformSubtitleForMode(edited, value);
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
    repaint();
    return true;
}

bool NodeCanvas::handleTransformEditorClick(Point<float> screenPosition) {
    const Node* node = findNode(expandedNodeId);

    if (node == nullptr || (node->kind != NodeKind::Fft && node->kind != NodeKind::Ifft)) {
        return false;
    }

    Rectangle<float> column = transformEditorColumnBounds(expandedEditorBoundsForNode(getLocalBounds().toFloat(), node));
    Rectangle<float> modeRow = column.removeFromTop(26.f);
    Rectangle<float> control = modeRow.withTrimmedLeft(kVoiceEditorLabelWidth).reduced(2.f, 0.f);
    Rectangle<float> left = control.removeFromLeft((control.getWidth() - 5.f) * 0.5f);
    control.removeFromLeft(5.f);
    Rectangle<float> right = control;

    if (left.expanded(4.f, 4.f).contains(screenPosition)) {
        const String mode = node->kind == NodeKind::Fft ? "cycle" : "cyclic";
        return setTransformParameter("mode", "Mode", mode, transformModeStatus(*node, mode));
    }

    if (right.expanded(4.f, 4.f).contains(screenPosition)) {
        const String mode = node->kind == NodeKind::Fft ? "fixedWindow" : "acyclicCarry";
        return setTransformParameter("mode", "Mode", mode, transformModeStatus(*node, mode));
    }

    return false;
}

bool NodeCanvas::setTrimeshPrimaryAxisValue(const String& axisValue) {
    const Node* node = findNode(expandedNodeId);

    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }

    if (parameterValueForNode(*node, "primaryAxis", "yellow") == axisValue) {
        return true;
    }

    auto result = commands.setNodeParameter(
            node->id,
            "primaryAxis",
            "Primary Axis",
            axisValue);

    if (!result.succeeded()) {
        return false;
    }

    refreshCompiledState();
    selectedNodeId = node->id;
    selectedEdgeIndex = -1;
    editStatusMessage = "Primary view axis: " + axisValue;
    repaint();
    return true;
}

bool NodeCanvas::toggleTrimeshLinkAxisValue(const String& axisValue) {
    const Node* node = findNode(expandedNodeId);

    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }

    const String parameterId = "link." + axisValue;
    const String defaultValue = axisValue == "yellow" ? "1" : "0";
    const bool linked = parameterValueForNode(*node, parameterId, defaultValue).getIntValue() != 0;
    auto result = commands.setNodeParameter(
            node->id,
            parameterId,
            "Link " + axisValue,
            linked ? "0" : "1");

    if (!result.succeeded()) {
        return false;
    }

    refreshCompiledState();
    selectedNodeId = node->id;
    selectedEdgeIndex = -1;
    editStatusMessage = "Link " + axisValue + (linked ? " off" : " on");
    repaint();
    return true;
}

bool NodeCanvas::beginTrimeshMorphEdit(const String& parameterId, float value) {
    const Node* node = findNode(expandedNodeId);

    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }

    selectedNodeId = node->id;
    selectedEdgeIndex = -1;
    activeTrimeshMorphNodeId = node->id;
    activeTrimeshMorphParameterId = parameterId;
    draggingTrimeshMorph = true;
    trimeshMorphUndoPushed = false;
    commands.beginCompoundEdit();
    return updateTrimeshMorphEditValue(value);
}

bool NodeCanvas::updateTrimeshMorphEditValue(float value) {
    const Node* node = findNode(activeTrimeshMorphNodeId);

    if (node == nullptr || node->kind != NodeKind::TrilinearMesh || activeTrimeshMorphParameterId.isEmpty()) {
        return false;
    }

    const String label = activeTrimeshMorphParameterId.substring(0, 1).toUpperCase()
            + activeTrimeshMorphParameterId.substring(1);
    commands.setNodeParameter(
            node->id,
            activeTrimeshMorphParameterId,
            label,
            String(value, 3));
    scheduleCompiledStateRefresh();
    editStatusMessage = "Morph " + label + " = " + String(value, 2);
    repaint();
    return true;
}

void NodeCanvas::endTrimeshMorphEdit() {
    commands.commitCompoundEdit();
    flushScheduledCompiledStateRefresh();
    draggingTrimeshMorph = false;
    trimeshMorphUndoPushed = false;
    activeTrimeshMorphNodeId = {};
    activeTrimeshMorphParameterId = {};
}

bool NodeCanvas::beginTrimeshVertexParameterEdit(const String& parameterId, float value) {
    const Node* node = findNode(expandedNodeId);

    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }

    int vertexIndex = parameterValueForNode(*node, "selectedVertexIndex", "-1").getIntValue();
    commands.beginCompoundEdit();

    if (vertexIndex < 0) {
        vertexIndex = trimeshWidgetFor(node->id).resolvedSelectedVertexIndexForNode(*node);

        if (vertexIndex >= 0) {
            commands.setNodeParameter(
                    node->id,
                    "selectedVertexIndex",
                    "Selected Vertex",
                    String(vertexIndex));
        }
    }

    if (vertexIndex < 0) {
        commands.cancelCompoundEdit();
        return false;
    }

    selectedNodeId = node->id;
    selectedEdgeIndex = -1;
    activeTrimeshVertexNodeId = node->id;
    activeTrimeshVertexParameterId = parameterId;
    activeTrimeshVertexIndex = vertexIndex;
    draggingTrimeshVertexParameter = true;
    return updateTrimeshVertexParameterEditValue(value);
}

bool NodeCanvas::updateTrimeshVertexParameterEditValue(float value) {
    const Node* node = findNode(activeTrimeshVertexNodeId);

    if (node == nullptr
            || node->kind != NodeKind::TrilinearMesh
            || activeTrimeshVertexParameterId.isEmpty()) {
        return false;
    }

    String label = activeTrimeshVertexParameterId.fromFirstOccurrenceOf(".", false, false);
    if (label.isEmpty()) {
        label = activeTrimeshVertexParameterId;
    }

    if (activeTrimeshVertexIndex < 0) {
        return false;
    }

    const String persistentParameterId = TrimeshMeshEditState::canonicalVertexParameterId(
            activeTrimeshVertexIndex,
            label);
    commands.setNodeParameter(
            node->id,
            persistentParameterId,
            label,
            String(value, 3));
    scheduleCompiledStateRefresh();
    editStatusMessage = "Vertex #" + String(activeTrimeshVertexIndex) + " " + label + " = " + String(value, 2);
    return true;
}

void NodeCanvas::endTrimeshVertexParameterEdit() {
    commands.commitCompoundEdit();
    flushScheduledCompiledStateRefresh();
    draggingTrimeshVertexParameter = false;
    trimeshVertexParameterUndoPushed = false;
    activeTrimeshVertexNodeId = {};
    activeTrimeshVertexParameterId = {};
    activeTrimeshVertexIndex = -1;
}

void NodeCanvas::persistTrimeshMeshEdits(const String& nodeId) {
    const Node* node = findNode(nodeId);

    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return;
    }

    commands.beginCompoundEdit();
    TrimeshMeshEditState editState = trimeshWidgetFor(nodeId).currentMeshEditState();
    for (const TrimeshVertexEdit& edit : editState.getVertexEdits()) {
        const String field = TrimeshMeshEditState::fieldForVertexValueIndex(edit.valueIndex);

        if (field.isEmpty()) {
            continue;
        }

        commands.setNodeParameter(
                nodeId,
                TrimeshMeshEditState::canonicalVertexParameterId(edit.vertexIndex, field),
                field,
                String(edit.value, 6));
    }
    commands.commitCompoundEdit();

    scheduleCompiledStateRefresh();
    repaint();
}

bool NodeCanvas::showTrimeshGuideAttachmentMenu(
        const String& parameterField,
        Rectangle<int> targetScreenArea) {
    if (trimeshExpandedEditorNodeId.isEmpty()) {
        return false;
    }

    TrimeshWidget& widget = trimeshWidgetFor(trimeshExpandedEditorNodeId);
    const Node* meshNode = findNode(trimeshExpandedEditorNodeId);

    if (meshNode == nullptr || meshNode->kind != NodeKind::TrilinearMesh) {
        return false;
    }

    const int vertexIndex = widget.resolvedSelectedVertexIndexForNode(*meshNode);
    const auto items = TrimeshGuideAttachmentMenu::itemsFor(
            graph,
            meshNode->id,
            vertexIndex,
            parameterField);

    PopupMenu menu;

    for (const auto& item : items) {
        menu.addItem(
                item.menuId,
                item.label,
                true,
                item.attached);
    }

    auto safeThis = Component::SafePointer<NodeCanvas>(this);
    const String meshNodeId = meshNode->id;
    menu.showMenuAsync(
            PopupMenu::Options().withTargetScreenArea(targetScreenArea),
            [safeThis, meshNodeId, vertexIndex, parameterField, items](int selectedMenuId) {
                if (safeThis == nullptr || selectedMenuId == 0) {
                    return;
                }

                GraphEditResult result { GraphEditCode::ValidationRejected, {}, {} };

                if (selectedMenuId == TrimeshGuideAttachmentMenu::newGuideMenuId) {
                    result = safeThis->commands.createAndAttachGuideCurve(
                            meshNodeId,
                            vertexIndex,
                            parameterField,
                            safeThis->viewportCentreWorld());
                } else {
                    for (const auto& item : items) {
                        if (item.menuId != selectedMenuId) {
                            continue;
                        }

                        result = safeThis->commands.attachGuideCurve(
                                item.guideNodeId,
                                meshNodeId,
                                vertexIndex,
                                parameterField);
                        break;
                    }
                }

                if (!result.succeeded()) {
                    safeThis->editStatusMessage = "Guide attachment failed";
                    safeThis->repaint();
                    return;
                }

                safeThis->selectedNodeId = result.nodeId.isEmpty() ? meshNodeId : result.nodeId;
                safeThis->selectedEdgeIndex = -1;
                safeThis->refreshCompiledState();
                safeThis->updateExpandedEditorHost(safeThis->findNode(safeThis->expandedNodeId));
                safeThis->editStatusMessage = "Guide " + parameterField + " attached";
                safeThis->repaint();
            });

    return true;
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

bool NodeCanvas::selectTrimeshVertexIndex(int vertexIndex) {
    const Node* node = findNode(expandedNodeId);

    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }

    if (parameterValueForNode(*node, "selectedVertexIndex", "-1").getIntValue() == vertexIndex) {
        return true;
    }

    auto result = commands.setNodeParameter(
            node->id,
            "selectedVertexIndex",
            "Selected Vertex",
            String(vertexIndex));

    if (!result.succeeded()) {
        return false;
    }

    refreshCompiledState();
    selectedNodeId = node->id;
    selectedEdgeIndex = -1;
    editStatusMessage = "Selected vertex #" + String(vertexIndex);
    return true;
}

bool NodeCanvas::canConnectPorts(const PortAddress& first, const PortAddress& second) const {
    if (first.input == second.input) {
        return false;
    }

    NodeGraph candidate = graph;
    return GraphEditor().connect(candidate, first, second).succeeded();
}

void NodeCanvas::updatePaletteHover(Point<float> screenPosition) {
    if (activePaletteSectionIndex >= 0
            && paletteHoverBounds(activePaletteSectionIndex).contains(screenPosition)) {
        const int groupIndex = paletteGroupIndexAt(screenPosition);

        if (groupIndex >= 0
                && groupIndex != activePaletteSectionIndex
                && paletteGroupBounds(groupIndex).contains(screenPosition)) {
            activePaletteSectionIndex = groupIndex;
        }

        return;
    }

    const int groupIndex = paletteGroupIndexAt(screenPosition);

    if (groupIndex >= 0) {
        activePaletteSectionIndex = groupIndex;
        return;
    }

    activePaletteSectionIndex = -1;
}

Path NodeCanvas::createCablePath(
        Point<float> source,
        Point<float> dest,
        PortSide sourceSide,
        PortSide destSide,
        bool) const {
    const float zoom = viewport.getZoom();
    Path path;
    path.startNewSubPath(source);

    const Point<float> vector = dest - source;
    const float distance = vector.getDistanceFromOrigin();

    if (absoluteFloat(vector.x) <= 0.5f || absoluteFloat(vector.y) <= 0.5f) {
        path.lineTo(dest);
        return path;
    }

    const float sourceStrength = jlimit(24.f * zoom, 120.f * zoom, distance * 0.34f);
    const float destStrength = jlimit(18.f * zoom, 74.f * zoom, distance * 0.18f);
    const Point<float> sourceDirection = normalizedOrFallback(vector, outwardNormalForSide(sourceSide));
    const Point<float> destNormal = outwardNormalForSide(destSide);
    const Point<float> c1 = source + sourceDirection * sourceStrength;
    const Point<float> c2 = dest + destNormal * destStrength;

    path.cubicTo(c1, c2, dest);

    return path;
}

}
