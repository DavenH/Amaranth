#include "UI/NodeCanvas.h"
#include "UI/NodeWorkspace.h"

#include "App/EnvelopeAuthoringAutomation.h"
#include "App/MeshTopologyAutomation.h"
#include "App/TrimeshAuthoringAutomation.h"
#include "Graph/MeshDimensionality.h"

namespace CycleV2 {

var NodeCanvas::shapeCommandForAutomation(const var& command) {
    const auto name = command["command"].toString();
    if (!name.startsWith("inspect")) {
        presentation.cancelPendingRefreshes();
    }
    var response = name == "editMeshTopology" ? runMeshTopologyCommand(document, commands, command)
            : name.endsWith("Envelope") ? runEnvelopeAuthoringCommand(document, commands, command)
            : runTrimeshAuthoringCommand(document, commands, command);
    if (name.startsWith("inspect") && (bool) response["ok"]) {
        const Node* node = document.graph().findNode(command["nodeId"].toString());
        auto* data = response["data"].getDynamicObject();
        if (node != nullptr && data != nullptr) {
            const auto authored = MeshDimensionality::position(*node);
            const auto effective = presentation.previewMorphForNode(*node).value_or(authored);
            const auto encode = [](const MorphPosition& position) {
                auto* value = new DynamicObject();
                value->setProperty("yellow", (float) position.time);
                value->setProperty("red", (float) position.red);
                value->setProperty("blue", (float) position.blue);
                return var(value);
            };
            data->setProperty("authoredMorph", encode(authored));
            data->setProperty("effectiveMorph", encode(effective));
        }
    }
    if ((bool) response["ok"] && (bool) response["data"]["changed"]) {
        refreshAfterAutomationEdit();
        requestCanvasRepaint();
    }
    return response;
}

var NodeWorkspace::shapeCommandForAutomation(const var& command) {
    return canvas.shapeCommandForAutomation(command);
}

}
