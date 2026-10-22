#include "UI/NodeCanvas.h"

#include "App/GuideCurveAutomation.h"

namespace CycleV2 {

var NodeCanvas::guideCurveCommandForAutomation(const var& command) {
    const bool inspecting = command["command"].toString() == "inspectGuideCurve";
    if (!inspecting) {
        presentation.cancelPendingRefreshes();
    }
    var response = runGuideCurveCommand(document, commands, command);
    if ((bool) response["ok"] && (bool) response["data"]["changed"]) {
        refreshAfterAutomationEdit();
        requestCanvasRepaint();
    }
    return response;
}

}
