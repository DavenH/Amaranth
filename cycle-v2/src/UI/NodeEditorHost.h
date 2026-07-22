#pragma once

#include <JuceHeader.h>

#include <memory>

#include "../Graph/GraphEditor.h"
#include "../Graph/GraphCommandDispatcher.h"
#include "../Graph/GraphDocument.h"
#include "../Graph/NodeGraph.h"
#include "../Nodes/Trimesh/TrimeshRenderProfile.h"
#include "../Runtime/NodeUpdateGraph.h"

namespace CycleV2 {

class Effect2DWidget;
class TrimeshWidget;

class NodeEditorCommands {
public:
    virtual ~NodeEditorCommands() = default;

    virtual bool publishCurveState(
            const String& nodeId,
            NodeModelStatePtr model,
            const std::vector<NodeParameter>& controls) = 0;
    virtual bool beginNodeParameterEdit(
            const String& nodeId,
            const String& parameterId,
            const String& label,
            float value) { return false; }
    virtual bool updateNodeParameterEditValue(float value) { return false; }
    virtual bool beginNodeParameterPairEdit(
            const String& nodeId,
            const String& firstParameterId,
            const String& firstLabel,
            float firstValue,
            const String& secondParameterId,
            const String& secondLabel,
            float secondValue) { return false; }
    virtual bool updateNodeParameterPairEditValues(float firstValue, float secondValue) {
        return false;
    }
    virtual void endNodeParameterEdit() {}
    virtual bool setNodeParameterValue(
            const String& nodeId,
            const String& parameterId,
            const String& label,
            float value) { return false; }
    virtual void beginCurveTransaction() = 0;
    virtual void commitCurveTransaction() = 0;
    virtual bool setTrimeshPrimaryAxisValue(const String& nodeId, const String& axis) = 0;
    virtual bool toggleTrimeshLinkAxisValue(const String& nodeId, const String& axis) = 0;
    virtual bool beginTrimeshMorphEdit(
            const String& nodeId,
            const String& parameterId,
            float value) = 0;
    virtual bool updateTrimeshMorphEditValue(float value) = 0;
    virtual void endTrimeshMorphEdit() = 0;
    virtual bool beginTrimeshVertexParameterEdit(
            const String& nodeId,
            const String& parameterId,
            float value) = 0;
    virtual bool updateTrimeshVertexParameterEditValue(float value) = 0;
    virtual void endTrimeshVertexParameterEdit() = 0;
    virtual bool showTrimeshGuideAttachmentMenu(
            const String& nodeId,
            const String& parameterField,
            Rectangle<int> targetScreenArea) = 0;
    virtual bool selectTrimeshVertexIndex(const String& nodeId, int vertexIndex) = 0;
    virtual void persistTrimeshMeshEdits(
            const String& nodeId,
            bool gestureComplete) = 0;
};

class NodeEditorPresentation {
public:
    virtual ~NodeEditorPresentation() = default;

    virtual void closeNodeEditor() = 0;
    virtual void repaintNodeEditor(bool openGl) = 0;
    virtual void selectEditedNode(const String& nodeId) = 0;
    virtual void setNodeEditorStatus(const String& message) = 0;
    virtual void scheduleNodeEditorRefresh() = 0;
    virtual void flushNodeEditorRefresh() = 0;
    virtual void refreshNodeEditorPresentation() = 0;
    virtual Point<float> nodeEditorCreationPosition() const = 0;
    virtual void rebindNodeEditor() = 0;
    virtual void rebindNodeEditorTransient() {
        rebindNodeEditor();
    }
    virtual ProbeRefreshMode probeRefreshMode() const {
        return ProbeRefreshMode::OnGestureCommit;
    }
    virtual void recordNodeEditorMovement(
            const String&,
            const String&,
            uint64_t) {
    }
    virtual void commitNodeEditorLocalState(
            const String&,
            const String&,
            uint64_t,
            uint64_t) {
    }
};

class NodeEditorResources {
public:
    virtual ~NodeEditorResources() = default;

    virtual Effect2DWidget* effect2DWidget(const Node& node) = 0;
    virtual TrimeshWidget* trimeshWidget(const Node& node) = 0;
    virtual TrimeshRenderProfile trimeshRenderProfile(const Node& node) const = 0;
    virtual std::array<String, 6> trimeshGuideLabels(const Node& node) = 0;
    virtual void paintNodePreview(
            Graphics& graphics,
            const Node& node,
            Rectangle<float> bounds) = 0;
};

struct NodeEditorContext {
    String nodeId;
    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
};

class NodeEditor {
public:
    virtual ~NodeEditor() = default;

    virtual Component& component() = 0;
    virtual void bind(const Node& node) = 0;
    virtual void renderOpenGL(float scaleFactor) = 0;
    virtual void appendAutomationState(DynamicObject& state) const = 0;
    virtual Rectangle<float> panelBoundsForAutomation() const = 0;
    virtual void releaseOpenGLResources() = 0;
};

class NodeEditorFactory {
public:
    virtual ~NodeEditorFactory() = default;
    virtual std::unique_ptr<NodeEditor> create(
            const Node& node,
            const NodeEditorContext& context) const = 0;
};

class NodeEditorFactoryProvider {
public:
    virtual ~NodeEditorFactoryProvider() = default;
    virtual const NodeEditorFactory* find(NodeKind kind) const = 0;
};

class NodeEditorFactoryRegistry final : public NodeEditorFactoryProvider {
public:
    static const NodeEditorFactoryRegistry& instance();
    const NodeEditorFactory* find(NodeKind kind) const override;

private:
    NodeEditorFactoryRegistry();

    std::vector<std::pair<NodeKind, std::unique_ptr<NodeEditorFactory>>> factories;
};

class NodeEditorCommandService final : public NodeEditorCommands {
public:
    NodeEditorCommandService(
            Component& owner,
            GraphDocument& document,
            GraphCommandDispatcher& commands,
            NodeEditorPresentation& presentation,
            NodeEditorResources& resources);

    bool publishCurveState(
            const String& nodeId,
            NodeModelStatePtr model,
            const std::vector<NodeParameter>& controls) override;
    bool beginNodeParameterEdit(
            const String& nodeId,
            const String& parameterId,
            const String& label,
            float value) override;
    bool updateNodeParameterEditValue(float value) override;
    bool beginNodeParameterPairEdit(
            const String& nodeId,
            const String& firstParameterId,
            const String& firstLabel,
            float firstValue,
            const String& secondParameterId,
            const String& secondLabel,
            float secondValue) override;
    bool updateNodeParameterPairEditValues(float firstValue, float secondValue) override;
    void endNodeParameterEdit() override;
    bool setNodeParameterValue(
            const String& nodeId,
            const String& parameterId,
            const String& label,
            float value) override;
    void beginCurveTransaction() override;
    void commitCurveTransaction() override;
    bool setTrimeshPrimaryAxisValue(const String& nodeId, const String& axis) override;
    bool toggleTrimeshLinkAxisValue(const String& nodeId, const String& axis) override;
    bool beginTrimeshMorphEdit(
            const String& nodeId,
            const String& parameterId,
            float value) override;
    bool updateTrimeshMorphEditValue(float value) override;
    void endTrimeshMorphEdit() override;
    bool beginTrimeshVertexParameterEdit(
            const String& nodeId,
            const String& parameterId,
            float value) override;
    bool updateTrimeshVertexParameterEditValue(float value) override;
    void endTrimeshVertexParameterEdit() override;
    bool showTrimeshGuideAttachmentMenu(
            const String& nodeId,
            const String& parameterField,
            Rectangle<int> targetScreenArea) override;
    bool selectTrimeshVertexIndex(const String& nodeId, int vertexIndex) override;
    void persistTrimeshMeshEdits(const String& nodeId, bool gestureComplete) override;

private:
    const Node* findNode(const String& nodeId) const;

    Component::SafePointer<Component> owner;
    GraphDocument& document;
    GraphCommandDispatcher& commands;
    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
    String activeMorphNodeId;
    String activeMorphParameterId;
    uint64_t activeMorphFingerprint {};
    bool activeMorphChanged {};
    String activeVertexNodeId;
    String activeVertexParameterId;
    TrimeshWidget* activeVertexWidget {};
    int activeVertexIndex { -1 };
    String activeMeshNodeId;
    bool activeMeshChanged {};
    bool curveTransactionActive {};
    bool curvePublicationPending {};
    String curvePublicationNodeId;
    uint64_t curvePublicationFingerprint {};
    String activeParameterNodeId;
    String activeParameterId;
    String activeParameterLabel;
    String secondaryParameterId;
    String secondaryParameterLabel;
    String activeParameterField;
    uint64_t activeParameterFingerprint {};
    bool activeParameterChanged {};
};

class NodeEditorHost {
public:
    NodeEditorHost(
            Component& parent,
            NodeEditorCommands& commands,
            NodeEditorPresentation& presentation,
            NodeEditorResources& resources,
            const NodeEditorFactoryProvider& factories = NodeEditorFactoryRegistry::instance());
    ~NodeEditorHost();

    bool bind(const Node* node, Rectangle<int> bounds, uint64_t documentRevision = 0);
    bool rebindTransient(const Node& node);
    void close();
    void detach();
    void renderOpenGL(float scaleFactor);
    bool hasEditor() const { return editor != nullptr; }
    bool isEditing(const String& nodeId) const;
    const String& nodeId() const { return activeNodeId; }
    Component* component() const;
    void appendAutomationState(DynamicObject& state) const;
    Rectangle<float> panelBoundsForAutomation() const;

private:
    Component& parent;
    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
    const NodeEditorFactoryProvider& factories;
    std::unique_ptr<NodeEditor> editor;
    String activeNodeId;
    NodeKind activeKind { NodeKind::GenericProcessor };
    uint64_t boundDocumentRevision {};
};

}
