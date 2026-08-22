#include <array>
#include <cmath>
#include <utility>

#include "NodeCanvasAutomationInspector.h"

#include "SignalProbeDetailView.h"
#include "../Nodes/Envelope/EnvelopePurpose.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"
#include "NodeViewModule.h"
#include "VoiceContextCompactEditor.h"

namespace CycleV2 {

namespace {

constexpr float kExpandedEditorHeaderHeight = 34.f;
constexpr float kExpandedEditorMinMargin = 18.f;

String updateProductLabel(UpdateProduct product) {
    switch (product) {
        case UpdateProduct::LocalSlice:          return "LocalSlice";
        case UpdateProduct::LocalSurface:        return "LocalSurface";
        case UpdateProduct::InteractionOverlay:  return "InteractionOverlay";
        case UpdateProduct::CompactPreview:      return "CompactPreview";
        case UpdateProduct::PreviewTraversal:    return "PreviewTraversal";
        case UpdateProduct::ProbePreview:        return "ProbePreview";
        case UpdateProduct::AudioConfiguration:  return "AudioConfiguration";
        case UpdateProduct::DurablePublication:  return "DurablePublication";
    }
    return "Unknown";
}

String updatePhaseLabel(UpdateTracePhase phase) {
    switch (phase) {
        case UpdateTracePhase::Dirtied:                  return "Dirtied";
        case UpdateTracePhase::Started:                  return "Started";
        case UpdateTracePhase::Completed:                return "Completed";
        case UpdateTracePhase::Published:                return "Published";
        case UpdateTracePhase::AlreadyCurrent:           return "AlreadyCurrent";
        case UpdateTracePhase::NotObserved:              return "NotObserved";
        case UpdateTracePhase::DeferredUntilCommit:      return "DeferredUntilCommit";
        case UpdateTracePhase::SupersededBeforeStart:    return "SupersededBeforeStart";
        case UpdateTracePhase::CancelledDuringExecution: return "CancelledDuringExecution";
        case UpdateTracePhase::StaleResultDiscarded:     return "StaleResultDiscarded";
        case UpdateTracePhase::NoEffectiveChange:        return "NoEffectiveChange";
        case UpdateTracePhase::InvariantViolation:       return "InvariantViolation";
    }
    return "Unknown";
}

String guideFieldLabel(GuideCurveField field) {
    switch (field) {
        case GuideCurveField::Time:       return "time";
        case GuideCurveField::Red:        return "red";
        case GuideCurveField::Blue:       return "blue";
        case GuideCurveField::Phase:      return "phase";
        case GuideCurveField::Amplitude:  return "amp";
        case GuideCurveField::Curve:      return "curve";
    }
    return {};
}

class AutomationValueEncoder {
public:
    static var rectangleToVar(Rectangle<float> bounds) {
        auto* object = new DynamicObject();
        object->setProperty("x", bounds.getX());
        object->setProperty("y", bounds.getY());
        object->setProperty("width", bounds.getWidth());
        object->setProperty("height", bounds.getHeight());
        return object;
    }

    static var pointToVar(Point<float> point) {
        auto* object = new DynamicObject();
        object->setProperty("x", point.x);
        object->setProperty("y", point.y);
        return object;
    }

    static Rectangle<float> rectangleFromVar(const var& value) {
        const auto* object = value.getDynamicObject();
        if (object == nullptr) {
            return {};
        }
        return {
            static_cast<float>(object->getProperty("x")),
            static_cast<float>(object->getProperty("y")),
            static_cast<float>(object->getProperty("width")),
            static_cast<float>(object->getProperty("height"))
        };
    }

    static var pointerTargetToVar(const String& id, const String& kind, Rectangle<float> bounds,
                                  const String& nodeId = {}, const String& portId = {}, bool input = false,
                                  const String& parameterId = {}, const String& axis = {}) {
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

    static var portToVar(const Port& port) {
        auto* object = new DynamicObject();
        object->setProperty("id", port.id);
        object->setProperty("label", port.label);
        object->setProperty("domain", labelForDomain(port.domain));
        object->setProperty("channelLayout", labelForChannelLayout(port.channelLayout));
        object->setProperty("purpose", port.purpose == PortPurpose::ScratchAttachment ? "scratchAttachment" : "signal");
        object->setProperty("input", port.input);
        return object;
    }

    static var parameterToVar(const NodeParameter& parameter) {
        auto* object = new DynamicObject();
        object->setProperty("id", parameter.id);
        object->setProperty("label", parameter.label);
        object->setProperty("value", parameter.value);
        return object;
    }

    static var edgeToVar(const Edge& edge) {
        auto* object = new DynamicObject();
        object->setProperty("sourceNodeId", edge.sourceNodeId);
        object->setProperty("sourcePortId", edge.sourcePortId);
        object->setProperty("destNodeId", edge.destNodeId);
        object->setProperty("destPortId", edge.destPortId);
        object->setProperty("domain", labelForDomain(edge.domain));
        object->setProperty("attachment", edge.isAttachment());
        object->setProperty("connectionKind", idForConnectionKind(edge.connectionKind));
        object->setProperty("attachmentType", idForAttachmentType(edge.attachmentType));
        return object;
    }

    static var probeToVar(const SignalProbe& probe) {
        auto* object = new DynamicObject();
        object->setProperty("id", probe.id);
        object->setProperty("sourceNodeId", probe.sourceNodeId);
        object->setProperty("sourcePortId", probe.sourcePortId);
        object->setProperty("anchorDestNodeId", probe.anchorDestNodeId);
        object->setProperty("anchorDestPortId", probe.anchorDestPortId);
        object->setProperty("label", probe.label);
        object->setProperty("tapPosition", probe.tapPosition);
        object->setProperty("railOrder", probe.railOrder);
        object->setProperty("connected", probe.sourceNodeId.isNotEmpty());
        return object;
    }

    static var probePreviewStatsToVar(const GraphPreviewResult::SignalProbePreview& preview) {
        auto* object = new DynamicObject();
        double absoluteTotal = 0.0;
        for (float value : preview.values) {
            absoluteTotal += (double) (value >= 0.f ? value : -value);
        }
        object->setProperty("probeId", preview.probeId);
        object->setProperty("connected", preview.connected);
        object->setProperty("domain", labelForDomain(preview.domain));
        object->setProperty("gridColumns", (int) preview.gridColumns);
        object->setProperty("gridRows", (int) preview.gridRows);
        object->setProperty("sampleCount", (int) preview.values.size());
        object->setProperty("absoluteSum", absoluteTotal);
        return object;
    }

    static String labelForPreviewRole(PreviewModuleRole role) {
        switch (role) {
        case PreviewModuleRole::Waveform:
            return "Waveform";
        case PreviewModuleRole::Image:
            return "Image";
        case PreviewModuleRole::MeshSurface:
            return "Mesh Surface";
        case PreviewModuleRole::SpectrumMagnitude:
            return "Spectrum Magnitude";
        case PreviewModuleRole::SpectrumPhase:
            return "Spectrum Phase";
        case PreviewModuleRole::Envelope:
            return "Envelope";
        case PreviewModuleRole::ImpulseResponse:
            return "Impulse Response";
        case PreviewModuleRole::Waveshaper:
            return "Waveshaper";
        case PreviewModuleRole::ReverbSpectrogram:
            return "Reverb Spectrogram";
        case PreviewModuleRole::EqualizerResponse:
            return "Equalizer Response";
        case PreviewModuleRole::OutputMeters:
            return "Output Meters";
        case PreviewModuleRole::VoiceContext:
            return "Voice Context";
        case PreviewModuleRole::SignalSpy:
            return "Signal Spy";
        case PreviewModuleRole::ModulationSource:
            return "Modulation Source";
        case PreviewModuleRole::Generic:
            return "Generic";
        case PreviewModuleRole::None:
        default:
            return "None";
        }
    }

    static var previewStatsToVar(const NodePreviewResult& preview) {
        auto* object = new DynamicObject();
        const auto& values = preview.primary;
        float minimum = values.empty() ? 0.f : values.front();
        float maximum = minimum;
        double total = 0.0;
        double absoluteTotal = 0.0;

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

    static var audioMetricsToVar(const SignalPayload& payload, double sampleRate) {
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

    static String trimeshHitRegionKind(TrimeshExpandedHitRegionKind kind) {
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

    static Rectangle<float> expandedEditorBounds(
            Rectangle<float> canvasContentBounds,
            const Node& node) {
        return NodeViewModuleRegistry::instance().moduleFor(node.kind).expandedEditorBounds(
                canvasContentBounds, kExpandedEditorMinMargin);
    }

    static Rectangle<float> expandedEditorCloseButton(Rectangle<float> panel) {
        return Rectangle<float>(22.f, 22.f)
                .withCentre({ panel.getRight() - 22.f, panel.getY() + kExpandedEditorHeaderHeight * 0.5f });
    }

    static void addEnvelopeModeTargets(
            Array<var>& targets,
            const Node& node,
            Rectangle<float> panel,
            const DynamicObject& effect2DState) {
        const var options = effect2DState.getProperty("modeOptions");
        if (!options.isArray()) {
            return;
        }

        for (const var& option : *options.getArray()) {
            const auto* object = option.getDynamicObject();
            if (object == nullptr) {
                continue;
            }
            const String mode = object->getProperty("id").toString();
            const Rectangle<float> bounds = AutomationValueEncoder::rectangleFromVar(
                    object->getProperty("bounds")).translated(panel.getX(), panel.getY());
            targets.add(pointerTargetToVar(
                    "expanded:" + node.id + ".envelopeMode." + mode,
                    "envelopeMode",
                    bounds,
                    node.id,
                    {},
                    false,
                    "purpose",
                    mode));
        }
    }

    static void addEnvelopeActionTargets(
            Array<var>& targets,
            const Node& node,
            Rectangle<float> panel,
            const DynamicObject& effect2DState) {
        const std::array<std::pair<const char*, const char*>, 5> actions {{
            { "loop", "loopBounds" },
            { "sustain", "sustainBounds" },
            { "logarithmic", "logarithmicBounds" },
            { "fitVertical", "fitVerticalBounds" },
            { "fullVertical", "fullVerticalBounds" }
        }};
        for (const auto& [id, boundsProperty] : actions) {
            const auto bounds = AutomationValueEncoder::rectangleFromVar(
                    effect2DState.getProperty(boundsProperty)).translated(
                            panel.getX(), panel.getY());
            targets.add(pointerTargetToVar(
                    "expanded:" + node.id + ".envelopeAction." + id,
                    "envelopeAction",
                    bounds,
                    node.id,
                    {},
                    false,
                    id));
        }
    }

    static void addEnvelopeTargets(
            Array<var>& targets,
            const Node& node,
            Rectangle<float> panel,
            const NodeEditorHost& editorHost) {
        DynamicObject editorState;
        editorHost.appendAutomationState(editorState);
        const var effect2D = editorState.getProperty("effect2D");
        const auto* effect2DState = effect2D.getDynamicObject();
        if (effect2DState == nullptr) {
            return;
        }
        addEnvelopeModeTargets(targets, node, panel, *effect2DState);
        addEnvelopeActionTargets(targets, node, panel, *effect2DState);
    }

    static void addExpandedEditorTargets(
            Array<var>& targets,
            const Node& node,
            Rectangle<float> canvasContentBounds,
            const NodeEditorHost& editorHost) {
        const Component* editorComponent = editorHost.component();
        const bool hostedComponentReady = editorComponent != nullptr
                && editorComponent->isShowing()
                && !editorComponent->getLocalBounds().isEmpty();
        const bool requiresHostedComponent = NodeViewModuleRegistry::instance()
                .moduleFor(node.kind).capabilities().hostedEditor;
        const bool nativeReady = hostedComponentReady || !requiresHostedComponent;
        Rectangle<float> panel = editorComponent != nullptr
                ? editorComponent->getBounds().toFloat()
                : expandedEditorBounds(canvasContentBounds, node);
        const int firstExpandedTarget = targets.size();
        targets.add(pointerTargetToVar("expanded:" + node.id, "expandedEditor", panel, node.id));
        targets.add(pointerTargetToVar("expanded:" + node.id + ".close", "expandedCloseButton",
                                       expandedEditorCloseButton(panel), node.id));

        if (node.kind == NodeKind::TrilinearMesh) {
            Rectangle<float> content = panel;
            content.removeFromTop(kExpandedEditorHeaderHeight);
            content = content.reduced(10.f, 8.f);

            targets.add(pointerTargetToVar("expanded:" + node.id + ".panel3D", "trimeshPanel3D",
                                           TrimeshWidget::expandedGridPanelContentBounds(content), node.id));
            targets.add(pointerTargetToVar("expanded:" + node.id + ".panel2D", "trimeshPanel2D",
                                           TrimeshWidget::expandedWavePanelContentBounds(content), node.id));

            for (const auto& region : TrimeshWidget::expandedControlHitRegions(content)) {
                const String kind = trimeshHitRegionKind(region.kind);
                const String suffix = region.parameterId.isNotEmpty() ? region.parameterId : region.axisValue;
                const Rectangle<float> targetBounds =
                        region.kind == TrimeshExpandedHitRegionKind::VertexParameter
                        ? region.bounds.reduced(5.f, 8.f)
                        : region.bounds;
                targets.add(pointerTargetToVar("expanded:" + node.id + "." + kind + "." + suffix, kind, targetBounds,
                                               node.id, {}, false, region.parameterId, region.axisValue));
            }
        } else if (node.kind == NodeKind::VoiceContext) {
            targets.add(pointerTargetToVar(
                    "expanded:" + node.id + ".octave",
                    "octave",
                    VoiceContextCompactEditor::octaveControlBounds(panel),
                    node.id));
            targets.add(pointerTargetToVar(
                    "expanded:" + node.id + ".voiceLength",
                    "voiceLength",
                    VoiceContextCompactEditor::voiceLengthControlBounds(panel),
                    node.id));
            targets.add(pointerTargetToVar(
                    "expanded:" + node.id + ".pitch",
                    "pitch",
                    VoiceContextCompactEditor::pitchControlBounds(panel),
                    node.id));
            targets.add(pointerTargetToVar(
                    "expanded:" + node.id + ".oversampling",
                    "oversampling",
                    VoiceContextCompactEditor::oversamplingControlBounds(panel),
                    node.id));
        } else {
            if (node.kind == NodeKind::Envelope) {
                addEnvelopeTargets(targets, node, panel, editorHost);
            }

            if (!editorHost.panelBoundsForAutomation().isEmpty()) {
                const Rectangle<float> panelHost =
                        editorHost.panelBoundsForAutomation().translated(panel.getX(), panel.getY());
                targets.add(pointerTargetToVar(
                        "expanded:" + node.id + ".panel2D",
                        "effect2DPanel",
                        panelHost,
                        node.id));
            }
        }

        for (int i = firstExpandedTarget; i < targets.size(); ++i) {
            auto* target = targets.getReference(i).getDynamicObject();
            if (target == nullptr) {
                continue;
            }

            target->setProperty("nativeReady", nativeReady);
            if (!hostedComponentReady) {
                continue;
            }

            const Rectangle<float> bounds = rectangleFromVar(target->getProperty("bounds"));
            const Point<float> editorLocalOrigin = bounds.getPosition() - panel.getPosition();
            const Rectangle<int> editorLocalBounds(
                    roundToInt(editorLocalOrigin.x),
                    roundToInt(editorLocalOrigin.y),
                    roundToInt(bounds.getWidth()),
                    roundToInt(bounds.getHeight()));
            target->setProperty(
                    "screenBounds",
                    rectangleToVar(editorComponent->localAreaToGlobal(editorLocalBounds).toFloat()));
        }
    }
};

}

NodeCanvasAutomationInspector::NodeCanvasAutomationInspector(NodeCanvasAutomationContext contextToUse) :
        context(contextToUse) {
}

var NodeCanvasAutomationInspector::exportState(const NodeCanvasAutomationPresentation& state) const {
    const NodeGraph& graph = context.document.graph();
    const auto& compileResult = context.presentation.compileResult();
    const auto& runtimeTrace = context.presentation.runtimeTrace();
    const auto& previewResult = context.presentation.previewResult();
    auto* root = new DynamicObject();
    root->setProperty("schema", "cycle-v2-automation-state.v1");
    root->setProperty("width", context.canvas.getWidth());
    root->setProperty("height", context.canvas.getHeight());
    root->setProperty("zoom", context.viewport.getZoom());
    root->setProperty("panX", context.viewport.getPan().x);
    root->setProperty("panY", context.viewport.getPan().y);
    root->setProperty("selectedNodeId", state.selectedNodeId);
    root->setProperty("expandedNodeId", state.expandedNodeId);
    root->setProperty("selectedEdgeIndex", state.selectedEdgeIndex);
    root->setProperty("previewVoiceLengthSeconds", state.previewVoiceLengthSeconds);
    root->setProperty("editStatusMessage", state.editStatusMessage);
    root->setProperty("nodeCount", (int) graph.getNodes().size());
    root->setProperty("edgeCount", (int) graph.getEdges().size());
    root->setProperty("probeCount", (int) graph.getSignalProbes().size());
    root->setProperty("guideCount", (int) graph.getGuideCurves().size());
    root->setProperty("guideAssignmentCount", (int) graph.getGuideAssignments().size());
    root->setProperty("probeDetailId", state.probeDetailId);
    root->setProperty("probeDetailOpen", state.probeDetailId.isNotEmpty());
    root->setProperty("probeDetailResolution", (int) state.probeDetailResolution);
    root->setProperty("probeDetailColumns", (int) state.probeDetailColumns);
    root->setProperty("probeDetailRows", (int) state.probeDetailRows);
    root->setProperty("compileSucceeded", compileResult.succeeded());
    root->setProperty("validationIssueCount", (int) compileResult.validationIssues.size());
    root->setProperty("compileIssueCount", (int) compileResult.compileIssues.size());
    root->setProperty("runtimeTraceNodeCount", (int) runtimeTrace.nodes.size());
    root->setProperty("previewNodeCount", (int) previewResult.nodes.size());
    root->setProperty(
            "probeRefreshMode",
            state.probeRefreshMode == ProbeRefreshMode::LiveLatest ? "Live" : "On Release");
    root->setProperty("expandedGuideId", state.guideDock.expandedGuideId);

    auto* guideDock = new DynamicObject();
    guideDock->setProperty("expanded", state.guideDock.expanded);
    guideDock->setProperty("guidesMinimized", state.guideDock.guidesMinimized);
    guideDock->setProperty("spiesMinimized", state.guideDock.spiesMinimized);
    guideDock->setProperty("expandedHeight", state.guideDock.expandedHeight);
    guideDock->setProperty("splitRatio", state.guideDock.splitRatio);
    guideDock->setProperty("guideHorizontalOffset", state.guideDock.guideHorizontalOffset);
    guideDock->setProperty("spyHorizontalOffset", state.guideDock.spyHorizontalOffset);
    guideDock->setProperty("selectedGuideId", state.guideDock.selectedGuideId);
    guideDock->setProperty("hoveredGuideId", state.guideDock.hoveredGuideId);
    guideDock->setProperty("keyboardFocusTarget", state.guideDock.keyboardFocusTarget);
    guideDock->setProperty("keyboardFocusItemId", state.guideDock.keyboardFocusItemId);
    guideDock->setProperty("guideTileCount", (int) state.guideDock.guideTiles.size());
    guideDock->setProperty("bounds", AutomationValueEncoder::rectangleToVar(state.guideDock.dockBounds));
    guideDock->setProperty(
            "guideShelfBounds",
            AutomationValueEncoder::rectangleToVar(state.guideDock.guideShelfBounds));
    guideDock->setProperty(
            "spyShelfBounds",
            AutomationValueEncoder::rectangleToVar(state.guideDock.spyShelfBounds));
    root->setProperty("guideDock", guideDock);

    Array<var> causalUpdates;
    for (const auto& event : context.presentation.updateTrace().snapshot()) {
        auto* update = new DynamicObject();
        update->setProperty("editId", String((int64_t) event.edit.editId));
        update->setProperty("gestureId", String((int64_t) event.edit.gestureId));
        update->setProperty("nodeId", event.nodeId);
        update->setProperty("product", updateProductLabel(event.product));
        update->setProperty("phase", updatePhaseLabel(event.phase));
        update->setProperty("fingerprint", String((int64_t) event.inputFingerprint));
        update->setProperty("sequence", String((int64_t) event.sequence));
        causalUpdates.add(update);
    }
    root->setProperty("causalUpdates", causalUpdates);

    Array<var> nodes;
    for (const auto& node : graph.getNodes()) {
        auto* nodeObject = new DynamicObject();
        nodeObject->setProperty("id", node.id);
        nodeObject->setProperty("kind", labelForNodeKind(node.kind));
        nodeObject->setProperty("title", labelForNodeKind(node.kind));
        nodeObject->setProperty("subtitle", node.subtitle);
        if (node.kind == NodeKind::Envelope) {
            nodeObject->setProperty(
                    "compactPurposeIcon",
                    envelopePurposeToString(envelopePurposeFor(node)));
        }
        nodeObject->setProperty("bounds", AutomationValueEncoder::rectangleToVar(node.bounds));

        Array<var> inputs;
        for (const auto& port : node.inputs) {
            inputs.add(AutomationValueEncoder::portToVar(port));
        }

        Array<var> outputs;
        for (const auto& port : node.outputs) {
            outputs.add(AutomationValueEncoder::portToVar(port));
        }

        Array<var> parameters;
        for (const auto& parameter : node.parameters) {
            parameters.add(AutomationValueEncoder::parameterToVar(parameter));
        }

        nodeObject->setProperty("inputs", inputs);
        nodeObject->setProperty("outputs", outputs);
        nodeObject->setProperty("parameters", parameters);
        if (node.model != nullptr) {
            nodeObject->setProperty("model", node.model->writeJSON());
        }
        if (node.editorState.getDynamicObject() != nullptr) {
            nodeObject->setProperty("editor", node.editorState);
        }
        nodes.add(nodeObject);
    }

    Array<var> edges;
    for (const auto& edge : graph.getEdges()) {
        edges.add(AutomationValueEncoder::edgeToVar(edge));
    }

    Array<var> probes;
    for (const auto& probe : graph.getSignalProbes()) {
        probes.add(AutomationValueEncoder::probeToVar(probe));
    }

    Array<var> guides;
    for (const auto& guide : graph.getGuideCurves()) {
        auto* guideObject = new DynamicObject();
        guideObject->setProperty("id", guide.id);
        guideObject->setProperty("shortLabel", guide.shortLabel);
        guideObject->setProperty("name", guide.name);
        guideObject->setProperty("colourIndex", guide.colourIndex);
        guideObject->setProperty("shelfOrder", guide.shelfOrder);
        guideObject->setProperty("usageCount", graph.guideUsageCount(guide.id));
        guideObject->setProperty("enabled", guide.enabled);
        guideObject->setProperty("noise", guide.noise);
        guideObject->setProperty("dcOffset", guide.dcOffset);
        guideObject->setProperty("phase", guide.phase);
        guideObject->setProperty("model", guide.model != nullptr ? guide.model->writeJSON() : var());
        guides.add(guideObject);
    }

    Array<var> guideAssignments;
    for (const auto& assignment : graph.getGuideAssignments()) {
        auto* target = new DynamicObject();
        target->setProperty("kind", "trimeshCubeComponent");
        target->setProperty("cubeIndex", assignment.target.cubeIndex);
        target->setProperty("field", guideFieldLabel(assignment.target.field));

        auto* assignmentObject = new DynamicObject();
        assignmentObject->setProperty("guideId", assignment.guideId);
        assignmentObject->setProperty("targetNodeId", assignment.targetNodeId);
        assignmentObject->setProperty("target", target);
        guideAssignments.add(assignmentObject);
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
        var stats = AutomationValueEncoder::previewStatsToVar(preview);
        stats.getDynamicObject()->setProperty(
                "audioProcessCount",
                static_cast<int>(context.presentation.previewAudioProcessCount(preview.nodeId)));
        previewStats.add(std::move(stats));
    }

    Array<var> probePreviewStats;
    for (const auto& preview : previewResult.probes) {
        probePreviewStats.add(AutomationValueEncoder::probePreviewStatsToVar(preview));
    }

    root->setProperty("nodes", nodes);
    root->setProperty("edges", edges);
    root->setProperty("probes", probes);
    root->setProperty("guides", guides);
    root->setProperty("guideAssignments", guideAssignments);
    root->setProperty("validationIssues", validationIssues);
    root->setProperty("compileIssues", compileIssues);
    root->setProperty("nodeOrder", nodeOrder);
    root->setProperty("previewStats", previewStats);
    root->setProperty("probePreviewStats", probePreviewStats);

    if (state.expandedNodeId.isNotEmpty()) {
        root->setProperty("expandedNodeControls", inspectNodeControls(state.expandedNodeId, state));
    }

    return root;
}

String NodeCanvasAutomationInspector::exportGraphJson() const { return context.document.toJson(); }

var NodeCanvasAutomationInspector::inspectNodeControls(const String& nodeId,
                                                       const NodeCanvasAutomationPresentation& state) const {
    auto* root = new DynamicObject();
    const Node* node = context.document.graph().findNode(nodeId);
    root->setProperty("nodeId", nodeId);
    root->setProperty("resolved", node != nullptr);

    if (node == nullptr) {
        return root;
    }

    root->setProperty("kind", labelForNodeKind(node->kind));
    root->setProperty("expanded", state.expandedNodeId == nodeId);

    Array<var> parameters;
    for (const auto& parameter : node->parameters) {
        parameters.add(AutomationValueEncoder::parameterToVar(parameter));
    }
    root->setProperty("parameters", parameters);
    if (node->model != nullptr) {
        root->setProperty("model", node->model->writeJSON());
    }
    if (node->editorState.getDynamicObject() != nullptr) {
        root->setProperty("editor", node->editorState);
    }

    if (context.editorHost.isEditing(nodeId)) {
        context.editorHost.appendAutomationState(*root);
    }

    return root;
}

var NodeCanvasAutomationInspector::inspectPointerTargets(const NodeCanvasAutomationPresentation& state) const {
    auto* root = new DynamicObject();
    Array<var> targets;
    targets.add(
            AutomationValueEncoder::pointerTargetToVar("canvas", "canvas", context.canvas.getLocalBounds().toFloat()));
    targets.add(AutomationValueEncoder::pointerTargetToVar(
            "probeRefreshMode",
            "probeRefreshMode",
            state.probeRefreshModeBounds));
    targets.add(AutomationValueEncoder::pointerTargetToVar(
            "guideDock",
            "guideDock",
            state.guideDock.dockBounds));
    targets.add(AutomationValueEncoder::pointerTargetToVar(
            "guideDock.collapse",
            "guideDockCollapse",
            state.guideDock.collapseBounds));
    if (!state.guideDock.resizeBounds.isEmpty()) {
        targets.add(AutomationValueEncoder::pointerTargetToVar(
                "guideDock.resize",
                "guideDockResize",
                state.guideDock.resizeBounds));
    }
    if (!state.guideDock.dividerBounds.isEmpty()) {
        targets.add(AutomationValueEncoder::pointerTargetToVar(
                "guideDock.divider",
                "guideDockDivider",
                state.guideDock.dividerBounds));
    }
    if (state.guideDock.expanded) {
        targets.add(AutomationValueEncoder::pointerTargetToVar(
                "guideShelf",
                "guideShelf",
                state.guideDock.guideShelfBounds));
        targets.add(AutomationValueEncoder::pointerTargetToVar(
                "spyShelf",
                "spyShelf",
                state.guideDock.spyShelfBounds));
        if (!state.guideDock.guideMinimizeBounds.isEmpty()) {
            targets.add(AutomationValueEncoder::pointerTargetToVar(
                    "guideShelf.minimize",
                    "guideShelfMinimize",
                    state.guideDock.guideMinimizeBounds));
        }
        if (!state.guideDock.spyMinimizeBounds.isEmpty()) {
            targets.add(AutomationValueEncoder::pointerTargetToVar(
                    "spyShelf.minimize",
                    "spyShelfMinimize",
                    state.guideDock.spyMinimizeBounds));
        }
        if (!state.guideDock.addGuideBounds.isEmpty()) {
            targets.add(AutomationValueEncoder::pointerTargetToVar(
                    "guideShelf.add",
                    "guideShelfAdd",
                    state.guideDock.addGuideBounds));
        }
        if (!state.guideDock.guidesMinimized) {
            for (const auto& tile : state.guideDock.guideTiles) {
                targets.add(AutomationValueEncoder::pointerTargetToVar(
                        "guide:" + tile.guideId,
                        "guide",
                        tile.bounds));
            }
        }
    }
    if (!state.guideDock.guideEditorBounds.isEmpty()) {
        targets.add(AutomationValueEncoder::pointerTargetToVar(
                "guideEditor:" + state.guideDock.expandedGuideId,
                "guideEditor",
                state.guideDock.guideEditorBounds));
    }
    if (state.probeDetailId.isNotEmpty()) {
        targets.add(AutomationValueEncoder::pointerTargetToVar(
                "probeDetail:" + state.probeDetailId,
                "probeDetail",
                state.probeDetailBounds));
        targets.add(AutomationValueEncoder::pointerTargetToVar(
                "probeDetailClose",
                "probeDetailClose",
                SignalProbeDetailView::closeBounds(state.probeDetailBounds)));
    }

    const auto& sceneSnapshot = scene.build(context.document.graph(), context.viewport, context.presentation.revision(),
                                            context.document.revision());

    for (const auto& sceneTarget : sceneSnapshot.targets) {
        if (sceneTarget.kind != NodeSceneTargetKind::Node && !sceneTarget.isPort()) {
            continue;
        }

        targets.add(AutomationValueEncoder::pointerTargetToVar(
                sceneTarget.semanticId,
                sceneTarget.kind == NodeSceneTargetKind::Node
                        ? "node"
                        : (sceneTarget.kind == NodeSceneTargetKind::InputPort ? "inputPort" : "outputPort"),
                sceneTarget.bounds, sceneTarget.nodeId, sceneTarget.portId,
                sceneTarget.kind == NodeSceneTargetKind::InputPort));
    }

    const auto& graphEdges = context.document.graph().getEdges();
    for (const auto& sceneEdge : sceneSnapshot.edges) {
        if (sceneEdge.edgeIndex < 0 || sceneEdge.edgeIndex >= (int) graphEdges.size()) {
            continue;
        }

        const auto& edge = graphEdges[(size_t) sceneEdge.edgeIndex];
        auto* target = new DynamicObject();
        target->setProperty("id", "edge:" + String(sceneEdge.edgeIndex));
        target->setProperty("kind", edge.isAttachment() ? "attachmentEdge" : "edge");
        target->setProperty("edgeIndex", sceneEdge.edgeIndex);
        target->setProperty("bounds", AutomationValueEncoder::rectangleToVar(sceneEdge.hitPath.getBounds()));
        target->setProperty("sourceNodeId", edge.sourceNodeId);
        target->setProperty("sourcePortId", edge.sourcePortId);
        target->setProperty("destNodeId", edge.destNodeId);
        target->setProperty("destPortId", edge.destPortId);
        target->setProperty("modulationBundle", sceneEdge.modulationBundle);
        target->setProperty("bundleEdgeCount", (int) sceneEdge.edgeIndices.size());
        target->setProperty(
                "destinationBundleIncludesYellow",
                sceneEdge.destinationBundleIncludesYellow);
        targets.add(target);
    }

    const Node* expandedNode = context.document.graph().findNode(state.expandedNodeId);
    if (expandedNode != nullptr) {
        AutomationValueEncoder::addExpandedEditorTargets(
                targets,
                *expandedNode,
                state.canvasContentBounds,
                context.editorHost);
    }

    for (auto& targetValue : targets) {
        auto* target = targetValue.getDynamicObject();
        auto* bounds = target != nullptr ? target->getProperty("bounds").getDynamicObject() : nullptr;

        if (bounds == nullptr || !target->getProperty("screenBounds").isVoid()) {
            continue;
        }
        if (!target->getProperty("nativeReady").isVoid()
                && !(bool) target->getProperty("nativeReady")) {
            continue;
        }

        const Rectangle<float> localBounds((float) bounds->getProperty("x"), (float) bounds->getProperty("y"),
                                           (float) bounds->getProperty("width"), (float) bounds->getProperty("height"));
        const Point<float> screenOrigin = context.canvas.localPointToGlobal(localBounds.getPosition());
        target->setProperty("screenBounds",
                            AutomationValueEncoder::rectangleToVar(localBounds.withPosition(screenOrigin)));
    }

    root->setProperty("schema", "cycle-v2-pointer-targets.v1");
    root->setProperty("coordinateSpace", "canvasLocalAndScreen");
    root->setProperty("targetCount", targets.size());
    root->setProperty("targets", targets);
    return root;
}

var NodeCanvasAutomationInspector::captureAudio(size_t frameCount) const {
    auto* root = new DynamicObject();
    const auto& compileResult = context.presentation.compileResult();
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

    const GraphAudioResult result = context.presentation.captureAudio(context.document.graph(), frameCount);
    root->setProperty("metrics", AutomationValueEncoder::audioMetricsToVar(result.output, 44100.0));

    Array<var> samples;
    for (float sample : result.output.block.samples) {
        samples.add(sample);
    }
    root->setProperty("samples", samples);

    Array<var> nodes;
    for (const auto& node : result.nodes) {
        auto* object = new DynamicObject();
        object->setProperty("nodeId", node.nodeId);
        object->setProperty("metrics", AutomationValueEncoder::audioMetricsToVar(node.output, 44100.0));
        object->setProperty("outputCount", (int) node.outputs.size());
        nodes.add(object);
    }

    root->setProperty("nodeCount", nodes.size());
    root->setProperty("nodes", nodes);
    return root;
}

}
