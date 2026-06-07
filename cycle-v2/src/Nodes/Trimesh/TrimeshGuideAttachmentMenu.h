#pragma once

#include "../../Graph/NodeGraph.h"

#include <vector>

namespace CycleV2 {

struct TrimeshGuideAttachmentMenuItem {
    int menuId {};
    String label;
    String guideNodeId;
    bool createNew {};
    bool attached {};
};

class TrimeshGuideAttachmentMenu {
public:
    static constexpr int newGuideMenuId = 1;
    static constexpr int firstGuideMenuId = 100;

    static std::vector<TrimeshGuideAttachmentMenuItem> itemsFor(
            const NodeGraph& graph,
            const String& meshNodeId,
            int vertexIndex,
            const String& parameterField);

    static String targetPortId(int vertexIndex, const String& parameterField);
};

}
