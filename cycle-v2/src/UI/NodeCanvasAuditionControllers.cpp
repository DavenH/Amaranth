#include "UI/NodeCanvas.h"

namespace CycleV2 {

bool NodeCanvas::setAuditionControllerValue(int controller, int value) {
    if (!isPositiveAndBelow(controller, 128) || !isPositiveAndBelow(value, 128)) {
        return false;
    }
    if (presentation.previewControllerValue(controller) == value) {
        return true;
    }
    if (!presentation.refreshPreviewControllerValue(
                commands.editingGraph(), document.revision(), controller, value)) {
        return false;
    }
    editorCoordinator.previewResources().refreshControllerPreview(controller);
    refreshProbeDetail();
    requestCanvasRepaint();
    if (comparisonRefreshCallback) {
        comparisonRefreshCallback();
    }
    return true;
}

var NodeCanvas::inspectPreviewControllersForAutomation() const {
    auto* result = new DynamicObject();
    Array<var> values;
    for (const int value : presentation.previewControllerValues()) {
        values.add(value);
    }
    result->setProperty("channel", 1);
    result->setProperty("controllers", values);
    result->setProperty("ephemeral", true);
    result->setProperty("documentRevision", (int64) document.revision());
    result->setProperty("audioPlanRevision", (int64) presentation.audioPlanRevision());
    result->setProperty("compilationCount", (int64) presentation.compilationCount());
    return var(result);
}

}
