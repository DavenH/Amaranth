#include "NodeViewModule.h"

#include "../Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"

#include <functional>

namespace CycleV2 {

using namespace juce;

class NodeViewModuleRegistry::RegisteredModule final : public NodeViewModule {
public:
    using AttachmentResolver = std::function<std::optional<Point<float>>(const Node&, const String&)>;

    RegisteredModule(NodeKind kindToUse, NodeViewCapabilities capabilitiesToUse,
            AttachmentResolver attachmentResolverToUse = {}) :
            kind(kindToUse),
            viewCapabilities(std::move(capabilitiesToUse)),
            attachmentResolver(std::move(attachmentResolverToUse)) {
    }

    std::optional<Point<float>> attachmentWorldCentre(
            const Node& node,
            const String& semanticPortId) const override {
        return attachmentResolver ? attachmentResolver(node, semanticPortId) : std::nullopt;
    }

    const NodeViewCapabilities& capabilities() const override { return viewCapabilities; }
    const NodeEditorFactory* editorFactory() const override {
        return viewCapabilities.hostedEditor
                ? NodeEditorFactoryRegistry::instance().find(kind)
                : nullptr;
    }

    Rectangle<float> expandedEditorBounds(
            Rectangle<float> componentBounds,
            float minimumMargin) const override {
        const Rectangle<float> available = componentBounds.reduced(minimumMargin);
        Point<float> size {
            jmax(420.f, componentBounds.getWidth() * 0.90f),
            jmax(300.f, componentBounds.getHeight() * 0.90f)
        };
        if (viewCapabilities.expandedEditorScale.has_value()) {
            size = {
                    componentBounds.getWidth() * viewCapabilities.expandedEditorScale->x,
                    componentBounds.getHeight() * viewCapabilities.expandedEditorScale->y
            };
        }
        if (viewCapabilities.expandedEditorSize.has_value()) {
            size = *viewCapabilities.expandedEditorSize;
        }
        return Rectangle<float>(
                jmin(available.getWidth(), size.x),
                jmin(available.getHeight(), size.y)).withCentre(available.getCentre());
    }

    NodeKind kind;

private:
    NodeViewCapabilities viewCapabilities;
    AttachmentResolver attachmentResolver;
};

NodeViewModuleRegistry::NodeViewModuleRegistry() {
    genericModule = std::make_unique<RegisteredModule>(NodeKind::GenericProcessor, NodeViewCapabilities {});
    const auto add = [this](NodeKind kind, NodeViewCapabilities capabilities,
            RegisteredModule::AttachmentResolver attachmentResolver = {}) {
        modules.push_back(std::make_unique<RegisteredModule>(
                kind, std::move(capabilities), std::move(attachmentResolver)));
    };

    NodeViewCapabilities preview;
    preview.previewable = true;
    add(NodeKind::WaveSource, preview);
    add(NodeKind::ImageSource, preview);

    NodeViewCapabilities voice = preview;
    voice.expandedEditorBlocksCanvas = false;
    voice.expandedEditorSize = Point<float>(334.f, 208.f);
    add(NodeKind::VoiceContext, voice);

    NodeViewCapabilities transform = preview;
    transform.expandedEditorBlocksCanvas = false;
    transform.expandedEditorSize = Point<float>(360.f, 144.f);
    add(NodeKind::Fft, transform);
    add(NodeKind::Ifft, transform);

    NodeViewCapabilities mesh = preview;
    mesh.hostedEditor = true;
    mesh.outputSideControl = true;
    mesh.expandedEditorScale = Point<float>(0.81f, 1.f);
    add(NodeKind::TrilinearMesh, mesh, [](const Node& node, const String& portId) {
        const auto target = TrimeshGuideAttachmentTarget::parse(portId);
        if (!target.isValid()) {
            return std::optional<Point<float>> {};
        }
        return std::optional<Point<float>> {{
                node.bounds.getX() + node.bounds.getWidth() * ((float) target.fieldIndex() + 1.f)
                        / ((float) TrimeshGuideAttachmentTarget::fieldCount + 1.f),
                node.bounds.getY()
        }};
    });

    NodeViewCapabilities operation;
    operation.operationLayoutControl = true;
    add(NodeKind::Add, operation);
    add(NodeKind::Multiply, operation);

    const auto addCurve = [&add](NodeKind kind, Point<float> editorSize) {
        NodeViewCapabilities curve;
        curve.previewable = true;
        curve.hostedEditor = true;
        curve.expandedEditorSize = editorSize;
        add(kind, curve);
    };
    addCurve(NodeKind::Envelope, { 840.f, 620.f });
    addCurve(NodeKind::GuideCurve, { 700.f, 380.f });
    addCurve(NodeKind::ImpulseResponse, { 1050.f, 470.f });
    addCurve(NodeKind::Waveshaper, { 540.f, 360.f });

    NodeViewCapabilities effect;
    effect.previewable = true;
    effect.hostedEditor = true;
    effect.expandedEditorSize = Point<float>(520.f, 520.f);
    add(NodeKind::Reverb, effect);

    effect.expandedEditorSize = Point<float>(520.f, 520.f);
    add(NodeKind::Delay, effect);

    effect.expandedEditorSize = Point<float>(620.f, 460.f);
    add(NodeKind::Equalizer, effect);
}

const NodeViewModuleRegistry& NodeViewModuleRegistry::instance() {
    static const NodeViewModuleRegistry registry;
    return registry;
}

const NodeViewModule& NodeViewModuleRegistry::moduleFor(NodeKind kind) const {
    for (const auto& module : modules) {
        if (module->kind == kind) {
            return *module;
        }
    }
    return *genericModule;
}

}
