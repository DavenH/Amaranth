#pragma once

#include "../../Graph/NodeGraph.h"
#include "Effect2DPanelBridge.h"

#include <JuceHeader.h>

#include <functional>
#include <memory>

namespace CycleV2 {

class Effect2DWidget {
public:
    explicit Effect2DWidget(NodeKind nodeKind);
    ~Effect2DWidget();

    Component* prepareExpandedPanelComponent(const Node& node, Rectangle<float> contentBounds);
    Component* getExpandedPanelComponentIfCreated();
    void setExpandedPanelCallbacks(
            std::function<void()> repaintCallback,
            std::function<void(const MouseCursor&)> cursorCallback);
    void renderExpandedPanelOpenGL(const Node& node, Rectangle<float> bounds, float scaleFactor);
    void releaseSharedGlResources();

private:
    NodeKind kind;
    Effect2DPanelBridge bridge;
};

}
