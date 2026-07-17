#include "NodeEditorHost.h"

namespace CycleV2 {

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
