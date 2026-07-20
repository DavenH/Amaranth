#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

#include "../../Graph/NodeGraph.h"
#include "CurvePanelAdapterTypes.h"
#include "../Trimesh/TrimeshNodeModel.h"

namespace CycleV2 {

class CurvePanelControllerDelegate {
public:
    virtual ~CurvePanelControllerDelegate() = default;
    virtual void repaintCurvePanelController() = 0;
    virtual void setCurvePanelControllerCursor(const MouseCursor& cursor) = 0;
    virtual void beginCurvePanelControllerEdit() = 0;
    virtual void curvePanelControllerEdited() = 0;
    virtual void commitCurvePanelControllerEdit() = 0;
};

class CurvePanelController {
public:
    virtual ~CurvePanelController() = default;

    virtual void syncFromNode(const Node& node) = 0;
    virtual Component* panelHostComponent() = 0;
    virtual Component* panelHostComponentIfCreated() = 0;
    virtual void setDelegate(CurvePanelControllerDelegate* delegate) = 0;
    virtual void setControlValues(
            bool enabled, float first, float second, float third, int menuId) = 0;
    virtual void render(
            Rectangle<float> bounds,
            Rectangle<float> clipBounds,
            float scaleFactor) = 0;
    virtual void renderPreview(Rectangle<float> bounds, float scaleFactor) = 0;
    virtual bool paintExpandedSnapshot(Graphics& graphics, Rectangle<float> bounds) const = 0;
    virtual bool paintPreviewSnapshot(Graphics& graphics, Rectangle<float> bounds) const = 0;
    virtual void releaseSharedGlResources() = 0;
    virtual int vertexCountForAutomation() const = 0;
    virtual var automationState() const = 0;
    virtual std::vector<CurvePreviewVertex> previewVertices() = 0;
    virtual String serializedMeshState() = 0;
    virtual String serializedModelSnapshot() = 0;
    virtual String prepareModelPublication(uint64_t currentRevision) = 0;
    virtual uint64_t modelRevision() const = 0;
    virtual uint64_t contentRevision() const = 0;
    virtual std::vector<TrimeshVertexParameter> selectedVertexParameters() const = 0;
    virtual bool setSelectedVertexParameter(const String& parameterId, float value) = 0;
};

class EnvelopeCurvePanelController {
public:
    virtual ~EnvelopeCurvePanelController() = default;
    virtual void setLogarithmic(bool logarithmic) = 0;
    virtual void setAxisLinks(bool redLinked, bool blueLinked) = 0;
    virtual bool selectedMarkerState(bool loopMarker) const = 0;
    virtual void toggleSelectedMarker(bool loopMarker) = 0;
};

std::unique_ptr<CurvePanelController> createCurvePanelController(NodeKind kind);

}
