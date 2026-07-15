#include "NodeEditorHost.h"

#include "../Nodes/Effect2D/CurveNodeEditors.h"
#include "../Nodes/Effect2D/CurveNodeModels.h"
#include "../Nodes/Effect2D/Effect2DWidget.h"
#include "../Nodes/Trimesh/TrimeshExpandedEditorComponent.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentMenu.h"
#include "../Nodes/Trimesh/TrimeshMeshEditState.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"

namespace CycleV2 {

namespace {

String parameterValue(const Node& node, const String& parameterId, const String& fallback = {}) {
    for (const auto& parameter : node.parameters) {
        if (parameter.id == parameterId) {
            return parameter.value;
        }
    }
    return fallback;
}

class CurveNodeEditor final : public NodeEditor {
public:
    CurveNodeEditor(
            const Node& node,
            const NodeEditorContext& context) :
            commands     (context.commands)
        ,   presentation (context.presentation)
        ,   editor       (createCurveNodeEditor(node.kind, *context.resources.effect2DWidget(node))) {
        Effect2DExpandedEditorComponent::Callbacks callbacks;
        callbacks.close = [this] { presentation.closeNodeEditor(); };
        callbacks.repaintOpenGL = [this] { presentation.repaintNodeEditor(true); };
        callbacks.publishState = [this](
                const String& snapshot,
                uint64_t revision,
                const std::vector<NodeParameter>& controls) {
            return commands.publishCurveState(nodeId, snapshot, revision, controls);
        };
        callbacks.beginTransaction = [this] { commands.beginCurveTransaction(); };
        callbacks.commitTransaction = [this] { commands.commitCurveTransaction(); };
        editor->setCallbacks(std::move(callbacks));
    }

    Component& component() override { return *editor; }

    void bind(const Node& node) override {
        nodeId = node.id;
        editor->setNode(node);
    }

    void renderOpenGL(float scaleFactor) override { editor->renderOpenGL(scaleFactor); }
    void appendAutomationState(DynamicObject& state) const override {
        state.setProperty("effect2D", editor->automationState());
    }
    Rectangle<float> panelBoundsForAutomation() const override {
        return editor->panelBoundsForAutomation();
    }
    void releaseOpenGLResources() override {}

private:
    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    std::unique_ptr<Effect2DExpandedEditorComponent> editor;
    String nodeId;
};

class CurveNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node& node,
            const NodeEditorContext& context) const override {
        return std::make_unique<CurveNodeEditor>(node, context);
    }
};

class TrimeshNodeEditor final : public NodeEditor {
public:
    TrimeshNodeEditor(
            const Node& node,
            const NodeEditorContext& context) :
            commands     (context.commands)
        ,   presentation (context.presentation)
        ,   resources    (context.resources)
        ,   editor       (std::make_unique<TrimeshExpandedEditorComponent>(
                    *context.resources.trimeshWidget(node))) {
        TrimeshExpandedEditorComponent::Callbacks callbacks;
        callbacks.close = [this] { presentation.closeNodeEditor(); };
        callbacks.repaintOpenGL = [this] { presentation.repaintNodeEditor(true); };
        callbacks.setPrimaryAxis = [this](const String& axis) {
            commands.setTrimeshPrimaryAxisValue(nodeId, axis);
        };
        callbacks.toggleLinkAxis = [this](const String& axis) {
            commands.toggleTrimeshLinkAxisValue(nodeId, axis);
        };
        callbacks.beginMorphEdit = [this](const String& id, float value) {
            commands.beginTrimeshMorphEdit(nodeId, id, value);
        };
        callbacks.updateMorphEdit = [this](float value) { commands.updateTrimeshMorphEditValue(value); };
        callbacks.endMorphEdit = [this] { commands.endTrimeshMorphEdit(); };
        callbacks.beginVertexParameterEdit = [this](const String& id, float value) {
            commands.beginTrimeshVertexParameterEdit(nodeId, id, value);
        };
        callbacks.updateVertexParameterEdit = [this](float value) {
            commands.updateTrimeshVertexParameterEditValue(value);
        };
        callbacks.endVertexParameterEdit = [this] { commands.endTrimeshVertexParameterEdit(); };
        callbacks.showVertexGuideAttachmentMenu = [this](const String& field, Rectangle<int> area) {
            commands.showTrimeshGuideAttachmentMenu(nodeId, field, area);
        };
        callbacks.selectVertex = [this](int index) { commands.selectTrimeshVertexIndex(nodeId, index); };
        editor->setCallbacks(std::move(callbacks));
    }

    Component& component() override { return *editor; }

    void bind(const Node& node) override {
        nodeId = node.id;
        auto* widget = resources.trimeshWidget(node);
        jassert(widget != nullptr);
        boundNode = node;
        boundWidget = widget;
        widget->setMeshEditedCallback([this] { commands.persistTrimeshMeshEdits(nodeId); });
        editor->setRenderProfile(resources.trimeshRenderProfile(node));
        editor->setGuideAttachmentLabels(resources.trimeshGuideLabels(node));
        editor->setNode(node);
    }

    void renderOpenGL(float scaleFactor) override { editor->renderOpenGL(scaleFactor); }
    void appendAutomationState(DynamicObject& state) const override {
        if (boundWidget == nullptr) {
            return;
        }
        Array<var> morphSliders;
        Array<var> primaryAxisButtons;
        Array<var> linkToggles;
        for (const auto& axis : { String("yellow"), String("red"), String("blue") }) {
            auto* slider = new DynamicObject();
            slider->setProperty("id", axis);
            slider->setProperty("value", parameterValue(boundNode, axis, "0.5").getDoubleValue());
            slider->setProperty("minimum", 0.0);
            slider->setProperty("maximum", 1.0);
            morphSliders.add(slider);

            auto* primary = new DynamicObject();
            primary->setProperty("id", axis);
            primary->setProperty("selected", parameterValue(boundNode, "primaryAxis", "yellow") == axis);
            primaryAxisButtons.add(primary);

            auto* link = new DynamicObject();
            const String defaultValue = axis == "yellow" ? "1" : "0";
            link->setProperty("id", axis);
            link->setProperty(
                    "selected",
                    parameterValue(boundNode, "link." + axis, defaultValue).getIntValue() != 0);
            linkToggles.add(link);
        }
        state.setProperty("morphSliders", morphSliders);
        state.setProperty("primaryAxisButtons", primaryAxisButtons);
        state.setProperty("linkToggles", linkToggles);

        auto* meshState = new DynamicObject();
        const TrimeshMeshEditState edits = boundWidget->currentMeshEditState();
        int vertexCount = 0;
        for (const auto& edit : edits.getVertexEdits()) {
            vertexCount = jmax(vertexCount, edit.vertexIndex + 1);
        }
        const int selectedVertexIndex = boundWidget->selectedVertexIndexForPanel();
        meshState->setProperty("vertexCount", vertexCount);
        meshState->setProperty("selectedVertexIndex", selectedVertexIndex);
        Array<var> selectedParameters;
        for (const auto& parameter : boundWidget->vertexParametersForIndex(selectedVertexIndex)) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("id", parameter.id);
            encoded->setProperty("value", parameter.value);
            selectedParameters.add(encoded);
        }
        meshState->setProperty("selectedVertexParameters", selectedParameters);
        Array<var> vertexMarkers;
        for (const auto& marker : boundWidget->vertexMarkers()) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("index", marker.index);
            encoded->setProperty("phase", marker.phase);
            encoded->setProperty("amp", marker.amp);
            vertexMarkers.add(encoded);
        }
        meshState->setProperty("vertexMarkers", vertexMarkers);
        state.setProperty("trimesh", var(meshState));
    }
    Rectangle<float> panelBoundsForAutomation() const override { return {}; }
    void releaseOpenGLResources() override {}

private:
    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
    std::unique_ptr<TrimeshExpandedEditorComponent> editor;
    String nodeId;
    Node boundNode;
    TrimeshWidget* boundWidget {};
};

class TrimeshNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node& node,
            const NodeEditorContext& context) const override {
        return std::make_unique<TrimeshNodeEditor>(node, context);
    }
};

}

const NodeEditorFactoryRegistry& NodeEditorFactoryRegistry::instance() {
    static const NodeEditorFactoryRegistry registry;
    return registry;
}

NodeEditorFactoryRegistry::NodeEditorFactoryRegistry() {
    factories.emplace_back(NodeKind::Envelope, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::GuideCurve, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::ImpulseResponse, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::Waveshaper, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::TrilinearMesh, std::make_unique<TrimeshNodeEditorFactory>());
}

const NodeEditorFactory* NodeEditorFactoryRegistry::find(NodeKind kind) const {
    for (const auto& entry : factories) {
        if (entry.first == kind) {
            return entry.second.get();
        }
    }
    return nullptr;
}

NodeEditorCommandService::NodeEditorCommandService(
        Component& ownerToUse,
        GraphDocument& documentToUse,
        GraphCommandDispatcher& commandsToUse,
        NodeEditorPresentation& presentationToUse,
        NodeEditorResources& resourcesToUse) :
        owner        (&ownerToUse)
    ,   document     (documentToUse)
    ,   commands     (commandsToUse)
    ,   presentation (presentationToUse)
    ,   resources    (resourcesToUse) {
}

const Node* NodeEditorCommandService::findNode(const String& nodeId) const {
    return document.graph().findNode(nodeId);
}

bool NodeEditorCommandService::publishCurveState(
        const String& nodeId,
        const String& snapshot,
        uint64_t revision,
        const std::vector<NodeParameter>& controls) {
    const Node* node = findNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    const auto result = commands.publishCurveState({
            nodeId,
            CurveNodeModelCodec::revisionFromParameters(node->parameters),
            revision,
            snapshot,
            controls
    });
    if (!result.succeeded() || !result.changed) {
        return result.succeeded();
    }
    if (curveTransactionActive) {
        curvePublicationPending = true;
    } else {
        presentation.scheduleNodeEditorRefresh();
    }
    presentation.repaintNodeEditor(true);
    return true;
}

void NodeEditorCommandService::beginCurveTransaction() {
    curveTransactionActive = true;
    curvePublicationPending = false;
    commands.beginCompoundEdit();
}

void NodeEditorCommandService::commitCurveTransaction() {
    commands.commitCompoundEdit();
    curveTransactionActive = false;
    if (!curvePublicationPending) {
        return;
    }
    curvePublicationPending = false;
    presentation.scheduleNodeEditorRefresh();
    presentation.repaintNodeEditor(true);
}

bool NodeEditorCommandService::setTrimeshPrimaryAxisValue(
        const String& nodeId,
        const String& axis) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }
    if (parameterValue(*node, "primaryAxis", "yellow") == axis) {
        return true;
    }
    const auto result = commands.setNodeParameter(nodeId, "primaryAxis", "Primary Axis", axis);
    if (!result.succeeded()) {
        return false;
    }
    presentation.selectEditedNode(nodeId);
    presentation.refreshNodeEditorPresentation();
    presentation.setNodeEditorStatus("Primary axis: " + axis);
    presentation.repaintNodeEditor(true);
    return true;
}

bool NodeEditorCommandService::toggleTrimeshLinkAxisValue(
        const String& nodeId,
        const String& axis) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }
    const String parameterId = "link." + axis;
    const String defaultValue = axis == "yellow" ? "1" : "0";
    const bool enabled = parameterValue(*node, parameterId, defaultValue).getIntValue() != 0;
    const auto result = commands.setNodeParameter(
            nodeId, parameterId, "Link " + axis, enabled ? "0" : "1");
    if (!result.succeeded()) {
        return false;
    }
    presentation.selectEditedNode(nodeId);
    presentation.refreshNodeEditorPresentation();
    presentation.setNodeEditorStatus("Link " + axis + (enabled ? " disabled" : " enabled"));
    presentation.repaintNodeEditor(true);
    return true;
}

bool NodeEditorCommandService::beginTrimeshMorphEdit(
        const String& nodeId,
        const String& parameterId,
        float value) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }
    activeMorphNodeId = nodeId;
    activeMorphParameterId = parameterId;
    presentation.selectEditedNode(nodeId);
    commands.beginCompoundEdit();
    return updateTrimeshMorphEditValue(value);
}

bool NodeEditorCommandService::updateTrimeshMorphEditValue(float value) {
    const Node* node = findNode(activeMorphNodeId);
    if (node == nullptr || activeMorphParameterId.isEmpty()) {
        return false;
    }
    const String label = activeMorphParameterId.substring(0, 1).toUpperCase()
            + activeMorphParameterId.substring(1);
    const auto result = commands.setNodeParameter(
            activeMorphNodeId, activeMorphParameterId, label, String(value, 3));
    if (!result.succeeded()) {
        return false;
    }
    presentation.scheduleNodeEditorRefresh();
    presentation.setNodeEditorStatus("Morph " + label + " = " + String(value, 2));
    presentation.repaintNodeEditor(false);
    return true;
}

void NodeEditorCommandService::endTrimeshMorphEdit() {
    commands.commitCompoundEdit();
    presentation.flushNodeEditorRefresh();
    activeMorphNodeId = {};
    activeMorphParameterId = {};
}

bool NodeEditorCommandService::beginTrimeshVertexParameterEdit(
        const String& nodeId,
        const String& parameterId,
        float value) {
    const Node* node = findNode(nodeId);
    TrimeshWidget* widget = node != nullptr ? resources.trimeshWidget(*node) : nullptr;
    if (node == nullptr || widget == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }
    int vertexIndex = parameterValue(*node, "selectedVertexIndex", "-1").getIntValue();
    commands.beginCompoundEdit();
    if (vertexIndex < 0) {
        vertexIndex = widget->resolvedSelectedVertexIndexForNode(*node);
        if (vertexIndex >= 0) {
            commands.setNodeParameter(nodeId, "selectedVertexIndex", "Selected Vertex", String(vertexIndex));
        }
    }
    if (vertexIndex < 0) {
        commands.cancelCompoundEdit();
        return false;
    }
    activeVertexNodeId = nodeId;
    activeVertexParameterId = parameterId;
    activeVertexIndex = vertexIndex;
    presentation.selectEditedNode(nodeId);
    return updateTrimeshVertexParameterEditValue(value);
}

bool NodeEditorCommandService::updateTrimeshVertexParameterEditValue(float value) {
    const Node* node = findNode(activeVertexNodeId);
    if (node == nullptr || activeVertexParameterId.isEmpty() || activeVertexIndex < 0) {
        return false;
    }
    String label = activeVertexParameterId.fromFirstOccurrenceOf(".", false, false);
    if (label.isEmpty()) {
        label = activeVertexParameterId;
    }
    const String persistentId = TrimeshMeshEditState::canonicalVertexParameterId(
            activeVertexIndex, label);
    const auto result = commands.setNodeParameter(
            activeVertexNodeId, persistentId, label, String(value, 3));
    if (!result.succeeded()) {
        return false;
    }
    presentation.scheduleNodeEditorRefresh();
    presentation.setNodeEditorStatus(
            "Vertex #" + String(activeVertexIndex) + " " + label + " = " + String(value, 2));
    return true;
}

void NodeEditorCommandService::endTrimeshVertexParameterEdit() {
    commands.commitCompoundEdit();
    presentation.flushNodeEditorRefresh();
    activeVertexNodeId = {};
    activeVertexParameterId = {};
    activeVertexIndex = -1;
}

void NodeEditorCommandService::persistTrimeshMeshEdits(const String& nodeId) {
    const Node* node = findNode(nodeId);
    TrimeshWidget* widget = node != nullptr ? resources.trimeshWidget(*node) : nullptr;
    if (node == nullptr || widget == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return;
    }
    commands.beginCompoundEdit();
    const TrimeshMeshEditState editState = widget->currentMeshEditState();
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
    commands.setNodeParameter(
            nodeId,
            "selectedVertexIndex",
            "Selected Vertex",
            String(widget->selectedVertexIndexForPanel()));
    commands.commitCompoundEdit();
    presentation.scheduleNodeEditorRefresh();
    presentation.repaintNodeEditor(false);
}

bool NodeEditorCommandService::showTrimeshGuideAttachmentMenu(
        const String& nodeId,
        const String& parameterField,
        Rectangle<int> targetScreenArea) {
    const Node* node = findNode(nodeId);
    TrimeshWidget* widget = node != nullptr ? resources.trimeshWidget(*node) : nullptr;
    if (node == nullptr || widget == nullptr || owner == nullptr) {
        return false;
    }
    const int vertexIndex = widget->resolvedSelectedVertexIndexForNode(*node);
    const auto items = TrimeshGuideAttachmentMenu::itemsFor(
            document.graph(), nodeId, vertexIndex, parameterField);
    PopupMenu menu;
    for (const auto& item : items) {
        menu.addItem(item.menuId, item.label, true, item.attached);
    }
    auto safeOwner = owner;
    menu.showMenuAsync(
            PopupMenu::Options().withTargetScreenArea(targetScreenArea),
            [this, safeOwner, nodeId, vertexIndex, parameterField, items](int menuId) {
                if (safeOwner == nullptr || menuId == 0) {
                    return;
                }
                GraphEditResult result { GraphEditCode::ValidationRejected, {}, {} };
                if (menuId == TrimeshGuideAttachmentMenu::newGuideMenuId) {
                    result = commands.createAndAttachGuideCurve(
                            nodeId, vertexIndex, parameterField,
                            presentation.nodeEditorCreationPosition());
                } else {
                    for (const auto& item : items) {
                        if (item.menuId == menuId) {
                            result = commands.attachGuideCurve(
                                    item.guideNodeId, nodeId, vertexIndex, parameterField);
                            break;
                        }
                    }
                }
                if (!result.succeeded()) {
                    presentation.setNodeEditorStatus("Guide attachment failed");
                    presentation.repaintNodeEditor(false);
                    return;
                }
                presentation.selectEditedNode(result.nodeId.isEmpty() ? nodeId : result.nodeId);
                presentation.refreshNodeEditorPresentation();
                presentation.rebindNodeEditor();
                presentation.setNodeEditorStatus("Guide " + parameterField + " attached");
                presentation.repaintNodeEditor(false);
            });
    return true;
}

bool NodeEditorCommandService::selectTrimeshVertexIndex(
        const String& nodeId,
        int vertexIndex) {
    const Node* node = findNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    if (parameterValue(*node, "selectedVertexIndex", "-1").getIntValue() == vertexIndex) {
        return true;
    }
    const auto result = commands.setNodeParameter(
            nodeId, "selectedVertexIndex", "Selected Vertex", String(vertexIndex));
    if (!result.succeeded()) {
        return false;
    }
    presentation.refreshNodeEditorPresentation();
    presentation.selectEditedNode(nodeId);
    presentation.setNodeEditorStatus("Selected vertex #" + String(vertexIndex));
    return true;
}

NodeEditorHost::NodeEditorHost(
        Component& parentToUse,
        NodeEditorCommands& commandsToUse,
        NodeEditorPresentation& presentationToUse,
        NodeEditorResources& resourcesToUse,
        const NodeEditorFactoryProvider& factoriesToUse) :
        parent       (parentToUse)
    ,   commands     (commandsToUse)
    ,   presentation (presentationToUse)
    ,   resources    (resourcesToUse)
    ,   factories    (factoriesToUse) {
}

NodeEditorHost::~NodeEditorHost() {
    detach();
}

bool NodeEditorHost::bind(const Node* node, Rectangle<int> bounds, uint64_t documentRevision) {
    const NodeEditorFactory* factory = node != nullptr
            ? factories.find(node->kind)
            : nullptr;
    if (node == nullptr || factory == nullptr) {
        close();
        return false;
    }

    bool created = false;
    if (editor == nullptr || activeNodeId != node->id || activeKind != node->kind) {
        close();
        NodeEditorContext context { node->id, commands, presentation, resources };
        editor = factory->create(*node, context);
        if (editor == nullptr) {
            return false;
        }
        activeNodeId = node->id;
        activeKind = node->kind;
        parent.addAndMakeVisible(editor->component());
        created = true;
    }

    if (created || boundDocumentRevision != documentRevision) {
        editor->bind(*node);
        boundDocumentRevision = documentRevision;
    }
    editor->component().setBounds(bounds);
    editor->component().setVisible(true);
    editor->component().toFront(false);
    return true;
}

void NodeEditorHost::close() {
    if (editor == nullptr) {
        activeNodeId = {};
        return;
    }
    editor->releaseOpenGLResources();
    if (editor->component().getParentComponent() == &parent) {
        parent.removeChildComponent(&editor->component());
    }
    editor.reset();
    activeNodeId = {};
    boundDocumentRevision = 0;
}

void NodeEditorHost::detach() {
    if (editor != nullptr && editor->component().getParentComponent() == &parent) {
        parent.removeChildComponent(&editor->component());
    }
}

void NodeEditorHost::renderOpenGL(float scaleFactor) {
    if (editor != nullptr && editor->component().isVisible()) {
        editor->renderOpenGL(scaleFactor);
    }
}

bool NodeEditorHost::isEditing(const String& nodeIdToCheck) const {
    return editor != nullptr && activeNodeId == nodeIdToCheck;
}

Component* NodeEditorHost::component() const {
    return editor != nullptr ? &editor->component() : nullptr;
}

void NodeEditorHost::appendAutomationState(DynamicObject& state) const {
    if (editor != nullptr) {
        editor->appendAutomationState(state);
    }
}

Rectangle<float> NodeEditorHost::panelBoundsForAutomation() const {
    return editor != nullptr ? editor->panelBoundsForAutomation() : Rectangle<float>();
}

}
