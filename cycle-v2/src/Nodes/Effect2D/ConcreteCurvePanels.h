#pragma once

#include "../../Graph/NodeGraph.h"
#include "../Trimesh/TrimeshNodeModel.h"

#include <JuceHeader.h>

#include <memory>
#include <vector>

class EnvelopeMesh;
class Mesh;
class Panel;
class SingletonRepo;
class VertCube;
class Vertex;

namespace CycleV2 {

class TrimeshPanelEnvironment;

class CurvePanel {
public:
    virtual ~CurvePanel() = default;

    virtual Panel& hostedPanel() = 0;
    virtual void initWithHost(Component* hostComponent) = 0;
    virtual void clearInteractionState() = 0;
    virtual void setControlValues(
            bool enabled, float first, float second, float third, int menuId) = 0;
    virtual void refreshRasterizer() = 0;
    virtual void updateZoomBounds(bool resetView) = 0;
    virtual var automationState() const = 0;
    virtual std::vector<TrimeshVertexParameter> selectedVertexParameters() const = 0;
    virtual bool setSelectedVertexParameter(const String& parameterId, float normalizedValue) = 0;
};

class FlatCurvePanelContract : public CurvePanel {
public:
    virtual Vertex* selectedFlatVertexForModel() = 0;
    virtual void restoreFlatSelection(Vertex* vertex) = 0;
};

class EnvelopeCurvePanelContract : public CurvePanel {
public:
    virtual VertCube* selectedEnvelopeCubeForModel() = 0;
    virtual void restoreEnvelopeSelection(VertCube* cube) = 0;
    virtual void setEnvelopeLogarithmic(bool logarithmic) = 0;
    virtual void setEnvelopeAxisLinks(bool redLinked, bool blueLinked) = 0;
    virtual bool selectedEnvelopeMarkerState(bool loopMarker) const = 0;
    virtual void toggleSelectedEnvelopeMarker(bool loopMarker) = 0;
};

std::unique_ptr<FlatCurvePanelContract> createFlatCurvePanel(
        NodeKind kind,
        SingletonRepo* repo,
        Mesh& mesh);
std::unique_ptr<EnvelopeCurvePanelContract> createEnvelopeCurvePanel(
        SingletonRepo* repo,
        TrimeshPanelEnvironment& environment,
        EnvelopeMesh& mesh);

}
