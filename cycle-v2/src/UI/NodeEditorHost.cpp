#include "NodeEditorHost.h"

#include "../Runtime/FingerprintBuilder.h"

namespace CycleV2 {

namespace {

uint64_t bindingFingerprint(const Node& node) {
    FingerprintBuilder fingerprint;
    fingerprint.add(node.subtitle);
    for (const auto& parameter : node.parameters) {
        fingerprint.add(parameter.id).add(parameter.value);
    }
    if (node.model != nullptr) {
        fingerprint
                .add(node.model->schemaId())
                .add(static_cast<uint64_t>(node.model->schemaVersion()))
                .add(node.model->revision())
                .add(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(node.model.get())));
    }
    fingerprint.add(JSON::toString(node.editorState, true));
    return fingerprint.value();
}

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

    const uint64_t nodeFingerprint = bindingFingerprint(*node);
    if (created
            || boundDocumentRevision != documentRevision
            || boundNodeFingerprint != nodeFingerprint) {
        editor->bind(*node);
        boundDocumentRevision = documentRevision;
        boundNodeFingerprint = nodeFingerprint;
    }
    editor->component().setBounds(bounds);
    editor->component().setVisible(true);
    editor->component().toFront(false);
    return true;
}

bool NodeEditorHost::rebindTransient(const Node& node) {
    if (editor == nullptr || activeNodeId != node.id || activeKind != node.kind) {
        return false;
    }
    editor->bind(node);
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
    boundNodeFingerprint = 0;
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
